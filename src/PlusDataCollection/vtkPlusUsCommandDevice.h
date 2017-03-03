#ifndef __vtkPlusUsCommandDevice_h
#define __vtkPlusUsCommandDevice_h

#include "vtkPlusDevice.h"

class vtkPlusDataCollectionExport vtkPlusUsCommandDevice : public vtkPlusDevice
{
public:
	static vtkPlusUsCommandDevice *New();
	vtkTypeMacro(vtkPlusUsCommandDevice, vtkPlusDevice);
	virtual void GetValidParameterNames(std::vector<std::string>& parameterNames);
};

#endif