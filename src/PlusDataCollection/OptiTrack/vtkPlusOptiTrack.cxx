/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

// Local includes
#include "PlusConfigure.h"
#include "vtkPlusOptiTrack.h"

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkMatrix4x4.h>
#include <vtkMath.h>
#include <vtkXMLDataElement.h>

// Motive API includes
#include <NPTrackingTools.h>

// NatNet callback function
//TODO: Move this out of the global namespace
void ReceiveDataCallback(sFrameOfMocapData* data, void* pUserData);

vtkStandardNewMacro(vtkPlusOptiTrack);

//----------------------------------------------------------------------------
class vtkPlusOptiTrack::vtkInternal
{
public:
  vtkPlusOptiTrack* External;

  vtkInternal(vtkPlusOptiTrack* external)
    : External(external)
  {
  }

  virtual ~vtkInternal()
  {
  }

  // NatNet client, parameters, and callback function
  NatNetClient* NNClient;
  float UnitsToMm;

  // Motive Files
  std::string ProjectFile;
  std::string CalibrationFile;
  std::vector<std::string> AdditionalRigidBodyFiles;

  // Maps rigid body names to transform names
  std::map<int, PlusTransformName> MapRBNameToTransform;


  // Flag to run Motive in background if user doesn't need GUI
  bool RunMotiveInBackground;

  /*!
  Print user friendly Motive API message to console
  */
  std::string GetMotiveErrorMessage(NPRESULT result);

  void MatchTrackedTools();
};

//-----------------------------------------------------------------------
std::string vtkPlusOptiTrack::vtkInternal::GetMotiveErrorMessage(NPRESULT result)
{
  return std::string(TT_GetResultString(result));
}

//-----------------------------------------------------------------------
void vtkPlusOptiTrack::vtkInternal::MatchTrackedTools()
{
  LOG_TRACE("vtkPlusOptiTrack::vtkInternal::MatchTrackedTools");
  std::string referenceFrame = this->External->GetToolReferenceFrameName();
  this->MapRBNameToTransform.clear();

  sDataDescriptions* dataDescriptions;
  this->NNClient->GetDataDescriptions(&dataDescriptions);
  for (int i = 0; i < dataDescriptions->nDataDescriptions; ++i)
  {
    sDataDescription currentDescription = dataDescriptions->arrDataDescriptions[i];
    if (currentDescription.type == Descriptor_RigidBody)
    {
      // Map the numerical ID of the tracked tool from motive to the name of the tool
      PlusTransformName toolToTracker = PlusTransformName(currentDescription.Data.RigidBodyDescription->szName, referenceFrame);
      this->MapRBNameToTransform[currentDescription.Data.RigidBodyDescription->ID] = toolToTracker;
    }
  }
}


//-----------------------------------------------------------------------
vtkPlusOptiTrack::vtkPlusOptiTrack()
  : vtkPlusDevice()
  , Internal(new vtkInternal(this))
{
  this->FrameNumber = 0;
  // always uses NatNet's callback to update

  this->InternalUpdateRate = 120;
  this->StartThreadForInternalUpdates = false;
}

//-------------------------------------------------------------------------
vtkPlusOptiTrack::~vtkPlusOptiTrack() 
{
  delete Internal;
  Internal = nullptr;
}

