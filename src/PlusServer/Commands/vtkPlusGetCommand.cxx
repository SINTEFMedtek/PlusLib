#include "PlusConfigure.h"
#include "vtkPlusGetCommand.h"

#include "vtkPlusDataCollector.h"

vtkStandardNewMacro(vtkPlusGetCommand);

static const char CMD_NAME[] = "Get";

//----------------------------------------------------------------------------
vtkPlusGetCommand::vtkPlusGetCommand()
	: Depth(0)
	, Gain(0)
//	: DeviceId(NULL)
//	, Text(NULL)
//	, ResponseText(NULL)
//	, ResponseExpected(true)
{
	// It handles only one command, set its name by default
	this->SetName(CMD_NAME);
	LOG_DEBUG("vtkPlusGetCommand constr");
}

//----------------------------------------------------------------------------
vtkPlusGetCommand::~vtkPlusGetCommand()
{
}

//----------------------------------------------------------------------------
void vtkPlusGetCommand::GetCommandNames(std::list<std::string>& cmdNames)
{
	cmdNames.clear();
	cmdNames.push_back(CMD_NAME);
}

//----------------------------------------------------------------------------
std::string vtkPlusGetCommand::GetDescription(const std::string& commandName)
{
	std::string desc;
	if (commandName.empty() || PlusCommon::IsEqualInsensitive(commandName, CMD_NAME))
	{
		desc += CMD_NAME;
		desc += ": Send command to the device.";
	}
	return desc;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusGetCommand::ReadConfiguration(vtkXMLDataElement* aConfig)
{
	if (vtkPlusCommand::ReadConfiguration(aConfig) != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	//TODO: Read all possible params
	XML_READ_STRING_ATTRIBUTE_OPTIONAL(DeviceId, aConfig);
//	XML_READ_STRING_ATTRIBUTE_OPTIONAL(Text, aConfig);
	XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, Depth, aConfig);
	XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, Gain, aConfig);
	return PLUS_SUCCESS;
}

PlusStatus vtkPlusGetCommand::Execute()
{
	LOG_DEBUG("vtkPlusGetCommand::Execute: " << (!this->Name.empty() ? this->Name : "(undefined)")
		<< ", device: " << (this->DeviceName.empty() ? "(undefined)" : this->DeviceName));
//		<< ", device: " << (this->DeviceId.empty() ? "(undefined)" : this->DeviceId));
//		<< ", text: " << (this->Text.empty() ? "(undefined)" : this->Text));

	vtkPlusDataCollector* dataCollector = GetDataCollector();
	if (dataCollector == NULL)
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", "Invalid data collector.");
		return PLUS_FAIL;
	}

	// Get device pointer
	if (this->DeviceName.empty())
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", "No DeviceName specified.");
		return PLUS_FAIL;
	}
	vtkPlusDevice* device = NULL;
	if (dataCollector->GetDevice(device, this->DeviceName) != PLUS_SUCCESS)
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", std::string("Device ")
			+ (this->DeviceName.empty() ? "(undefined)" : this->DeviceName) + std::string(" is not found."));
		return PLUS_FAIL;
	}


	std::string response;
	//response << "Got Get command for device: " + this->DeviceName.c_str() << " DeviceId: " << DeviceId.c_str() << " Depth: " << Depth << " Gain: " << Gain;
//	response += std::string("Got Get command for device: ") + this->DeviceName + std::string(" DeviceId: ") + DeviceId + std::string(" Depth: ") + Depth + std::string(" Gain: ") + Gain;
	response += std::string("Got Get command for device: ") + this->DeviceName;
	this->QueueCommandResponse(PLUS_SUCCESS, response);
	return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
std::string vtkPlusGetCommand::GetDeviceId() const
{
	return this->DeviceId;
}

//----------------------------------------------------------------------------
void vtkPlusGetCommand::SetDeviceId(const std::string& deviceId)
{
	this->DeviceId = deviceId;
}

double vtkPlusGetCommand::GetDepth() const
{
	return this->Depth;
}

void vtkPlusGetCommand::SetDepth(const double& depth)
{
	this->Depth = depth;
}

double vtkPlusGetCommand::GetGain() const
{
	return this->Gain;
}

void vtkPlusGetCommand::SetGain(const double& gain)
{
	this->Gain = gain;
}