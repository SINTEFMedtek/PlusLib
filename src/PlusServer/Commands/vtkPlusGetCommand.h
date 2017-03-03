#ifndef __vtkPlusGetCommand_h
#define __vtkPlusGetCommand_h

#include "vtkPlusServerExport.h"

#include "vtkPlusCommand.h"
class vtkPlusUsCommandDevice;

class vtkPlusServerExport vtkPlusGetCommand : public vtkPlusCommand
{
public:
	static vtkPlusGetCommand* New();
	vtkTypeMacro(vtkPlusGetCommand, vtkPlusCommand);
//	virtual void PrintSelf(ostream& os, vtkIndent indent);
	virtual vtkPlusCommand* Clone() { return New(); }

	/*! Get all the command names that this class can execute */
	virtual void GetCommandNames(std::list<std::string>& cmdNames);

	/*! Gets the description for the specified command name. */
	virtual std::string GetDescription(const std::string& commandName);

	/*! Id of the device that the text will be sent to */
	virtual std::string GetDeviceId() const;
	virtual void SetDeviceId(const std::string& deviceId);

	virtual double GetDepth() const;
	virtual void SetDepth(const double& depth);
	virtual double GetGain() const;
	virtual void SetGain(const double& gain);


	/*! Read command parameters from XML */
	virtual PlusStatus ReadConfiguration(vtkXMLDataElement* aConfig);

	/*! Executes the command  */
	virtual PlusStatus Execute();

protected:
	vtkPlusGetCommand();
	virtual ~vtkPlusGetCommand();

private:
	std::string DeviceId;
	double Depth, Gain;
	std::string ParameterReplies;
	std::vector<std::string> ParameterList;

	vtkPlusGetCommand(const vtkPlusGetCommand&);
	void operator=(const vtkPlusGetCommand&);

	PlusStatus CreateParameterList(vtkXMLDataElement* aConfig);
	PlusStatus CreateParameterReplies(vtkPlusUsCommandDevice* usCommandDevice);
};

#endif