//-------------------------------------------------------------------------
void vtkPlusOptiTrack::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::ReadConfiguration(vtkXMLDataElement* rootConfigElement)
{
  LOG_TRACE("vtkPlusOptiTrack::ReadConfiguration")
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  XML_READ_STRING_ATTRIBUTE_NONMEMBER_REQUIRED(ProjectFile, this->Internal->ProjectFile, deviceConfig);
  XML_READ_BOOL_ATTRIBUTE_NONMEMBER_REQUIRED(RunMotiveInBackground, this->Internal->RunMotiveInBackground, deviceConfig);

  XML_FIND_NESTED_ELEMENT_REQUIRED(dataSourcesElement, deviceConfig, "DataSources");
  for (int nestedElementIndex = 0; nestedElementIndex < dataSourcesElement->GetNumberOfNestedElements(); nestedElementIndex++)
  {
    vtkXMLDataElement* toolDataElement = dataSourcesElement->GetNestedElement(nestedElementIndex);
    if (STRCASECMP(toolDataElement->GetName(), "DataSource") != 0)
    {
      // if this is not a data source element, skip it
      continue;
    }
    if (toolDataElement->GetAttribute("Type") != NULL && STRCASECMP(toolDataElement->GetAttribute("Type"), "Tool") != 0)
    {
      // if this is not a Tool element, skip it
      continue;
    }

    std::string toolId(toolDataElement->GetAttribute("Id"));
    if (toolId.empty())
    {
      // tool doesn't have ID needed to generate transform
      LOG_ERROR("Failed to initialize OptiTrack tool: DataSource Id is missing. This should be the name of the Motive Rigid Body that tracks the tool.");
      continue;
    }

    if (toolDataElement->GetAttribute("RigidBodyFile") != NULL)
    {
      // this tool has an associated rigid body definition
      const char* rigidBodyFile = toolDataElement->GetAttribute("RigidBodyFile");
      this->Internal->AdditionalRigidBodyFiles.push_back(rigidBodyFile);
    }
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
  LOG_TRACE("vtkPlusOptiTrack::WriteConfiguration");
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::Probe()
{
  LOG_TRACE("vtkPlusOptiTrack::Probe");
  return PLUS_SUCCESS;
}

//-------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalConnect()
{
  LOG_TRACE("vtkPlusOptiTrack::InternalConnect");
  if (this->Internal->RunMotiveInBackground == true)
  {
    // RUN MOTIVE IN BACKGROUND
    // initialize the API
    if (TT_Initialize() != NPRESULT_SUCCESS)
    {
      LOG_ERROR("Failed to start Motive.");
      return PLUS_FAIL;
    }

    // pick up recently-arrived cameras
    TT_Update();

    // open project file
    std::string projectFilePath = vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationPath(this->Internal->ProjectFile);
    NPRESULT ttpLoad = TT_LoadProject(projectFilePath.c_str());
    if (ttpLoad != NPRESULT_SUCCESS)
    {
      LOG_ERROR("Failed to load Motive project file. Motive error: " << TT_GetResultString(ttpLoad));
      return PLUS_FAIL;
    }

    // add any additional rigid body files to project
    std::string rbFilePath;
    for (auto it = this->Internal->AdditionalRigidBodyFiles.begin(); it != this->Internal->AdditionalRigidBodyFiles.end(); it++)
    {
      rbFilePath = vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationPath(*it);
      NPRESULT addRBResult = TT_AddRigidBodies(rbFilePath.c_str());
      if (addRBResult != NPRESULT_SUCCESS)
      {
        LOG_ERROR("Failed to load rigid body file located at: " << rbFilePath << ". Motive error message: " << this->Internal->GetMotiveErrorMessage(addRBResult));
        return PLUS_FAIL;
      }
    }

    LOG_INFO("\n---------------------------------MOTIVE SETTINGS--------------------------------")
      // list connected cameras
      LOG_INFO("Connected cameras:")
      for (int i = 0; i < TT_CameraCount(); i++)
      {
        LOG_INFO(i << ": " << TT_CameraName(i));
      }
    // list project file
    LOG_INFO("\nUsing Motive project file located at:\n" << projectFilePath);
    // list rigid bodies
    LOG_INFO("\nTracked rigid bodies:");
    for (int i = 0; i < TT_RigidBodyCount(); ++i)
    {
      LOG_INFO(TT_RigidBodyName(i));
    }
    LOG_INFO("--------------------------------------------------------------------------------\n");

    this->StartThreadForInternalUpdates = true;
  }

  // CONFIGURE NATNET CLIENT
  this->Internal->NNClient = new NatNetClient(ConnectionType_Multicast);
  this->Internal->NNClient->SetVerbosityLevel(Verbosity_None);
  this->Internal->NNClient->SetVerbosityLevel(Verbosity_Warning);
  this->Internal->NNClient->SetDataCallback(ReceiveDataCallback, this);

  int retCode = this->Internal->NNClient->Initialize("127.0.0.1", "127.0.0.1");

  void* response;
  int nBytes;
  if (this->Internal->NNClient->SendMessageAndWait("UnitsToMillimeters", &response, &nBytes) == ErrorCode_OK)
  {
    this->Internal->UnitsToMm = (*(float*)response);
  }
  else
  {
    // Fail if motive is not running
    LOG_ERROR("Failed to connect to Motive. Please either set RunMotiveInBackground=TRUE or ensure that Motive is running and streaming is enabled.");
    return PLUS_FAIL;
  }

  // verify all rigid bodies in Motive have unique names
  std::set<std::string> rigidBodies;
  sDataDescriptions* dataDescriptions;
  this->Internal->NNClient->GetDataDescriptions(&dataDescriptions);
  for (int i = 0; i < dataDescriptions->nDataDescriptions; ++i)
  {
    sDataDescription currentDescription = dataDescriptions->arrDataDescriptions[i];
    if (currentDescription.type == Descriptor_RigidBody)
    {
      // Map the numerical ID of the tracked tool from motive to the name of the tool
      if (!rigidBodies.insert(currentDescription.Data.RigidBodyDescription->szName).second)
      {
        LOG_ERROR("Duplicate rigid bodies with name: " << currentDescription.Data.RigidBodyDescription->szName);
        return PLUS_FAIL;
      }
    }
  }

  return PLUS_SUCCESS; 
}

//-------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalDisconnect()
{
  LOG_TRACE("vtkPlusOptiTrack::InternalDisconnect")
  if (this->Internal->RunMotiveInBackground)
  {
    TT_Shutdown();
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalStartRecording()
{
  LOG_TRACE("vtkPlusOptiTrack::InternalStartRecording")
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalStopRecording()
{
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalUpdate()
{
  LOG_TRACE("vtkPlusOptiTrack::InternalUpdate");
  // internal update is only called if usign Motive API
  TT_Update();
  return PLUS_SUCCESS;
}

//-------------------------------------------------------------------------
PlusStatus vtkPlusOptiTrack::InternalCallback(sFrameOfMocapData* data)
{
  LOG_TRACE("vtkPlusOptiTrack::InternalCallback");

  this->Internal->MatchTrackedTools();

  const double unfilteredTimestamp = vtkPlusAccurateTimer::GetSystemTime();

  sDataDescriptions* dataDescriptions;
  this->Internal->NNClient->GetDataDescriptions(&dataDescriptions);

  int numberOfRigidBodies = data->nRigidBodies;
  sRigidBodyData* rigidBodies = data->RigidBodies;

  if (data->nOtherMarkers)
  {
    LOG_WARNING("vtkPlusOptiTrack::InternalCallback: Untracked markers detected. Check for interference or make sure that the entire tool is in view")
  }

  // identity transform for tools out of view
  vtkSmartPointer<vtkMatrix4x4> rigidBodyToTrackerMatrix = vtkSmartPointer<vtkMatrix4x4>::New();

  for (int rigidBodyId = 0; rigidBodyId < numberOfRigidBodies; ++rigidBodyId)
  {
    // TOOL IN VIEW
    rigidBodyToTrackerMatrix->Identity();
    sRigidBodyData currentRigidBody = rigidBodies[rigidBodyId];

    if (currentRigidBody.MeanError != 0)
    {
      // convert translation to mm
      double translation[3] = { currentRigidBody.x * this->Internal->UnitsToMm, currentRigidBody.y * this->Internal->UnitsToMm, currentRigidBody.z * this->Internal->UnitsToMm };

      // convert rotation from quaternion to 3x3 matrix
      double quaternion[4] = { currentRigidBody.qw, currentRigidBody.qx, currentRigidBody.qy, currentRigidBody.qz };
      double rotation[3][3] = { 0,0,0, 0,0,0, 0,0,0 };
      vtkMath::QuaternionToMatrix3x3(quaternion, rotation);

      // construct the transformation matrix from the rotation and translation components
      for (int i = 0; i < 3; ++i)
      {
        for (int j = 0; j < 3; ++j)
        {
          rigidBodyToTrackerMatrix->SetElement(i, j, rotation[i][j]);
        }
        rigidBodyToTrackerMatrix->SetElement(i, 3, translation[i]);
      }

      // make sure the tool was specified in the Config file
      PlusTransformName toolToTracker = this->Internal->MapRBNameToTransform[currentRigidBody.ID];
      ToolTimeStampedUpdate(toolToTracker.GetTransformName(), rigidBodyToTrackerMatrix, TOOL_OK, FrameNumber, unfilteredTimestamp);
    }
    else
    {
      // TOOL OUT OF VIEW
      PlusTransformName toolToTracker = this->Internal->MapRBNameToTransform[currentRigidBody.ID];
      ToolTimeStampedUpdate(toolToTracker.GetTransformName(), rigidBodyToTrackerMatrix, TOOL_OUT_OF_VIEW, FrameNumber, unfilteredTimestamp);
    }
    
  }

  this->FrameNumber++;
  LOG_INFO("Frame: " << this->FrameNumber);
  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void ReceiveDataCallback(sFrameOfMocapData* data, void* pUserData)
{
  vtkPlusOptiTrack* internalCallback = (vtkPlusOptiTrack*)pUserData;
  internalCallback->InternalCallback(data);
}