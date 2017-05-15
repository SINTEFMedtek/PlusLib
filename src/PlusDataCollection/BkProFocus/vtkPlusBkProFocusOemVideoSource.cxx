/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// GRAB_FRAME API was partly contributed by by Xin Kang at SZI, Children's National Medical Center

// Define OFFLINE_TESTING to read image input from file instead of reading from the actual hardware device.
// This is useful only for testing and debugging without having access to an actual BK scanner.
//#define OFFLINE_TESTING
//static const char OFFLINE_TESTING_FILENAME[] = "c:\\Users\\lasso\\Downloads\\bktest.png";
static const char OFFLINE_TESTING_FILENAME[] = "c:\\dev\\bktest.png";

#include "PlusConfigure.h"
#include "vtkPlusBkProFocusOemVideoSource.h"

#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtk_png.h"
#include "vtksys/SystemTools.hxx"

#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"
#include "vtkClientSocket.h"
#include "PixelCodec.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <ostream>
#include <string>
#include "stdint.h"

//TODO: Remove ParamConnectionSettings
#include "ParamConnectionSettings.h"

#include "UseCaseParser.h"
#include "UseCaseStructs.h"
#include "vtkPlusUsImagingParameters.h"

static const int TIMESTAMP_SIZE = 4;


const char* vtkPlusBkProFocusOemVideoSource::KEY_DEPTH				= "Depth";
const char* vtkPlusBkProFocusOemVideoSource::KEY_GAIN				= "Gain";

const char* vtkPlusBkProFocusOemVideoSource::KEY_START_DEPTH		= "StartDepth";
const char* vtkPlusBkProFocusOemVideoSource::KEY_STOP_DEPTH			= "StopDepth";
const char* vtkPlusBkProFocusOemVideoSource::KEY_START_LINE_X		= "StartLineX";
const char* vtkPlusBkProFocusOemVideoSource::KEY_START_LINE_Y		= "StartLineY";
const char* vtkPlusBkProFocusOemVideoSource::KEY_STOP_LINE_X		= "StopLineX";
const char* vtkPlusBkProFocusOemVideoSource::KEY_STOP_LINE_Y		= "StopLineY";
const char* vtkPlusBkProFocusOemVideoSource::KEY_START_LINE_ANGLE	= "StartLineAngle";
const char* vtkPlusBkProFocusOemVideoSource::KEY_STOP_LINE_ANGLE	= "StopLineAngle";

//const char* vtkPlusBkProFocusOemVideoSource::KEY_WIDTH			= "Width";
const char* vtkPlusBkProFocusOemVideoSource::KEY_PROBE_TYPE			= "ProbeType";

const char* vtkPlusBkProFocusOemVideoSource::KEY_SPACING_X			= "SpacingX";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SPACING_Y			= "SpacingY";

const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_LEFT_PIXELS		= "SectorLeftPixels";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_RIGHT_PIXELS	= "SectorRightPixels";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_TOP_PIXELS		= "SectorTopPixels";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_BOTTOM_PIXELS	= "SectorBottomPixels";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_LEFT_MM			= "SectorLeftMm";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_RIGHT_MM		= "SectorRightMm";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_TOP_MM			= "SectorTopMm";
const char* vtkPlusBkProFocusOemVideoSource::KEY_SECTOR_BOTTOM_MM		= "SectorBottomMm";

vtkStandardNewMacro(vtkPlusBkProFocusOemVideoSource);

class vtkPlusBkProFocusOemVideoSource::vtkInternal
{
public:
  vtkPlusBkProFocusOemVideoSource *External;

  vtkPlusChannel* Channel;

  ParamConnectionSettings BKparamSettings;
  vtkSmartPointer<vtkClientSocket> VtkSocket;
  std::vector<char> OemMessage;

  // Image buffer to hold the decoded image frames, it's a member variable to avoid memory allocation at each frame receiving
  vtkImageData* DecodedImageFrame;
  // Data buffer to hold temporary data during decoding, it's a member variable to avoid memory allocation at each frame receiving
  std::vector<unsigned char> DecodingBuffer;
  // Data buffer to hold temporary data during decoding (pointers to image lines), it's a member variable to avoid memory allocation at each frame receiving
  std::vector<png_bytep> DecodingLineBuffer;


  vtkPlusBkProFocusOemVideoSource::vtkInternal::vtkInternal(vtkPlusBkProFocusOemVideoSource* external)
    : External(external)
    , Channel(NULL)
  {
    this->DecodedImageFrame = vtkImageData::New();

  }

  virtual vtkPlusBkProFocusOemVideoSource::vtkInternal::~vtkInternal()
  {
    this->Channel = NULL;
    this->DecodedImageFrame->Delete();
    this->DecodedImageFrame = NULL;
    this->External = NULL;
  }

};


//----------------------------------------------------------------------------
vtkPlusBkProFocusOemVideoSource::vtkPlusBkProFocusOemVideoSource()
{
  this->Internal = new vtkInternal(this);

  this->IniFileName = NULL;

  this->ContinuousStreamingEnabled = false;
  this->UltrasoundWindowSize[0] = 0;
  this->UltrasoundWindowSize[1] = 0;
  this->StartLineX_m = 0;
  this->StartLineY_m = 0;
  this->StartLineAngle_rad = 0;
  this->StartDepth_m = 0;
  this->StopLineX_m = 0;
  this->StopLineY_m = 0;
  this->StopLineAngle_rad = 0;
  this->StopDepth_m = 0;
  this->pixelLeft_pix = 0;
  this->pixelTop_pix = 0;
  this->pixelRight_pix = 0;
  this->pixelBottom_pix = 0;
  this->tissueLeft_m = 0;
  this->tissueTop_m = 0;
  this->tissueRight_m = 0;
  this->tissueBottom_m = 0;
  this->gain_percent = 0;
  this->probeTypePortA = UNKNOWN;
  this->probeTypePortB = UNKNOWN;
  this->probeTypePortC = UNKNOWN;
  this->probeTypePortM = UNKNOWN;
  this->probePort = "";

  this->RequireImageOrientationInConfiguration = true;

  // No callback function provided by the device, so the data capture thread will be used to poll the hardware and add new items to the buffer
  this->StartThreadForInternalUpdates = true;
  this->AcquisitionRate = 1; // image retrieval may slow down the exam software, so keep the frame rate low by default
}

//----------------------------------------------------------------------------
vtkPlusBkProFocusOemVideoSource::~vtkPlusBkProFocusOemVideoSource()
{
  if (!this->Connected)
  {
    this->Disconnect();
  }

  delete this->Internal;
  this->Internal = NULL;

  SetIniFileName(NULL);
}

