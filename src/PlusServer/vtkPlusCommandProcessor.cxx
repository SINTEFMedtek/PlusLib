/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtkImageData.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPlusCommand.h"
#include "vtkPlusCommandProcessor.h"
#include "vtkPlusGetImageCommand.h"
#include "vtkPlusReconstructVolumeCommand.h"
#ifdef PLUS_USE_STEALTHLINK
#include "vtkPlusStealthLinkCommand.h"
#endif
#ifdef PLUS_USE_OPTIMET_CONOPROBE
#include "vtkPlusConoProbeLinkCommand.h"
#endif
#include "igtl_header.h"
#include "vtkPlusGetTransformCommand.h"
#include "vtkPlusRecursiveCriticalSection.h"
#include "vtkPlusRequestIdsCommand.h"
#include "vtkPlusSaveConfigCommand.h"
#include "vtkPlusSendTextCommand.h"
#include "vtkPlusStartStopRecordingCommand.h"
#include "vtkPlusUpdateTransformCommand.h"
#include "vtkPlusVersionCommand.h"
#include "vtkXMLUtilities.h"
#include "vtkPlusGetCommand.h"

vtkStandardNewMacro(vtkPlusCommandProcessor);

//----------------------------------------------------------------------------
vtkPlusCommandProcessor::vtkPlusCommandProcessor()
  : PlusServer(NULL)
  , Threader(vtkSmartPointer<vtkMultiThreader>::New())
  , Mutex(vtkSmartPointer<vtkPlusRecursiveCriticalSection>::New())
  , CommandExecutionActive(std::make_pair(false, false))
  , CommandExecutionThreadId(-1)
{
  // Register default commands
  RegisterPlusCommand(vtkSmartPointer<vtkPlusStartStopRecordingCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusReconstructVolumeCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusRequestIdsCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusUpdateTransformCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusGetTransformCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusSaveConfigCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusSendTextCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusVersionCommand>::New());
  RegisterPlusCommand(vtkSmartPointer<vtkPlusGetCommand>::New());
#ifdef PLUS_USE_STEALTHLINK
  RegisterPlusCommand(vtkSmartPointer<vtkPlusStealthLinkCommand>::New());
#endif
#ifdef PLUS_USE_OPTIMET_CONOPROBE
  RegisterPlusCommand(vtkSmartPointer<vtkPlusConoProbeLinkCommand>::New());
#endif
}

//----------------------------------------------------------------------------
vtkPlusCommandProcessor::~vtkPlusCommandProcessor()
{
  SetPlusServer(NULL);
}

