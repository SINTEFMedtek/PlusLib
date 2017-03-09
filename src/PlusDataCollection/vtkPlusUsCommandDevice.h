#ifndef __vtkPlusUsCommandDevice_h
#define __vtkPlusUsCommandDevice_h

#include "vtkPlusDevice.h"

class vtkPlusDataCollectionExport vtkPlusUsCommandDevice : public vtkPlusDevice
{
public:
	static vtkPlusUsCommandDevice *New();
	vtkTypeMacro(vtkPlusUsCommandDevice, vtkPlusDevice);
	virtual void GetValidParameterNames(std::vector<std::string>& parameterNames);
	virtual PlusStatus GenerateParameterAnswers(const std::vector<std::string> parameterNames, std::map<std::string, std::string>& parameterReplies);
};

#endif