//----------------------------------------------------------------------------
void vtkPlusBkProFocusOemVideoSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::InternalConnect()
{
  LOG_TRACE("vtkPlusBkProFocusOemVideoSource::InternalConnect");

  if (this->Internal->Channel == NULL)
  {
    if (this->OutputChannels.empty())
    {
      LOG_ERROR("Cannot connect: no output channel is specified for device " << this->GetDeviceId());
      return PLUS_FAIL;
    }
    this->Internal->Channel = this->OutputChannels[0];
  }

  std::string iniFilePath;
  GetFullIniFilePath(iniFilePath);
  if (!this->Internal->BKparamSettings.LoadSettingsFromIniFile(iniFilePath.c_str()))
  {
    LOG_ERROR("Could not load BK parameter settings from file: " << iniFilePath.c_str());
    return PLUS_FAIL;
  }

  LOG_DEBUG("BK scanner address: " << this->Internal->BKparamSettings.GetScannerAddress());
  LOG_DEBUG("BK scanner OEM port: " << this->Internal->BKparamSettings.GetOemPort());
  LOG_DEBUG("BK scanner toolbox port: " << this->Internal->BKparamSettings.GetToolboxPort());

  // Clear buffer on connect because the new frames that we will acquire might have a different size 
  this->Internal->Channel->Clear();
  this->Internal->VtkSocket = vtkSmartPointer<vtkClientSocket>::New();

#ifndef OFFLINE_TESTING
  LOG_DEBUG("Connecting to BK scanner");
  bool connected = this->Internal->VtkSocket->ConnectToServer(this->Internal->BKparamSettings.GetScannerAddress(), this->Internal->BKparamSettings.GetOemPort());
  if (!connected)
  {
    LOG_ERROR("Could not connect to BKProFocusOem:"
      << " scanner address = " << this->Internal->BKparamSettings.GetScannerAddress()
      << ", OEM port = " << this->Internal->BKparamSettings.GetOemPort()
      << ", toolbox port = " << this->Internal->BKparamSettings.GetToolboxPort());
    return PLUS_FAIL;
  }
  LOG_DEBUG("Connected to BK scanner");

  if (this->ContinuousStreamingEnabled)
  {
	  if (!(this->RequestParametersFromScanner()
		  && this->ConfigEventsOn()
		  && this->SubscribeToParameterChanges()))
	  {
		  LOG_ERROR("Cound not init BK scanner");
		  return PLUS_FAIL;
	  }

    // Start data streaming
    std::string query = "QUERY:GRAB_FRAME \"ON\",20;";
    LOG_TRACE("Start data streaming. Query: " << query);
	if (!SendQuery(query))
	{
		LOG_ERROR("Cound not start data streaming");
		return PLUS_FAIL;
	}

	//Process all parameter messages, and read the first image message
	size_t numBytesReceived = 0;
	if (!ProcessMessagesAndReadNextImage(500, numBytesReceived))
	{
		LOG_ERROR("Cound not process inital parameter messages");
		return PLUS_FAIL;
	}
  }

#endif

  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::InternalDisconnect()
{

  LOG_DEBUG("Disconnect from BKProFocusOem");

#ifndef OFFLINE_TESTING
  std::string query = "QUERY:GRAB_FRAME \"OFF\";";
  LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);
  if (!SendQuery(query))
  {
	  return PLUS_FAIL;
  }

  // Retrieve the "ACK;"
  if(!this->ReadNextMessage())
  {
	  LOG_ERROR("Failed to read response from BK OEM interface");
	  return PLUS_FAIL;
  }
#endif

  this->StopRecording();
  
  if (this->Internal->VtkSocket->GetConnected())
	  this->Internal->VtkSocket->CloseSocket();

  return PLUS_SUCCESS;

}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::InternalStartRecording()
{
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::InternalStopRecording()
{
  return PLUS_SUCCESS;
}


//Not used for now
//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::AddParameterReplies()
{
	vtkPlusDataSource* parameterSource(NULL);
	if (this->Internal->Channel->GetParameterDataSource(parameterSource, "GetParameters") == PLUS_SUCCESS)
	{
		//std::map<std::string, std::string> parameters;
		if (this->ProcessParameterValues(/*parameters*/) == PLUS_SUCCESS)
		{
			LOG_DEBUG("Add parameter replies to parameter data source");
			//this->FrameNumber++;//Need to add frame number?
			/*if (parameterSource->AddItem(parameters, this->FrameNumber) != PLUS_SUCCESS)
			{
				LOG_ERROR("Error adding item " << parameterSource->GetSourceId() << " on channel " << this->Internal->Channel->GetChannelId());
				return PLUS_FAIL;
			}*/
		}
	}
	return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::InternalUpdate()
{
	//Test: Remove this for now. Just send paramaters as part of TrackedFrame, added to FrameFields. 
	//Setup which parameters to send in xml file.

	//Always send parameter replies
//	this->ProcessParameterValues();
	/*if (this->AddParameterReplies() != PLUS_SUCCESS)
	{
		LOG_ERROR("Error adding parameter replies on channel " << this->Internal->Channel->GetChannelId());
		return PLUS_FAIL;
	}*/
	
  if (!this->Recording)
  {
    // drop the frame, we are not recording data now
    return PLUS_SUCCESS;
  }

  unsigned char* uncompressedPixelBuffer = 0;
  unsigned int uncompressedPixelBufferSize = 0;
  int numBytesProcessed = 0;
#ifndef OFFLINE_TESTING
//  try
//  {
    // Set a buffer size that is likely to be able to hold a complete image
    int maxReplySize = 8 * 1024 * 1024;

    if (!this->ContinuousStreamingEnabled)
    {
      std::string query = "query:capture_image \"PNG\";";
      LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);
	  if (!SendQuery(query))
	  {
		  return PLUS_FAIL;
	  }
    }
    else
    {
      maxReplySize = 3 * this->UltrasoundWindowSize[0] * this->UltrasoundWindowSize[1] + 30; // incl. header & command
    }
	//Process all incomming messages until an image message is found
	size_t numBytesReceived = 0;
	if (!this->ProcessMessagesAndReadNextImage(maxReplySize, numBytesReceived))
	{
		return PLUS_FAIL;
	}
	
    // First detect the #
	for (numBytesProcessed = 0; this->Internal->OemMessage[numBytesProcessed] != '#' && numBytesProcessed < numBytesReceived; numBytesProcessed++);
    numBytesProcessed++;

	int numChars = (int)this->Internal->OemMessage[numBytesProcessed] - (int)('0');
    numBytesProcessed++;
    LOG_TRACE("Number of bytes in the image size: " << numChars); // 7 or 6
    if (numChars == 0)
    {
      LOG_ERROR("Failed to read image from BK OEM interface");
      return PLUS_FAIL;
    }

    for (int k = 0; k < numChars; k++, numBytesProcessed++)
    {
	  uncompressedPixelBufferSize = uncompressedPixelBufferSize * 10 + ((int)this->Internal->OemMessage[numBytesProcessed] - '0');
    }
    LOG_TRACE("uncompressedPixelBufferSize = " << uncompressedPixelBufferSize);

	uncompressedPixelBuffer = (unsigned char*)&(this->Internal->OemMessage[numBytesProcessed]);

    if (this->ContinuousStreamingEnabled)
    {
      // Extract timestamp of the image
      char timeStamp[TIMESTAMP_SIZE];
      for (int k = 0; k < TIMESTAMP_SIZE; k++, numBytesProcessed++)
      {
		timeStamp[k] = this->Internal->OemMessage[numBytesProcessed];
      }
      // Seems this is NOT correct, but the format is NOT described in the manual
      unsigned int _timestamp = *(int*)timeStamp;
      LOG_TRACE("Image timestamp = " << static_cast<std::ostringstream*>(&(std::ostringstream() << _timestamp))->str());
    }
/*  }
  catch (TcpClientWaitException e)
  {
    LOG_ERROR("Communication error on the BK OEM interface (TcpClientWaitException: " << e.Message << ")")
      return PLUS_FAIL;
  }*/

#endif

/*
FILE * f;
f = fopen("c:\\andrey\\bktest.bmp", "wb");
if(f!=NULL){
fwrite(&buf[numBytesProcessed],1,jpgSize,f);
fclose(f);
}
*/

  if (this->ContinuousStreamingEnabled)
  {
    this->Internal->DecodedImageFrame->SetExtent(0, this->UltrasoundWindowSize[0] - 1, 0, this->UltrasoundWindowSize[1] - 1, 0, 0);
    this->Internal->DecodedImageFrame->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

    if (uncompressedPixelBufferSize > (this->UltrasoundWindowSize[0] * this->UltrasoundWindowSize[1] + TIMESTAMP_SIZE))
    {
      // we received color image, convert to grayscale
      PlusStatus status = PixelCodec::ConvertToGray(BI_RGB, this->UltrasoundWindowSize[0], this->UltrasoundWindowSize[1],
        (unsigned char*)&(this->Internal->OemMessage[numBytesProcessed]),
        (unsigned char*)this->Internal->DecodedImageFrame->GetScalarPointer());
    }
    else
    {
      std::memcpy(this->Internal->DecodedImageFrame->GetScalarPointer(),
        (void*)&(this->Internal->OemMessage[numBytesProcessed]),
        uncompressedPixelBufferSize);
      LOG_TRACE(uncompressedPixelBufferSize << " bytes copied, start at " << numBytesProcessed); // 29
    }
  }
  else
  {
    if (DecodePngImage(uncompressedPixelBuffer, uncompressedPixelBufferSize, this->Internal->DecodedImageFrame) != PLUS_SUCCESS)
    {
      LOG_ERROR("Failed to decode received PNG image on channel " << this->Internal->Channel->GetChannelId());
      return PLUS_FAIL;
    }
  }
  this->FrameNumber++;

  vtkPlusDataSource* aSource(NULL);
  if (this->Internal->Channel->GetVideoSource(aSource) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to retrieve the video source in the BKProFocusOem device on channel " << this->Internal->Channel->GetChannelId());
    return PLUS_FAIL;
  }
  // If the buffer is empty, set the pixel type and frame size to the first received properties 
  if (aSource->GetNumberOfItems() == 0)
  {
    LOG_DEBUG("Set up BK ProFocus image buffer");
    int* frameExtent = this->Internal->DecodedImageFrame->GetExtent();
    int frameSizeInPix[2] = { frameExtent[1] - frameExtent[0] + 1, frameExtent[3] - frameExtent[2] + 1 };
    aSource->SetPixelType(this->Internal->DecodedImageFrame->GetScalarType());
    aSource->SetImageType(US_IMG_BRIGHTNESS);
    aSource->SetInputFrameSize(frameSizeInPix[0], frameSizeInPix[1], 1);

    LOG_INFO("Frame size: " << frameSizeInPix[0] << "x" << frameSizeInPix[1]
      << ", pixel type: " << vtkImageScalarTypeNameMacro(this->Internal->DecodedImageFrame->GetScalarType())
      << ", buffer image orientation: " << PlusVideoFrame::GetStringFromUsImageOrientation(aSource->GetInputImageOrientation()));

  }

  double spacingZ_mm = 1.0;
  this->Internal->DecodedImageFrame->SetSpacing(GetSpacingX(), GetSpacingY(), spacingZ_mm);//Spacing is not being sent to IGTLink?

  this->AddParametersToFrameFields();
  //if (aSource->AddItem(this->Internal->DecodedImageFrame, aSource->GetInputImageOrientation(), US_IMG_BRIGHTNESS, this->FrameNumber) != PLUS_SUCCESS)
  if (aSource->AddItem(this->Internal->DecodedImageFrame, aSource->GetInputImageOrientation(), US_IMG_BRIGHTNESS, this->FrameNumber, UNDEFINED_TIMESTAMP, UNDEFINED_TIMESTAMP, &this->FrameFields) != PLUS_SUCCESS)
  {
    LOG_ERROR("Error adding item to video source " << aSource->GetSourceId() << " on channel " << this->Internal->Channel->GetChannelId());
    return PLUS_FAIL;
  }
  this->Modified();
  
  return PLUS_SUCCESS;
}


//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::ProcessMessagesAndReadNextImage(int maxReplySize, size_t &numBytesReceived)
{
	LOG_DEBUG("ProcessMessagesAndReadNextImage");
	//this->Internal->OemClientReadBuffer.resize(maxReplySize);

	//Read and process messages until an image message is found
	while (true)
	{
		LOG_TRACE("Before client read");
		if(!this->ReadNextMessage())
		{
			LOG_ERROR("Failed to read response from BK OEM interface");
			return PLUS_FAIL;
		}
		
		std::string fullMessage = this->ReadBufferIntoString();
		std::istringstream replyStream(fullMessage);

		std::string messageString;
		std::getline(replyStream, messageString, ' ');

		std::istringstream messageStream(messageString);
		std::string messageType;
		std::string messageName;
		std::string messageSubtype;//Typically A or B (view A or B on the scanner)
		std::getline(messageStream, messageType, ':');
		std::getline(messageStream, messageName, ':');
		std::getline(messageStream, messageSubtype, ':');

		LOG_DEBUG("Process message from BK: " << fullMessage);

		if (messageString.compare("DATA:CAPTURE_IMAGE") == 0)
		{
			return PLUS_SUCCESS;
		}
		else if (messageString.compare("DATA:GRAB_FRAME") == 0)
		{
			return PLUS_SUCCESS;
		}
		//Handle both replies to queries and subscribed data
		else if ((messageType.compare("DATA") == 0) || (messageType.compare("SDATA") == 0))
		{
			if (messageName.compare("US_WIN_SIZE") == 0)
			{
				this->ParseImageSize();
			}
			else if ((messageName.compare("B_GEOMETRY_SCANAREA") == 0) && (messageSubtype.compare("A") == 0))
			{
				this->ParseGeometryScanarea();
			}
			else if ((messageName.compare("B_GEOMETRY_PIXEL") == 0) && (messageSubtype.compare("A") == 0))
			{
				this->ParseGeometryPixel();
			}
			else if ((messageName.compare("B_GEOMETRY_TISSUE") == 0) && (messageSubtype.compare("A") == 0))
			{
				this->ParseGeometryTissue();
			}
			else if ((messageName.compare("B_GAIN") == 0) && (messageSubtype.compare("A") == 0))
			{
				this->ParseGain();
			}
			else if (messageName.compare("TRANSDUCER_LIST") == 0)
			{
				this->ParseTransducerList(replyStream);
			}
			else if ((messageName.compare("TRANSDUCER") == 0) && (messageSubtype.compare("A") == 0))
			{
				this->ParseTransducerData(replyStream);
			}
		}
		else if ((messageString.compare("EVENT:TRANSDUCER_CONNECT") == 0)
			|| (messageString.compare("EVENT:TRANSDUCER_DISCONNECT") == 0)
			|| (messageString.compare("EVENT:TRANSDUCER_SELECTED") == 0) )
		{
			this->QueryTransducerList();//Need to query, as this can't be subscribed to
			//this->QueryTransducer();//TODO: May not be needed if we subscribe to this?
		}
		else if (messageString.compare("EVENT:FREEZE") == 0)
		{
			LOG_DEBUG("Freeze");
		}
		else if (messageString.compare("EVENT:UNFREEZE") == 0)
		{
			LOG_DEBUG("Unfreeze");
		}
		else if (messageString.compare("ACK") == 0)
		{
			LOG_DEBUG("Acknowledge message received");
		}
		else
		{
			LOG_WARNING("Received unknown message from BK: " << messageString);
		}
	}

	return PLUS_SUCCESS; //Should newer reach this
}

PlusStatus vtkPlusBkProFocusOemVideoSource::ReadNextMessage()
{
	std::vector<char> rawMessage;
	char character(0);
	unsigned totalBytes = 0;
	int receivedBytes = 1;
	while (character != EOT && receivedBytes == 1)
	{
		receivedBytes = this->Internal->VtkSocket->Receive(&character, 1);
		rawMessage.push_back(character);
		totalBytes++;
	}
	this->Internal->OemMessage = removeSpecialCharacters(rawMessage);

	if (receivedBytes != 1)
		return PLUS_FAIL;
	else
		return PLUS_SUCCESS;
}

std::vector<char> vtkPlusBkProFocusOemVideoSource::removeSpecialCharacters(std::vector<char> inMessage)
{
	std::vector<char> retval;
	int inPos = 1;//Skip starting character SOH
	while (inPos < inMessage.size() - 1)//Skip ending character EOT
	{
		if ((inMessage[inPos]) != ESC)
		{
			retval.push_back(inMessage[inPos++]);
		}
		else
		{
			inPos++;
			retval.push_back(~inMessage[inPos++]);//Character after ESC is inverted
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// QUERY:US_WIN_SIZE;
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryImageSize()
{
	LOG_DEBUG("Get ultrasound image size from BKProFocusOem");

	std::string query = "QUERY:US_WIN_SIZE;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);
	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseImageSize()
{
  // Retrieve the "DATA:US_WIN_SIZE X,Y;"
  sscanf(&(this->Internal->OemMessage[0]), "DATA:US_WIN_SIZE %d,%d;", &this->UltrasoundWindowSize[0], &this->UltrasoundWindowSize[1]);
  LOG_TRACE("Ultrasound image size = " << this->UltrasoundWindowSize[0] << " x " << this->UltrasoundWindowSize[1]);
}

// QUERY:B_GEOMETRY_SCANAREA;
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryGeometryScanarea()
{
	LOG_DEBUG("Get ultrasound geometry from BKProFocusOem");

	std::string query = "QUERY:B_GEOMETRY_SCANAREA:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}
	
void vtkPlusBkProFocusOemVideoSource::ParseGeometryScanarea()
{
	// Retrieve the "DATA:B_GEOMETRY_SCANAREA StartLineX(m),StartLineY(m),StartLineAngle(rad),StartDepth(m),StopLineX(m),StopLineY(m),StopLineAngle(rad),StopDepth(m);"
	sscanf(&(this->Internal->OemMessage[0]), "DATA:B_GEOMETRY_SCANAREA:A %lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf;",
		&StartLineX_m, &StartLineY_m, &StartLineAngle_rad, &StartDepth_m, &StopLineX_m, &StopLineY_m, &StopLineAngle_rad, &StopDepth_m);
	LOG_DEBUG("Ultrasound geometry. StartLineX_m: " << StartLineX_m << " StartLineY_m: " << StartLineY_m << " StartLineAngle_rad: " << StartLineAngle_rad <<
		" StartDepth_m: " << StartDepth_m << " StopLineX_m: " << StopLineX_m << " StopLineY_m: " << StopLineY_m << " StopLineAngle_rad: " << StopLineAngle_rad << " StopDepth_m: " << StopDepth_m);
}

//-----------------------------------------------------------------------------
// QUERY:B_GEOMETRY_PIXEL;
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryGeometryPixel()
{
	std::string query = "QUERY:B_GEOMETRY_PIXEL:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseGeometryPixel()
{
	// Retrieve the "DATA:B_GEOMETRY_PIXEL Left,Top,Right,Bottom;"
	sscanf(&(this->Internal->OemMessage[0]), "DATA:B_GEOMETRY_PIXEL:A %d,%d,%d,%d;",
		&pixelLeft_pix, &pixelTop_pix, &pixelRight_pix, &pixelBottom_pix);
	LOG_DEBUG("Ultrasound geometry. pixelLeft_pix: " << pixelLeft_pix << " pixelTop_pix: " << pixelTop_pix << " pixelRight_pix: " << pixelRight_pix << " pixelBottom_pix: " << pixelBottom_pix);
}

//-----------------------------------------------------------------------------
// QUERY:B_GEOMETRY_TISSUE;
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryGeometryTissue()
{
	std::string query = "QUERY:B_GEOMETRY_TISSUE:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseGeometryTissue()
{
	sscanf(&(this->Internal->OemMessage[0]), "DATA:B_GEOMETRY_TISSUE:A %lf,%lf,%lf,%lf;",
		&tissueLeft_m, &tissueTop_m, &tissueRight_m, &tissueBottom_m);
	LOG_DEBUG("Ultrasound geometry. tissueLeft_m: " << tissueLeft_m << " tissueTop_m: " << tissueTop_m << " tissueRight_m: " << tissueRight_m << " tissueBottom_m: " << tissueBottom_m);
}

//-----------------------------------------------------------------------------
// QUERY:B_GAIN;
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryGain()
{
	std::string query = "QUERY:B_GAIN:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseGain()
{
	sscanf(&(this->Internal->OemMessage[0]), "DATA:B_GAIN:A %d;", &gain_percent);
	LOG_TRACE("Ultrasound gain. gain_percent: " << gain_percent);
}


//-----------------------------------------------------------------------------
// QUERY:TRANSDUCER_LIST;
// Get list of transducers, connected to which port, and tranducer type
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryTransducerList()
{
	std::string query = "QUERY:TRANSDUCER_LIST;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseTransducerList(std::istringstream &replyStream)
{
	std::string probeName;
	std::string probeType;
	//Port A
	std::getline(replyStream, probeName, ',');
	std::getline(replyStream, probeType, ',');
	this->SetProbeTypeForPort("A", RemoveQuotationMarks(probeType));
	//Port B
	std::getline(replyStream, probeName, ',');
	std::getline(replyStream, probeType, ',');
	this->SetProbeTypeForPort("B", RemoveQuotationMarks(probeType));
	//Port C
	std::getline(replyStream, probeName, ',');
	std::getline(replyStream, probeType, ',');
	this->SetProbeTypeForPort("C", RemoveQuotationMarks(probeType));

	//Port M
	std::getline(replyStream, probeName, ',');
	std::getline(replyStream, probeType, ',');
	this->SetProbeTypeForPort("M", RemoveQuotationMarks(probeType));
}

std::string vtkPlusBkProFocusOemVideoSource::ReadBufferIntoString()
{
	std::string retval(this->Internal->OemMessage.begin(), this->Internal->OemMessage.end());
	/*bool stop = false;
	int readPos = 0;

	while (readPos < this->Internal->OemMessage.size() && this->Internal->OemMessage[readPos] != ';')
	{
		retval += this->Internal->OemMessage[readPos++];
	}*/
	return retval;
}

std::string vtkPlusBkProFocusOemVideoSource::RemoveQuotationMarks(std::string inString)
{
	std::string retval;
	std::istringstream inStream(inString);
	std::getline(inStream, retval, '"');//Removes first "
	std::getline(inStream, retval, '"');//Read characters until next " is found
	return retval;
}

void vtkPlusBkProFocusOemVideoSource::SetProbeTypeForPort(std::string port, std::string probeTypeString)
{
	PROBE_TYPE probeTypeEnum = UNKNOWN;
	
	if (probeTypeString.compare("C") == 0)
		probeTypeEnum = SECTOR;
	else if (probeTypeString.compare("L") == 0)
		probeTypeEnum = LINEAR;
	else if (probeTypeString.compare("M") == 0)
		probeTypeEnum = MECHANICAL;
	
	if (port.compare("A") == 0)
		probeTypePortA = probeTypeEnum;
	else if (port.compare("B") == 0)
		probeTypePortB = probeTypeEnum;
	else if (port.compare("C") == 0)
		probeTypePortC = probeTypeEnum;
	else if (port.compare("M") == 0)
		probeTypePortM = probeTypeEnum;
}

//-----------------------------------------------------------------------------
// QUERY:TRANSDUCER:A;
// Get transducer that is used to create view A
PlusStatus vtkPlusBkProFocusOemVideoSource::QueryTransducer()
{
	std::string query = "QUERY:TRANSDUCER:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}

void vtkPlusBkProFocusOemVideoSource::ParseTransducerData(std::istringstream &replyStream)
{
	std::string probePortString;
	std::string probeName;
	std::getline(replyStream, probePortString, ',');
	std::getline(replyStream, probeName, ',');
	probePort = this->RemoveQuotationMarks(probePortString);
}


//-----------------------------------------------------------------------------
// CONFIG:DATA:SUBSCRIBE;
PlusStatus vtkPlusBkProFocusOemVideoSource::SubscribeToParameterChanges()
{
	std::string query = "CONFIG:DATA:SUBSCRIBE ";
	query += "\"US_WIN_SIZE\"";
	query += ",\"B_GEOMETRY_SCANAREA\"";
	query += ",\"B_GEOMETRY_PIXEL\"";
	query += ",\"B_GEOMETRY_TISSUE\"";
	query += ",\"B_GAIN\"";
	query += ",\"TRANSDUCER\"";
	query += ";";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	return SendQuery(query);
}


//-----------------------------------------------------------------------------
// QUERY:B_TRANS_IMAGE_CALIB; //Get only zeroes as return values
/*PlusStatus vtkPlusBkProFocusOemVideoSource::QueryTransverseImageCalibration()
{
	std::string query = "QUERY:B_TRANS_IMAGE_CALIB:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	size_t replyBytes = 100;
	PlusStatus retval = SendReceiveQuery(query, replyBytes);

	sscanf(&(this->Internal->OemClientReadBuffer[0]), "DATA:B_TRANS_IMAGE_CALIB:A %lf,%lf,%lf,%lf,%lf;",
		&resolutionX_m, &resolutionY_m, &probeCenterX_m, &probeCenterY_m, &probeRadius_m);
	LOG_TRACE("Ultrasound geometry. resolutionX_m: " << resolutionX_m << " resolutionY_m: " << resolutionY_m << " probeCenterX_m: " << probeCenterX_m << " probeCenterY_m: " << probeCenterY_m << " probeRadius_m: " << probeRadius_m);

	return retval;
}

//-----------------------------------------------------------------------------
// QUERY:B_SAG_IMAGE_CALIB; //Get only zeroes as return values
PlusStatus vtkPlusBkProFocusOemVideoSource::QuerySagImageCalibration()
{
	std::string query = "QUERY:B_SAG_IMAGE_CALIB:A;";
	LOG_TRACE("Query from vtkPlusBkProFocusOemVideoSource: " << query);

	size_t replyBytes = 100;
	PlusStatus retval = SendReceiveQuery(query, replyBytes);

	sscanf(&(this->Internal->OemClientReadBuffer[0]), "DATA:B_SAG_IMAGE_CALIB:A %lf,%lf,%lf,%lf,%lf;",
		&resolutionX_m, &resolutionY_m, &probeCenterX_m, &probeCenterY_m, &probeRadius_m);
	LOG_TRACE("Ultrasound geometry. resolutionX_m: " << resolutionX_m << " resolutionY_m: " << resolutionY_m << " probeCenterX_m: " << probeCenterX_m << " probeCenterY_m: " << probeCenterY_m << " probeRadius_m: " << probeRadius_m);

	return retval;
}*/

//-----------------------------------------------------------------------------
// CONFIG:EVENTS;
PlusStatus vtkPlusBkProFocusOemVideoSource::ConfigEventsOn()
{
	std::string query = "CONFIG:EVENTS 1;";
	LOG_TRACE("Command from vtkPlusBkProFocusOemVideoSource: " << query);
	return SendQuery(query);
}

//-----------------------------------------------------------------------------
// COMMAND:P_MODE;
PlusStatus vtkPlusBkProFocusOemVideoSource::CommandPowerDopplerOn()
{
	std::string query = "COMMAND:P_MODE: \"ON\";";
	LOG_TRACE("Command from vtkPlusBkProFocusOemVideoSource: " << query);
	return SendQuery(query);
}

//-----------------------------------------------------------------------------
/*PlusStatus vtkPlusBkProFocusOemVideoSource::SendReceiveQuery(std::string query, size_t replyBytes)
{
	size_t queryWrittenSize = this->Internal->OemClient->Write(query.c_str(), query.size());
	if (queryWrittenSize != query.size() + 2) // OemClient->Write returns query.size()+2 on a successfully sent event (see #722)
	{
		LOG_ERROR("Failed to send query through BK OEM interface (" << query << ")" << queryWrittenSize << " vs " << query.size() << "+2");
		return PLUS_FAIL;
	}

	this->Internal->OemClientReadBuffer.resize(replyBytes);
	size_t numBytesReceived = this->Internal->OemClient->Read(&(this->Internal->OemClientReadBuffer[0]), replyBytes);
	if (numBytesReceived == 0)
	{
		LOG_ERROR("Failed to read response from BK OEM interface");
		return PLUS_FAIL;
	}
	return PLUS_SUCCESS;
}*/

//-----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::SendQuery(std::string query)
{
	std::string codedQuery = this->AddSpecialCharacters(query);
	
	if (!this->Internal->VtkSocket->Send(codedQuery.c_str(), codedQuery.size()))
	{
		return PLUS_FAIL;
	}
	return PLUS_SUCCESS;
}

std::string vtkPlusBkProFocusOemVideoSource::AddSpecialCharacters(std::string query)
{
	std::string retval;
	const char special[] = { SOH, EOT, ESC, 0 }; // 0 is not special, it is an indicator for end of string
	retval += SOH; //Add start character
	for (int i = 0; i < query.size(); i++)
	{
		char ch = query[i];
		if (NULL != strchr(special, ch))
		{
			retval += ESC; //Escape special character
			ch = ~ch; //Invert special character
		}
		retval += ch;
	}
	retval += EOT; //Add end character
	return retval;
}

//-----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::ReadConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);
  XML_READ_CSTRING_ATTRIBUTE_REQUIRED(IniFileName, deviceConfig);
  XML_READ_BOOL_ATTRIBUTE_OPTIONAL(ContinuousStreamingEnabled, deviceConfig);
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);
  deviceConfig->SetAttribute("IniFileName", this->IniFileName);
  XML_WRITE_BOOL_ATTRIBUTE(ContinuousStreamingEnabled, deviceConfig);
  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::GetFullIniFilePath(std::string &fullPath)
{
  if (this->IniFileName == NULL)
  {
    LOG_ERROR("Ini file name has not been set");
    return PLUS_FAIL;
  }
  if (vtksys::SystemTools::FileIsFullPath(this->IniFileName))
  {
    fullPath = this->IniFileName;
  }
  else
  {
    fullPath = vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationPath(this->IniFileName);
  }
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::NotifyConfigured()
{
  if (this->OutputChannels.size() > 1)
  {
    LOG_WARNING("vtkPlusBkProFocusOemVideoSource is expecting one output channel and there are " << this->OutputChannels.size() << " channels. First output channel will be used.");
  }

  if (this->OutputChannels.empty())
  {
    LOG_ERROR("No output channels defined for vtkPlusBkProFocusOemVideoSource. Cannot proceed.");
    this->CorrectlyConfigured = false;
    return PLUS_FAIL;
  }

  this->Internal->Channel = this->OutputChannels[0];

  return PLUS_SUCCESS;
}

void ReadDataFromByteArray(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
  if (png_ptr->io_ptr == NULL)
  {
    LOG_ERROR("ReadDataFromInputStream failed, no input pointer is set");
    png_error(png_ptr, "ReadDataFromInputStream failed, no input pointer is set");
    return;
  }

  unsigned char* bufferPointer = (unsigned char*)png_ptr->io_ptr;
  memcpy(outBytes, bufferPointer, byteCountToRead);
  bufferPointer += byteCountToRead;

  png_ptr->io_ptr = bufferPointer;
}

void PngErrorCallback(png_structp png_ptr, png_const_charp message)
{
  LOG_ERROR("PNG error: " << (message ? message : "no details available"));
}

void PngWarningCallback(png_structp png_ptr, png_const_charp message)
{
  LOG_WARNING("PNG warning: " << (message ? message : "no details available"));
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::DecodePngImage(unsigned char* pngBuffer, unsigned int pngBufferSize, vtkImageData* decodedImage)
{

#ifdef OFFLINE_TESTING
  FILE *fp = fopen(OFFLINE_TESTING_FILENAME, "rb");
  if (!fp)
  {
    LOG_ERROR("Failed to read png");
    return PLUS_FAIL;
  }
  fseek(fp, 0, SEEK_END);
  size_t fileSizeInBytes = ftell(fp);
  rewind(fp);
  std::vector<unsigned char> fileReadBuffer;
  fileReadBuffer.resize(fileSizeInBytes);
  pngBuffer = &(fileReadBuffer[0]);
  fread(pngBuffer, 1, fileSizeInBytes, fp);
  fclose(fp);
#endif

  unsigned int headerSize = 8;
  unsigned char* header = pngBuffer; // a 8-byte header
  int is_png = !png_sig_cmp(header, 0, headerSize);
  if (!is_png)
  {
    LOG_ERROR("Invalid PNG header");
    return PLUS_FAIL;
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
  if (!png_ptr)
  {
    LOG_ERROR("Failed to decode PNG buffer");
    return PLUS_FAIL;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
  {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    LOG_ERROR("Failed to decode PNG buffer");
    return PLUS_FAIL;
  }

  png_infop end_info = png_create_info_struct(png_ptr);
  if (!end_info)
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    LOG_ERROR("Failed to decode PNG buffer");
    return PLUS_FAIL;
  }

  png_set_error_fn(png_ptr, NULL, PngErrorCallback, PngWarningCallback);

  // Set error handling
  if (setjmp(png_jmpbuf(png_ptr)))
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    LOG_ERROR("Failed to decode PNG buffer");
    return PLUS_FAIL;
  }

  png_set_read_fn(png_ptr, pngBuffer + 8, ReadDataFromByteArray);

  //png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  int compression_type, filter_method;
  // get size and bit-depth of the PNG-image
  png_get_IHDR(png_ptr, info_ptr,
    &width, &height,
    &bit_depth, &color_type, &interlace_type,
    &compression_type, &filter_method);

  // set-up the transformations
  // convert palettes to RGB
  if (color_type == PNG_COLOR_TYPE_PALETTE)
  {
    png_set_palette_to_rgb(png_ptr);
  }

  // minimum of a byte per pixel
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
  {
#if PNG_LIBPNG_VER >= 10400
    png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
    png_set_gray_1_2_4_to_8(png_ptr);
#endif
  }

  // add alpha if any alpha found
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
  {
    png_set_tRNS_to_alpha(png_ptr);
  }

  if (bit_depth > 8)
  {
#ifndef VTK_WORDS_BIGENDIAN
    png_set_swap(png_ptr);
#endif
  }

  // have libpng handle interlacing
  //int number_of_passes = png_set_interlace_handling(png_ptr);

  // update the info now that we have defined the filters
  png_read_update_info(png_ptr, info_ptr);

  int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
  this->Internal->DecodingBuffer.resize(rowbytes*height);
  unsigned char *tempImage = &(this->Internal->DecodingBuffer[0]);

  this->Internal->DecodingLineBuffer.resize(height);
  png_bytep *row_pointers = &(this->Internal->DecodingLineBuffer[0]);
  for (unsigned int ui = 0; ui < height; ++ui)
  {
    row_pointers[ui] = tempImage + rowbytes*ui;
  }
  png_read_image(png_ptr, row_pointers);

  // copy the data into the outPtr
  if (width * 3 != rowbytes)
  {
    LOG_WARNING("There is padding at the end of PNG lines, image may be skewed");
  }

  decodedImage->SetExtent(0, width - 1, 0, height - 1, 0, 0);

#if (VTK_MAJOR_VERSION < 6)
  decodedImage->SetScalarTypeToUnsignedChar();
  decodedImage->SetNumberOfScalarComponents(1);
  decodedImage->AllocateScalars();
#else
  decodedImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
#endif

  PlusStatus status = PixelCodec::ConvertToGray(PixelCodec::PixelEncoding_RGBA32, width, height, &(this->Internal->DecodingBuffer[0]), (unsigned char*)decodedImage->GetScalarPointer());

  // close the file
  png_read_end(png_ptr, NULL);
  png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

  return status;
}


//----------------------------------------------------------------------------
void vtkPlusBkProFocusOemVideoSource::GetValidParameterNames(std::vector<std::string>& parameterNames)
{
	parameterNames.clear();
	parameterNames.push_back(KEY_DEPTH);
	parameterNames.push_back(KEY_GAIN);

	parameterNames.push_back(KEY_START_DEPTH);
	parameterNames.push_back(KEY_STOP_DEPTH);
	parameterNames.push_back(KEY_START_LINE_X);
	parameterNames.push_back(KEY_START_LINE_Y);
	parameterNames.push_back(KEY_STOP_LINE_X);
	parameterNames.push_back(KEY_STOP_LINE_Y);
	parameterNames.push_back(KEY_START_LINE_ANGLE);
	parameterNames.push_back(KEY_STOP_LINE_ANGLE);
	parameterNames.push_back(KEY_PROBE_TYPE);
	parameterNames.push_back(KEY_SPACING_X);
	parameterNames.push_back(KEY_SPACING_Y);
	parameterNames.push_back(KEY_SECTOR_LEFT_PIXELS);
	parameterNames.push_back(KEY_SECTOR_RIGHT_PIXELS);
	parameterNames.push_back(KEY_SECTOR_TOP_PIXELS);
	parameterNames.push_back(KEY_SECTOR_BOTTOM_PIXELS);
	parameterNames.push_back(KEY_SECTOR_LEFT_MM);
	parameterNames.push_back(KEY_SECTOR_RIGHT_MM);
	parameterNames.push_back(KEY_SECTOR_TOP_MM);
	parameterNames.push_back(KEY_SECTOR_BOTTOM_MM);
	//parameterNames.push_back(KEY_SECTOR_INFO);
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::TriggerParameterAnswers(const std::vector<std::string> parameterNames)
{
	LOG_DEBUG("vtkPlusBkProFocusOemVideoSource::TriggerParameterAnswers");

	//Use either the existing variable FieldDataSources or create a new one: ParameterDataSources?

	bool newParameterDataSource = false;
	vtkSmartPointer<vtkPlusDataSource> parameterDataSource;
	vtkPlusDataSource* aSource(NULL);
	if (this->Internal->Channel->GetParameterDataSource(aSource, "GetParameters") != PLUS_SUCCESS)
	{
		parameterDataSource = vtkSmartPointer<vtkPlusDataSource>::New();
		parameterDataSource->SetId("GetParameters");
		newParameterDataSource = true;
	}
	else
	{
		parameterDataSource = aSource;
	}
	
	for (unsigned i = 0; i < parameterNames.size(); ++i)
	{
		parameterDataSource->SetCustomProperty(parameterNames[i], "update");
	}
	parameterDataSource->SetCustomProperty("Processed", "");

	if (newParameterDataSource)
	{
		this->Internal->Channel->AddParameterDataSource(parameterDataSource);
	}

	return PLUS_SUCCESS;
}

PlusStatus vtkPlusBkProFocusOemVideoSource::RequestParametersFromScanner()
{
	if (this->QueryImageSize() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryGeometryScanarea() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryGeometryPixel() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryGeometryTissue() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryGain() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryTransducerList() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->QueryTransducer() != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	
	return PLUS_SUCCESS;
}

PlusStatus vtkPlusBkProFocusOemVideoSource::UpdateScannerParameters()
{
	if (this->RequestParametersFromScanner() != PLUS_SUCCESS)
	{
		//Disable for testing
#ifndef OFFLINE_TESTING
		return PLUS_FAIL;
#endif
	}

	//this->CurrentImagingParameters->SetValue<std::string>(vtkPlusUsImagingParameters::KEY_DEPTH, this->CalculateDepth());
	//int test = 1;
	//this->CurrentImagingParameters->SetValue<int>("testparemeter", test);

	
	//TODO: Save all parameters in this->CurrentImagingParameters?

	if (this->CurrentImagingParameters->SetDepthMm(this->CalculateDepthMm()) != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}
	if (this->CurrentImagingParameters->SetGainPercent(this->CalculateGain()) != PLUS_SUCCESS)
	{
		return PLUS_FAIL;
	}


	return PLUS_SUCCESS;
}

//Not used for now
PlusStatus vtkPlusBkProFocusOemVideoSource::ProcessParameterValues(/*std::map<std::string, std::string>& parameters*/)
{
	LOG_DEBUG("vtkPlusBkProFocusOemVideoSource::ProcessParameterValues()");
	//TODO: Implement all parameter answers

	vtkPlusDataSource* aSource(NULL);
	if (this->Internal->Channel->GetParameterDataSource(aSource, "GetParameters") == PLUS_SUCCESS)
	{
		LOG_DEBUG("Got ParameterDataSource GetParameters");
		//Only process get parameter once
		if (!aSource->GetCustomProperty("Processed").empty())
		{
			return PLUS_FAIL;
		}
		LOG_DEBUG("Got new GetParameters request");

		if (this->UpdateScannerParameters() != PLUS_SUCCESS)
		{
			//Disable for testing
#ifndef OFFLINE_TESTING
			return PLUS_FAIL;
#endif
		}
		
		if (!aSource->GetCustomProperty(KEY_DEPTH).empty())
		{
			LOG_DEBUG("Depth: " << this->CalculateDepthMm());
			aSource->SetCustomProperty(KEY_DEPTH, PlusCommon::ToString(this->CalculateDepthMm()));
			//aSource->SetCustomProperty(KEY_DEPTH, PlusCommon::ToString(this->CurrentImagingParameters->GetDepthMm()));
			//parameters[KEY_DEPTH] = this->CalculateDepth();
		}
		if (!aSource->GetCustomProperty(KEY_GAIN).empty())
		{
			LOG_DEBUG("Gain: " << this->CalculateGain());
			aSource->SetCustomProperty(KEY_GAIN, PlusCommon::ToString(this->CalculateGain()));
			//aSource->SetCustomProperty(KEY_GAIN, PlusCommon::ToString(this->CurrentImagingParameters->GetGainPercent()));
			
			//parameters[KEY_GAIN] = this->CalculateGain();
		}

		if (!aSource->GetCustomProperty(KEY_PROBE_TYPE).empty())
		{
			aSource->SetCustomProperty(KEY_PROBE_TYPE, PlusCommon::ToString(this->GetProbeType()));
		}

		if (!aSource->GetCustomProperty(KEY_START_DEPTH).empty())
		{
			aSource->SetCustomProperty(KEY_START_DEPTH, PlusCommon::ToString(this->GetStartDepth()));
		}
		if (!aSource->GetCustomProperty(KEY_STOP_DEPTH).empty())
		{
			aSource->SetCustomProperty(KEY_STOP_DEPTH, PlusCommon::ToString(this->GetStopDepth()));
		}

		if (!aSource->GetCustomProperty(KEY_START_LINE_X).empty())
		{
			aSource->SetCustomProperty(KEY_START_LINE_X, PlusCommon::ToString(this->GetStartLineX()));
		}
		if (!aSource->GetCustomProperty(KEY_START_LINE_Y).empty())
		{
			aSource->SetCustomProperty(KEY_START_LINE_Y, PlusCommon::ToString(this->GetStartLineY()));
		}

		if (!aSource->GetCustomProperty(KEY_STOP_LINE_X).empty())
		{
			aSource->SetCustomProperty(KEY_STOP_LINE_X, PlusCommon::ToString(this->GetStopLineX()));
		}
		if (!aSource->GetCustomProperty(KEY_STOP_LINE_Y).empty())
		{
			aSource->SetCustomProperty(KEY_STOP_LINE_Y, PlusCommon::ToString(this->GetStopLineY()));
		}

		if (!aSource->GetCustomProperty(KEY_START_LINE_ANGLE).empty())
		{
			aSource->SetCustomProperty(KEY_START_LINE_ANGLE, PlusCommon::ToString(this->GetStartLineAngle()));
		}
		if (!aSource->GetCustomProperty(KEY_STOP_LINE_ANGLE).empty())
		{
			aSource->SetCustomProperty(KEY_STOP_LINE_ANGLE, PlusCommon::ToString(this->GetStopLineAngle()));
		}

		if (!aSource->GetCustomProperty(KEY_SPACING_X).empty())
		{
			aSource->SetCustomProperty(KEY_SPACING_X, PlusCommon::ToString(this->GetSpacingX()));
		}
		if (!aSource->GetCustomProperty(KEY_SPACING_Y).empty())
		{
			aSource->SetCustomProperty(KEY_SPACING_Y, PlusCommon::ToString(this->GetSpacingY()));
		}

		if (!aSource->GetCustomProperty(KEY_SECTOR_LEFT_PIXELS).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_LEFT_PIXELS, PlusCommon::ToString(this->GetSectorLeftPixels()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_RIGHT_PIXELS).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_RIGHT_PIXELS, PlusCommon::ToString(this->GetSectorRightPixels()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_TOP_PIXELS).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_TOP_PIXELS, PlusCommon::ToString(this->GetSectorTopPixels()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_BOTTOM_PIXELS).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_BOTTOM_PIXELS, PlusCommon::ToString(this->GetSectorBottomPixels()));
		}

		if (!aSource->GetCustomProperty(KEY_SECTOR_LEFT_MM).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_LEFT_MM, PlusCommon::ToString(this->GetSectorLeftMm()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_RIGHT_MM).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_RIGHT_MM, PlusCommon::ToString(this->GetSectorRightMm()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_TOP_MM).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_TOP_MM, PlusCommon::ToString(this->GetSectorTopMm()));
		}
		if (!aSource->GetCustomProperty(KEY_SECTOR_BOTTOM_MM).empty())
		{
			aSource->SetCustomProperty(KEY_SECTOR_BOTTOM_MM, PlusCommon::ToString(this->GetSectorBottomMm()));
		}

		LOG_DEBUG("Add DeviceId: " << this->GetDeviceId());
		//parameters["DeviceId"] = this->GetDeviceId();
		aSource->SetCustomProperty("DeviceId", this->GetDeviceId());

		aSource->SetCustomProperty("Processed", "Read");
	}
	else
	{
		return PLUS_FAIL;
	}

	return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusBkProFocusOemVideoSource::AddParametersToFrameFields()
{
	//Disable for now to test subscribe
	/*if (this->UpdateScannerParameters() != PLUS_SUCCESS)
	{
		//Disable for testing
#ifndef OFFLINE_TESTING
		return PLUS_FAIL;
#endif
	}*/

	vtkPlusUsDevice::InternalUpdate();// Move to beginning of vtkPlusBkProFocusOemVideoSource::InternalUpdate()?

	this->FrameFields[KEY_DEPTH]            = PlusCommon::ToString(this->CalculateDepthMm());
	this->FrameFields[KEY_GAIN]             = PlusCommon::ToString(this->CalculateGain());
	this->FrameFields[KEY_PROBE_TYPE]       = PlusCommon::ToString(this->GetProbeType());
	this->FrameFields[KEY_START_DEPTH]      = PlusCommon::ToString(this->GetStartDepth());
	this->FrameFields[KEY_STOP_DEPTH]       = PlusCommon::ToString(this->GetStopDepth());
	this->FrameFields[KEY_START_LINE_X]     = PlusCommon::ToString(this->GetStartLineX());
	this->FrameFields[KEY_START_LINE_Y]     = PlusCommon::ToString(this->GetStartLineY());
	this->FrameFields[KEY_STOP_LINE_X]      = PlusCommon::ToString(this->GetStopLineX());
	this->FrameFields[KEY_STOP_LINE_Y]      = PlusCommon::ToString(this->GetStopLineY());
	this->FrameFields[KEY_START_LINE_ANGLE] = PlusCommon::ToString(this->GetStartLineAngle());
	this->FrameFields[KEY_STOP_LINE_ANGLE]  = PlusCommon::ToString(this->GetStopLineAngle());
	this->FrameFields[KEY_SPACING_X]		= PlusCommon::ToString(this->GetSpacingX());
	this->FrameFields[KEY_SPACING_Y]		= PlusCommon::ToString(this->GetSpacingY());
	this->FrameFields[KEY_SECTOR_LEFT_PIXELS]	= PlusCommon::ToString(this->GetSectorLeftPixels());
	this->FrameFields[KEY_SECTOR_RIGHT_PIXELS]	= PlusCommon::ToString(this->GetSectorRightPixels());
	this->FrameFields[KEY_SECTOR_TOP_PIXELS]	= PlusCommon::ToString(this->GetSectorTopPixels());
	this->FrameFields[KEY_SECTOR_BOTTOM_PIXELS] = PlusCommon::ToString(this->GetSectorBottomPixels());
	this->FrameFields[KEY_SECTOR_LEFT_MM]		= PlusCommon::ToString(this->GetSectorLeftMm());
	this->FrameFields[KEY_SECTOR_RIGHT_MM]		= PlusCommon::ToString(this->GetSectorRightMm());
	this->FrameFields[KEY_SECTOR_TOP_MM]		= PlusCommon::ToString(this->GetSectorTopMm());
	this->FrameFields[KEY_SECTOR_BOTTOM_MM]		= PlusCommon::ToString(this->GetSectorBottomMm());
	return PLUS_SUCCESS;
}

double vtkPlusBkProFocusOemVideoSource::CalculateDepthMm()
{
	double depth_mm = (StopDepth_m - StartDepth_m) * 1000.0;
	//return PlusCommon::ToString(depth_mm);
	return depth_mm;
}

int vtkPlusBkProFocusOemVideoSource::CalculateGain()
{
	//return PlusCommon::ToString(gain_percent);
	return gain_percent;
}

double vtkPlusBkProFocusOemVideoSource::GetStartDepth()
{
	return StartDepth_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetStopDepth()
{
	return StopDepth_m * 1000.0;
}
double vtkPlusBkProFocusOemVideoSource::GetStartLineX()
{
	return StartLineX_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetStartLineY()
{
	return StartLineY_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetStopLineX()
{
	return StopLineX_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetStopLineY()
{
	return StopLineY_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetStartLineAngle()
{
	return StartLineAngle_rad;
}

double vtkPlusBkProFocusOemVideoSource::GetStopLineAngle()
{
	return StopLineAngle_rad;
}
double vtkPlusBkProFocusOemVideoSource::GetSpacingX()
{
	double spacingX_mm = 1000.0 * (tissueRight_m - tissueLeft_m) / (pixelRight_pix - pixelLeft_pix);
#ifdef OFFLINE_TESTING
	spacingX_mm = 1.0;
#endif
	return spacingX_mm;
}

double vtkPlusBkProFocusOemVideoSource::GetSpacingY()
{
	double spacingY_mm = 1000.0 * (tissueTop_m - tissueBottom_m) / (pixelBottom_pix - pixelTop_pix);
#ifdef OFFLINE_TESTING
	spacingY_mm = 1.0;
#endif
	return spacingY_mm;
}

int vtkPlusBkProFocusOemVideoSource::GetSectorLeftPixels()
{
	return pixelLeft_pix;
}
int vtkPlusBkProFocusOemVideoSource::GetSectorRightPixels()
{
	return pixelRight_pix;
}

int vtkPlusBkProFocusOemVideoSource::GetSectorTopPixels()
{
	return pixelTop_pix;
}

int vtkPlusBkProFocusOemVideoSource::GetSectorBottomPixels()
{
	return pixelBottom_pix;
}

double vtkPlusBkProFocusOemVideoSource::GetSectorLeftMm()
{
	return tissueLeft_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetSectorRightMm()
{
	return tissueRight_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetSectorTopMm()
{
	return tissueTop_m * 1000.0;
}

double vtkPlusBkProFocusOemVideoSource::GetSectorBottomMm()
{
	return tissueBottom_m * 1000.0;
}


vtkPlusBkProFocusOemVideoSource::PROBE_TYPE vtkPlusBkProFocusOemVideoSource::GetProbeType()
{
	if (probePort.compare("A"))
		return probeTypePortA;
	else if (probePort.compare("B"))
		return probeTypePortB;
	else if (probePort.compare("C"))
		return probeTypePortC;
	else if (probePort.compare("M"))
		return probeTypePortM;
	else
		return UNKNOWN;
}