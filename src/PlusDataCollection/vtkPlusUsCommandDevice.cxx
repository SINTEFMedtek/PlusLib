#include "vtkPlusUsCommandDevice.h"


vtkStandardNewMacro(vtkPlusUsCommandDevice);

void vtkPlusUsCommandDevice::GetValidParameterNames(std::vector<std::string>& parameterNames)
{
	parameterNames.clear();
}


PlusStatus vtkPlusUsCommandDevice::GenerateParameterAnswers(const std::vector<std::string> parameterNames, std::map<std::string, std::string>& parameterReplies)
{
	LOG_DEBUG("vtkPlusUsCommandDevice::GenerateParameterAnswers. Need implementation in subclass");
	return PLUS_FAIL;
}