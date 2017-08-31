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

  \ingroup PlusLibDataCollection
*/
class vtkPlusDataCollectionExport vtkPlusBkProFocusOemVideoSource : public vtkPlusUsDevice
{
public:

	enum PROBE_TYPE
	{
		UNKNOWN,
		SECTOR,
		LINEAR,
		MECHANICAL
	};

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

  /*! BK scanner address */
  vtkSetStringMacro(ScannerAddress);

  /*! BK scanner address */
  vtkGetStringMacro(ScannerAddress);

  /* BK scanner OEM port */
  vtkSetMacro(OemPort, unsigned short);
  
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

  /*! Enable/disable color in the streamed video */
  vtkSetMacro(ColorEnabled, bool);

  /*! Enable/disable color in the streamed video */
  vtkGetMacro(ColorEnabled, bool);

  /*! Enable/disable color in the streamed video */
  vtkBooleanMacro(ColorEnabled, bool);

  /*! Enable/disable offline testing */
  vtkSetMacro(OfflineTesting, bool);

  /*! Enable/disable offline testing */
  vtkGetMacro(OfflineTesting, bool);

  /*! Enable/disable offline testing */
  vtkBooleanMacro(OfflineTesting, bool);

  /*! Path to image file used for offline testing */
  vtkSetStringMacro(OfflineTestingFilePath);

  /*! Path to image file used for offline testing */
  vtkGetStringMacro(OfflineTestingFilePath);

protected:
  static const char* KEY_ORIGIN;
  static const char* KEY_ANGLES;
  static const char* KEY_BOUNDING_BOX;
  static const char* KEY_DEPTHS;
  static const char* KEY_LINEAR_WIDTH;


	static const char* KEY_DEPTH;
	static const char* KEY_GAIN;
	
	static const char* KEY_START_DEPTH;
	static const char* KEY_STOP_DEPTH;
	static const char* KEY_START_LINE_X;
	static const char* KEY_START_LINE_Y;
	static const char* KEY_STOP_LINE_X;
	static const char* KEY_STOP_LINE_Y;
	static const char* KEY_START_LINE_ANGLE;
	static const char* KEY_STOP_LINE_ANGLE;
	static const char* KEY_PROBE_TYPE;
	static const char* KEY_SPACING_X;
	static const char* KEY_SPACING_Y;

	static const char* KEY_SECTOR_LEFT_PIXELS;
	static const char* KEY_SECTOR_RIGHT_PIXELS;
	static const char* KEY_SECTOR_TOP_PIXELS;
	static const char* KEY_SECTOR_BOTTOM_PIXELS;
	static const char* KEY_SECTOR_LEFT_MM;
	static const char* KEY_SECTOR_RIGHT_MM;
	static const char* KEY_SECTOR_TOP_MM;
	static const char* KEY_SECTOR_BOTTOM_MM;
	//static const char* KEY_SECTOR_INFO;
	
  // Size of the ultrasound image. Only used if ContinuousStreamingEnabled is true.
  unsigned int UltrasoundWindowSize[2];

  //Ultrasound sector geometry, values read from the scanner
  double StartLineX_m, StartLineY_m, StartLineAngle_rad, StartDepth_m, StopLineX_m, StopLineY_m, StopLineAngle_rad, StopDepth_m;
  int pixelLeft_pix, pixelTop_pix, pixelRight_pix, pixelBottom_pix;
  int grabFramePixelLeft_pix, grabFramePixelTop_pix, grabFramePixelRight_pix, grabFramePixelBottom_pix;
  double tissueLeft_m, tissueTop_m, tissueRight_m, tissueBottom_m;
  //double resolutionX_m, resolutionY_m, probeCenterX_m, probeCenterY_m, probeRadius_m;
  int gain_percent;

  PROBE_TYPE probeTypePortA, probeTypePortB, probeTypePortC, probeTypePortM;
  std::string probePort;

  /*! Constructor */
  vtkPlusBkProFocusOemVideoSource();
  /*! Destructor */
  ~vtkPlusBkProFocusOemVideoSource();

  /*! Device-specific connect */
  virtual PlusStatus InternalConnect();

  PlusStatus StartDataStreaming();
  PlusStatus StopDataStreaming();

  /*! Device-specific disconnect */
  virtual PlusStatus InternalDisconnect();

  /*! Device-specific recording start */
  virtual PlusStatus InternalStartRecording();