//----------------------------------------------------------------------------
void vtkPlusCommandProcessor::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Available Commands : ";
  // TODO: print registered commands
  /*
  if( AvailableCommands )
  {
    AvailableCommands->PrintSelf( os, indent );
  }
  else
  {
    os << "None.";
  }
  */
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::Start()
{
  if (this->CommandExecutionThreadId < 0)
  {
    this->CommandExecutionActive.first = true;
    this->CommandExecutionThreadId = this->Threader->SpawnThread((vtkThreadFunctionType)&CommandExecutionThread, this);
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::Stop()
{
  // Stop the command execution thread
  if (this->CommandExecutionThreadId >= 0)
  {
    this->CommandExecutionActive.first = false;
    while (this->CommandExecutionActive.second)
    {
      // Wait until the thread stops
      vtkPlusAccurateTimer::Delay(0.2);
    }
    this->CommandExecutionThreadId = -1;
  }

  LOG_DEBUG("Command execution thread stopped");

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void* vtkPlusCommandProcessor::CommandExecutionThread(vtkMultiThreader::ThreadInfo* data)
{
  vtkPlusCommandProcessor* self = (vtkPlusCommandProcessor*)(data->UserData);

  self->CommandExecutionActive.second = true;

  // Execute commands until a stop is requested
  while (self->CommandExecutionActive.first)
  {
    self->ExecuteCommands();
    // no commands in the queue, wait a bit before checking again
    const double commandQueuePollIntervalSec = 0.010;
#ifdef _WIN32
    Sleep(commandQueuePollIntervalSec * 1000);
#else
    usleep(commandQueuePollIntervalSec * 1000000);
#endif
  }

  // Close thread
  self->CommandExecutionThreadId = -1;
  self->CommandExecutionActive.second = false;
  return NULL;
}

//----------------------------------------------------------------------------
int vtkPlusCommandProcessor::ExecuteCommands()
{
  // Implemented in a while loop to not block the mutex during command execution, only during management of the queue.
  int numberOfExecutedCommands(0);
  while (1)
  {
    vtkSmartPointer<vtkPlusCommand> cmd; // next command to be processed
    {
      PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
      if (this->CommandQueue.empty())
      {
        return numberOfExecutedCommands;
      }
      cmd = this->CommandQueue.front();
      this->CommandQueue.pop_front();
    }

    LOG_DEBUG("Executing command");
    if (cmd->Execute() != PLUS_SUCCESS)
    {
      LOG_ERROR("Command execution failed");
    }

    // move the response objects from the command to the processor's queue
    {
      PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
      cmd->PopCommandResponses(this->CommandResponseQueue);
    }

    numberOfExecutedCommands++;
  }

  // we never actually reach this point
  return numberOfExecutedCommands;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::RegisterPlusCommand(vtkPlusCommand* cmd)
{
  if (cmd == NULL)
  {
    LOG_ERROR("vtkPlusCommandProcessor::RegisterPlusCommand received an invalid command object");
    return PLUS_FAIL;
  }

  std::list<std::string> cmdNames;
  cmd->GetCommandNames(cmdNames);
  if (cmdNames.empty())
  {
    LOG_ERROR("Cannot register command: command name is empty");
    return PLUS_FAIL;
  }

  for (std::list<std::string>::iterator nameIt = cmdNames.begin(); nameIt != cmdNames.end(); ++nameIt)
  {
    this->RegisteredCommands[*nameIt] = cmd;
    cmd->Register(this);
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
vtkPlusCommand* vtkPlusCommandProcessor::CreatePlusCommand(const std::string& commandName, const std::string& commandStr)
{
  vtkSmartPointer<vtkXMLDataElement> cmdElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromString(commandStr.c_str()));
  if (cmdElement.GetPointer() == NULL)
  {
    LOG_ERROR("failed to parse XML command string (received: " + commandStr + ")");
    return NULL;
  }

  if (STRCASECMP(cmdElement->GetName(), "Command") != 0)
  {
    LOG_ERROR("Command element expected (received: " + commandStr + ")");
    return NULL;
  }

  if (this->RegisteredCommands.find(commandName) == this->RegisteredCommands.end())
  {
    // unregistered command
    LOG_ERROR("Unknown command: " << commandName);
    return NULL;
  }

  vtkPlusCommand* cmd = (this->RegisteredCommands[commandName])->Clone();
  if (cmd->ReadConfiguration(cmdElement) != PLUS_SUCCESS)
  {
    cmd->Delete();
    cmd = NULL;
    LOG_ERROR("Failed to initialize command from string: " + commandStr);
    return NULL;
  }
  return cmd;
}

//------------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::QueueCommand(bool respondUsingIGTLCommand, unsigned int clientId, const std::string& commandName, const std::string& commandString, const std::string& deviceName, uint32_t uid)
{
	LOG_DEBUG("QueueCommand respondUsingIGTLCommand: " << respondUsingIGTLCommand << " clientId: " << clientId << " commandName: " << commandName << " deviceName: " << deviceName << " commandString: " << commandString);
  if (commandString.empty())
  {
    LOG_ERROR("Command string is undefined");
    return PLUS_FAIL;
  }

  if (commandName.empty())
  {
    LOG_ERROR("Command name is undefined");
    return PLUS_FAIL;
  }
  vtkSmartPointer<vtkPlusCommand> cmd;
  if (commandName != "GetDeviceParameters")
	  cmd = vtkSmartPointer<vtkPlusCommand>::Take(CreatePlusCommand(commandName, commandString));
  else
      cmd = vtkSmartPointer<vtkPlusCommand>::Take(CreatePlusCommand("Get", commandString));
  if (cmd.GetPointer() == NULL)
  {
    if (!respondUsingIGTLCommand)
    {
      this->QueueStringResponse(PLUS_FAIL, deviceName, std::string("Error attempting to process command."));
    }
    else
    {
      this->QueueCommandResponse(PLUS_FAIL, deviceName, clientId, commandName, uid, std::string("Error attempting to process command."));
    }
    return PLUS_FAIL;
  }

  cmd->SetCommandProcessor(this);
  cmd->SetClientId(clientId);
  cmd->SetDeviceName(deviceName.c_str());
  cmd->SetId(uid);
  cmd->SetRespondWithCommandMessage(respondUsingIGTLCommand);

  // Add command to the execution queue
  PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
  this->CommandQueue.push_back(cmd);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::QueueStringResponse(const PlusStatus& status, const std::string& deviceName, const std::string& replyString)
{
  vtkSmartPointer<vtkPlusCommandStringResponse> response = vtkSmartPointer<vtkPlusCommandStringResponse>::New();
  response->SetDeviceName(deviceName);
  std::ostringstream replyStr;
  replyStr << "<CommandReply";
  replyStr << " Status=\"" << (status == PLUS_SUCCESS ? "SUCCESS" : "FAIL") << "\"";
  replyStr << " Message=\"";
  // Write to XML, encoding special characters, such as " ' \ < > &
  vtkXMLUtilities::EncodeString(replyString.c_str(), VTK_ENCODING_NONE, replyStr, VTK_ENCODING_NONE, 1 /* encode special characters */);
  replyStr << "\"";
  replyStr << " />";

  response->SetMessage(replyStr.str());
  response->SetStatus(status);

  // Add response to the command response queue
  PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
  this->CommandResponseQueue.push_back(response);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusCommandProcessor::QueueCommandResponse(const PlusStatus& status, const std::string& deviceName, unsigned int clientId, const std::string& commandName, uint32_t uid, const std::string& replyString)
{
  // TODO : determine error string syntax/standard
  std::string errorMessage = commandName + std::string(": failure");
  LOG_ERROR(errorMessage);

  vtkSmartPointer<vtkPlusCommandCommandResponse> response = vtkSmartPointer<vtkPlusCommandCommandResponse>::New();
  response->SetClientId(clientId);
  response->SetDeviceName(deviceName);
  response->SetOriginalId(uid);
  response->SetRespondWithCommandMessage(true);
  response->SetErrorString(errorMessage);
  response->SetStatus(PLUS_FAIL);

  // Add response to the command response queue
  PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
  this->CommandResponseQueue.push_back(response);

  return PLUS_SUCCESS;
}

//------------------------------------------------------------------------------
void vtkPlusCommandProcessor::PopCommandResponses(PlusCommandResponseList& responses)
{
  PlusLockGuard<vtkPlusRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);
  // Add reply to the sending queue
  // Append this->CommandResponses to 'responses'.
  // Elements appended to 'responses' are removed from this->CommandResponses.
  responses.splice(responses.end(), this->CommandResponseQueue, this->CommandResponseQueue.begin(), this->CommandResponseQueue.end());
}

//------------------------------------------------------------------------------
bool vtkPlusCommandProcessor::IsRunning()
{
  return this->CommandExecutionActive.second;
}

