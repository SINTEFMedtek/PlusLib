#include "PlusConfigure.h"
#include "vtkPlusGetCommand.h"

#include "vtkPlusDataCollector.h"
#include "vtkPlusUsCommandDevice.h"

vtkStandardNewMacro(vtkPlusGetCommand);

static const char CMD_NAME[] = "Get";

//----------------------------------------------------------------------------
vtkPlusGetCommand::vtkPlusGetCommand()
	: Depth(0)
	, Gain(0)
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
	XML_READ_STRING_ATTRIBUTE_OPTIONAL(DeviceId, aConfig);

	//Used for reading values from message into attribute, use in SetCommand
	//XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, Depth, aConfig);
	//XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, Gain, aConfig);

	if (this->CreateParameterList(aConfig) != PLUS_SUCCESS)
		return PLUS_FAIL;
	
	return PLUS_SUCCESS;
}


PlusStatus vtkPlusGetCommand::CreateParameterReplies(vtkPlusUsCommandDevice* usCommandDevice)
{
	std::vector<std::string> validParametersFromDevice;
	usCommandDevice->GetValidParameterNames(validParametersFromDevice);
	
	std::vector<std::string> validParameters;

	ParameterReplies.clear();
	//ParameterReplies += "<Command>\n";
	ParameterReplies += "\n";

	std::vector<std::string>::iterator iter;
	for (iter = ParameterList.begin(); iter != ParameterList.end(); ++iter)
	{
		ParameterReplies += "<Result success=";
		if (std::find(validParametersFromDevice.begin(), validParametersFromDevice.end(), *iter) != validParametersFromDevice.end())
		{
			ParameterReplies += "true";
			validParameters.push_back(*iter);
		}
		else
		{
			ParameterReplies += "false";
		}
		ParameterReplies += "><Paramter Name=\"" + *iter + "\"/></Result>\n";
	}
	//ParameterReplies += "</Command>\n";

	if (usCommandDevice->TriggerParameterAnswers(validParameters) != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}

	return PLUS_SUCCESS;
}


PlusStatus vtkPlusGetCommand::CreateParameterList(vtkXMLDataElement* aConfig)
{
	ParameterList.clear();
	LOG_DEBUG("NumberOfNestedElements: " << aConfig->GetNumberOfNestedElements());
	int numElements = aConfig->GetNumberOfNestedElements();
	for (int i = 0; i < numElements; ++i)
	{
		vtkXMLDataElement* element = aConfig->GetNestedElement(i);
		if (!element)
		{
			LOG_ERROR("Got no element");
			return PLUS_FAIL;
		}

		if (element->GetAttribute("Name"))
		{
//			LOG_DEBUG("Name: " << element->GetAttribute("Name"));
			ParameterList.push_back(element->GetAttribute("Name"));
		}
		else
		{
			LOG_ERROR("Element got no Name");
			return PLUS_FAIL;
		}
	}
	return PLUS_SUCCESS;
}

PlusStatus vtkPlusGetCommand::Execute()
{
	LOG_DEBUG("vtkPlusGetCommand::Execute: " << (!this->Name.empty() ? this->Name : "(undefined)")
		<< ", Device: " << (this->DeviceId.empty() ? "(undefined)" : this->DeviceId)
		<< ", Depth: " <<  this->Depth
		<< ", Gain: " <<  this->Gain);

	vtkPlusDataCollector* dataCollector = GetDataCollector();
	if (dataCollector == NULL)
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", "Invalid data collector.");
		return PLUS_FAIL;
	}

	// Get device pointer
	if (this->DeviceId.empty())
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", "No DeviceId specified.");
		return PLUS_FAIL;
	}
	vtkPlusDevice* device = NULL;
	if (dataCollector->GetDevice(device, this->DeviceId) != PLUS_SUCCESS)
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", std::string("Device ")
			+ (this->DeviceId.empty() ? "(undefined)" : this->DeviceId) + std::string(" is not found."));
		return PLUS_FAIL;
	}

	vtkPlusUsCommandDevice* usCommandDevice = dynamic_cast<vtkPlusUsCommandDevice*>(device);
	if (!usCommandDevice)
	{
		this->QueueCommandResponse(PLUS_FAIL, "Command failed. See error message.", std::string("Device ")
			+ (this->DeviceId.empty() ? "(undefined)" : this->DeviceId) + std::string(" is not a vtkPlusUsCommandDevice."));
		return PLUS_FAIL;
	}

	if (this->CreateParameterReplies(usCommandDevice) != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	this->QueueCommandResponse(PLUS_SUCCESS, ParameterReplies);
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