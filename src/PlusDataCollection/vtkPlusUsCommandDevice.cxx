#include "vtkPlusUsCommandDevice.h"


vtkStandardNewMacro(vtkPlusUsCommandDevice);

void vtkPlusUsCommandDevice::GetValidParameterNames(std::vector<std::string>& parameterNames)
{
	parameterNames.clear();
}


void vtkPlusUsCommandDevice::GenerateParameterAnswers(const std::vector<std::string> parameterNames)
{
	LOG_DEBUG("vtkPlusUsCommandDevice::GenerateParameterAnswers. Need implementation in subclass");
}