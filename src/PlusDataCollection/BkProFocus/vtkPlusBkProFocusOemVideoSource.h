/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

// GRAB_FRAME API was partly contributed by by Xin Kang at SZI, Children's National Medical Center

#ifndef __vtkPlusBkProFocusOemVideoSource_h
#define __vtkPlusBkProFocusOemVideoSource_h

#include "vtkPlusDataCollectionExport.h"

#include "vtkPlusUsDevice.h"

/*!
  \class vtkPlusBkProFocusOemVideoSource 
  \brief Class for acquiring ultrasound images from BK ultrasound systems through the OEM interface

  Requires the PLUS_USE_BKPROFOCUS_VIDEO option in CMake.
  Requires GrabbieLib (SDK provided by BK).

  \ingroup PlusLibDataCollection
*/
class vtkPlusDataCollectionExport vtkPlusBkProFocusOemVideoSource : public vtkPlusUsDevice
{
public:
  static vtkPlusBkProFocusOemVideoSource *New();
  vtkTypeMacro(vtkPlusBkProFocusOemVideoSource, vtkPlusUsDevice);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual bool IsTracker() const { return false; }

  /*! Read configuration from xml data */  
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* config); 
  /*! Write configuration to xml data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* config);    

  /*! Verify the device is correctly configured */
  virtual PlusStatus NotifyConfigured();

  /*! Set the name of the BK ini file that stores connection and acquisition settings */
  vtkSetStringMacro(IniFileName);

  /*!
    Enable/disable continuous streaming. Continuous streaming (GRAB_FRAME command) requires extra license
    from BK but it allows faster image acquisition.
  */
  vtkSetMacro(ContinuousStreamingEnabled, bool);

  /*!
  Enable/disable continuous streaming. Continuous streaming (GRAB_FRAME command) requires extra license
  from BK but it allows faster image acquisition.
  */
  vtkGetMacro(ContinuousStreamingEnabled, bool);

  /*!
  Enable/disable continuous streaming. Continuous streaming (GRAB_FRAME command) requires extra license
  from BK but it allows faster image acquisition.
  */
  vtkBooleanMacro(ContinuousStreamingEnabled, bool);


  void GetValidParameterNames(std::vector<std::string>& parameterNames);
  PlusStatus TriggerParameterAnswers(const std::vector<std::string> parameterNames);

protected:
	
  // Size of the ultrasound image. Only used if ContinuousStreamingEnabled is true.
  unsigned int UltrasoundWindowSize[2];

  //Ultrasound sector geometry, values read from the scanner
  double StartLineX_m, StartLineY_m, StartLineAngle_rad, StartDepth_m, StopLineX_m, StopLineY_m, StopLineAngle_rad, StopDepth_m;
  int pixelLeft_pix, pixelTop_pix, pixelRight_pix, pixelBottom_pix;
  double tissueLeft_m, tissueTop_m, tissueRight_m, tissueBottom_m;
  //double resolutionX_m, resolutionY_m, probeCenterX_m, probeCenterY_m, probeRadius_m;
  int gain_percent;

  /*! Constructor */
  vtkPlusBkProFocusOemVideoSource();
  /*! Destructor */
  ~vtkPlusBkProFocusOemVideoSource();

  /*! Device-specific connect */
  virtual PlusStatus InternalConnect();

  /*! Device-specific disconnect */
  virtual PlusStatus InternalDisconnect();

  /*! Device-specific recording start */
  virtual PlusStatus InternalStartRecording();

  /*! Device-specific recording stop */
  virtual PlusStatus InternalStopRecording();

  /*! The internal function which actually does the grab.  */
  PlusStatus InternalUpdate();
  
  /*! The internal function which ...  */
  PlusStatus QueryImageSize();
  PlusStatus QueryGeometryScanarea();
  PlusStatus QueryGeometryPixel();
  PlusStatus QueryGeometryTissue();
  //PlusStatus QueryTransverseImageCalibration();
  //PlusStatus QuerySagImageCalibration();
  PlusStatus QueryGain();

  PlusStatus CommandPowerDopplerOn();
  PlusStatus SendReceiveQuery(std::string query, size_t replyBytes);

  PlusStatus GetFullIniFilePath(std::string &fullPath);

  PlusStatus DecodePngImage(unsigned char* pngBuffer, unsigned int pngBufferSize, vtkImageData* decodedImage);

  PlusStatus UpdateScannerParameters();

  std::string CalculateDepth();
  std::string CalculateGain();

  PlusStatus ProcessParameterValues(/*std::map<std::string, std::string>& parameters*/);
  PlusStatus AddParameterReplies();

  /*! BK ini file storing the connection and acquisition settings */
  char* IniFileName;

  bool ContinuousStreamingEnabled;

  // For internal storage of additional variables (to minimize the number of included headers)
  class vtkInternal;
  vtkInternal* Internal;

private:
  vtkPlusBkProFocusOemVideoSource(const vtkPlusBkProFocusOemVideoSource&);  // Not implemented.
  void operator=(const vtkPlusBkProFocusOemVideoSource&);  // Not implemented.
};

#endif