  /*! Device-specific recording stop */
  virtual PlusStatus InternalStopRecording();

  /*! The internal function which actually does the grab.  */
  PlusStatus InternalUpdate();

  /*! Process all received messages until an image message is read. */
  PlusStatus ProcessMessagesAndReadNextImage(int maxReplySize);

  PlusStatus ReadNextMessage();
  std::vector<char> removeSpecialCharacters(std::vector<char> inMessage);
  int addAdditionalBinaryDataToImageUntilEOTReached(char &character, std::vector<char> &rawMessage);

  //PlusStatus SendReceiveQuery(std::string query, size_t replyBytes);
  PlusStatus SendQuery(std::string query);
  std::string AddSpecialCharacters(std::string query);
  
  /*! The internal function which ...  */
  PlusStatus QueryImageSize();
  PlusStatus QueryGeometryScanarea();
  PlusStatus QueryGeometryPixel();
  PlusStatus QueryGeometryUsGrabFrame();
  PlusStatus QueryGeometryTissue();
  //PlusStatus QueryTransverseImageCalibration();
  //PlusStatus QuerySagImageCalibration();
  PlusStatus QueryGain();
  PlusStatus QueryTransducerList();
  PlusStatus QueryTransducer();
  
  void ParseImageSize(std::istringstream &replyStream);
  void ParseGeometryScanarea(std::istringstream &replyStream);
  void ParseGeometryPixel(std::istringstream &replyStream);
  void ParseGeometryUsGrabFrame(std::istringstream &replyStream);
  void ParseGeometryTissue(std::istringstream &replyStream);
  void ParseGain(std::istringstream &replyStream);
  void ParseTransducerList(std::istringstream &replyStream);
  void ParseTransducerData(std::istringstream &replyStream);

  PlusStatus SubscribeToParameterChanges();
  PlusStatus ConfigEventsOn();
  PlusStatus CommandPowerDopplerOn();
  
  PlusStatus DecodePngImage(unsigned char* pngBuffer, unsigned int pngBufferSize, vtkImageData* decodedImage);

  PlusStatus RequestParametersFromScanner();

  //New standard
  std::vector<double> CalculateOrigin();
  std::vector<double> CalculateAngles();
  std::vector<double> CalculateBoundingBox();
  std::vector<double> CalculateDepths();
  double CalculateLinearWidth();

  //Utility functions
  bool IsSectorProbe();
  double CalculateWidthInRadians();
  bool similar(double a, double b, double tol = 1.0E-6);

  double CalculateDepthMm();
  int CalculateGain();
  PROBE_TYPE GetProbeType();

  double GetStartDepth();
  double GetStopDepth();
  double GetStartLineX();
  double GetStartLineY();
  double GetStopLineX();
  double GetStopLineY();
  double GetStartLineAngle();
  double GetStopLineAngle();
  double GetSpacingX();
  double GetSpacingY();
  int GetSectorLeftPixels();
  int GetSectorRightPixels();
  int GetSectorTopPixels();
  int GetSectorBottomPixels();
  double GetSectorLeftMm();
  double GetSectorRightMm();
  double GetSectorTopMm();
  double GetSectorBottomMm();

  PlusStatus AddParametersToFrameFields();

  /*! Read theOemClientReadBuffer into a string. Discards the ; at the end of the string */
  std::string ReadBufferIntoString();
  /*! Remove doube quotes from a string. E.g. "testString" -> testString */
  std::string RemoveQuotationMarks(std::string inString);
  void SetProbeTypeForPort(std::string port, std::string probeTypeString);

  /*! BK scanner address */
  char* ScannerAddress;
  /*! BK OEM port */
  unsigned short OemPort;

  bool ContinuousStreamingEnabled;
  bool ColorEnabled;
  bool OfflineTesting;
  char* OfflineTestingFilePath;

  // For internal storage of additional variables (to minimize the number of included headers)
  class vtkInternal;
  vtkInternal* Internal;

private:
  vtkPlusBkProFocusOemVideoSource(const vtkPlusBkProFocusOemVideoSource&);  // Not implemented.
  void operator=(const vtkPlusBkProFocusOemVideoSource&);  // Not implemented.

  //Special characters used in communication protocol
  enum
  {
	  SOH = 1,  ///> Start of header.
	  EOT = 4,  ///> End of header.
	  ESC = 27, ///> Escape, used for escaping the other special characters. The character itself is inverted.
  };
};

#endif
