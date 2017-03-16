#include "vtkPlusUsCommandDevice.h"


vtkStandardNewMacro(vtkPlusUsCommandDevice);

void vtkPlusUsCommandDevice::GetValidParameterNames(std::vector<std::string>& parameterNames)
{
	parameterNames.clear();
}


PlusStatus vtkPlusUsCommandDevice::TriggerParameterAnswers(const std::vector<std::string> parameterNamess)
{
	LOG_DEBUG("vtkPlusUsCommandDevice::TriggerParameterAnswers. Need implementation in subclass");
	return PLUS_FAIL;
}