// Copyright 2017 Google Inc.

#include "GoogleARCoreAPI.h"

#include "Misc/EngineVersion.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace 
{
#if PLATFORM_ANDROID
	static const FMatrix ARCoreToUnrealTransform = FMatrix(
		FPlane(0.0f, 0.0f, -1.0f, 0.0f),
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	static const FMatrix ARCoreToUnrealTransformInverse = ARCoreToUnrealTransform.InverseFast();

	EGoogleARCoreAPIStatus ToARCoreAPIStatus(ArStatus Status)
	{
		return static_cast<EGoogleARCoreAPIStatus>(Status);
	}

	FTransform ARCorePoseToUnrealTransform(ArPose* ArPoseHandle, const ArSession* SessionHandle, float WorldToMeterScale)
	{
		FMatrix ARCorePoseMatrix;
		ArPose_getMatrix(SessionHandle, ArPoseHandle, ARCorePoseMatrix.M[0]);
		FTransform Result = FTransform(ARCoreToUnrealTransform * ARCorePoseMatrix * ARCoreToUnrealTransformInverse);
		Result.SetLocation(Result.GetLocation() * WorldToMeterScale);

		return Result;
	}

	void UnrealTransformToARCorePose(const FTransform& UnrealTransform, const ArSession* SessionHandle, ArPose** OutARPose, float WorldToMeterScale)
	{	
		check(OutARPose);

		FMatrix UnrealPoseMatrix = UnrealTransform.ToMatrixNoScale();
		UnrealPoseMatrix.SetOrigin(UnrealPoseMatrix.GetOrigin() / WorldToMeterScale);
		FMatrix ARCorePoseMatrix = ARCoreToUnrealTransformInverse * UnrealPoseMatrix * ARCoreToUnrealTransform;

		FVector ArPosePosition = ARCorePoseMatrix.GetOrigin();
		FQuat ArPoseRotation = ARCorePoseMatrix.ToQuat();
		float ArPoseData[7] = { ArPoseRotation.X, ArPoseRotation.Y, ArPoseRotation.Z, ArPoseRotation.W, ArPosePosition.X, ArPosePosition.Y, ArPosePosition.Z };
		ArPose_create(SessionHandle, ArPoseData, OutARPose);
	}
#endif
	inline bool CheckIsSessionValid(FString TypeName, const TWeakPtr<FGoogleARCoreSession>& SessionPtr)
	{
		if (!SessionPtr.IsValid())
		{
			UE_LOG(LogGoogleARCoreAPI, Error, TEXT("Invalid %s: This may caused by ARCore session got recreated due to FATAL error."), *TypeName);
			return false;
		}
#if PLATFORM_ANDROID
		if (SessionPtr.Pin()->GetHandle() == nullptr)
		{
			return false;
		}
#endif
		return true;
	}
}

extern "C" 
{
#if PLATFORM_ANDROID
void ArSession_reportEngineType(ArSession* session, const char* engine_type, const char* engine_version);
#endif
}

/****************************************/
/*         FGoogleARCoreSession         */
/****************************************/
FGoogleARCoreSession::FGoogleARCoreSession()
	: SessionCreateStatus(EGoogleARCoreAPIStatus::AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE)
	, LatestFrame(nullptr)
	, UObjectManager(nullptr)
	, CameraTextureId(0)
	, CachedWorldToMeterScale(100.0f)
{
	// Create Android ARSession handle.
	LatestFrame = new FGoogleARCoreFrame(this);
#if PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "getApplicationContext", "()Landroid/content/Context;", false);
	jobject ApplicationContext = FJavaWrapper::CallObjectMethod(Env, FAndroidApplication::GetGameActivityThis(), Method);
	check(Env);
	check(ApplicationContext);

	SessionCreateStatus = ToARCoreAPIStatus(ArSession_create(Env, ApplicationContext, &SessionHandle));
	if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCoreAPI, Error, TEXT("ArSession_create returns with error: %d"), SessionCreateStatus);
		return;
	}

	ArConfig_create(SessionHandle, &ConfigHandle);
	LatestFrame->Init();

	static bool ARCoreAnalyticsReported = false;
	if (!ARCoreAnalyticsReported)
	{
		ArSession_reportEngineType(SessionHandle, "Unreal Engine", TCHAR_TO_ANSI(*FEngineVersion::Current().ToString()));
		ARCoreAnalyticsReported = true;
	}
#endif
}

FGoogleARCoreSession::~FGoogleARCoreSession()
{
	delete LatestFrame;
	if (UObjectManager->IsValidLowLevel())
	{
		UObjectManager->RemoveFromRoot();
	}

#if PLATFORM_ANDROID
	ArSession_destroy(SessionHandle);
	ArConfig_destroy(ConfigHandle);
#endif
}

// Properties
EGoogleARCoreAPIStatus FGoogleARCoreSession::GetSessionCreateStatus()
{
	return SessionCreateStatus;
}

UGoogleARCoreUObjectManager* FGoogleARCoreSession::GetUObjectManager()
{
	return UObjectManager;
}

float FGoogleARCoreSession::GetWorldToMeterScale()
{
	return CachedWorldToMeterScale;
}
#if PLATFORM_ANDROID
ArSession* FGoogleARCoreSession::GetHandle()
{
	return SessionHandle;
}
#endif

// Session lifecycle
bool FGoogleARCoreSession::IsConfigSupported(FGoogleARCoreSessionConfig Config)
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return false;
	}

	ArConfig* NewConfigHandle = nullptr;
	ArConfig_create(SessionHandle, &NewConfigHandle);

	ArConfig_setLightEstimationMode(SessionHandle, NewConfigHandle, static_cast<ArLightEstimationMode>(Config.LightEstimationMode));
	ArConfig_setPlaneFindingMode(SessionHandle, NewConfigHandle, static_cast<ArPlaneFindingMode>(Config.PlaneDetectionMode));
	ArConfig_setUpdateMode(SessionHandle, NewConfigHandle, static_cast<ArUpdateMode>(Config.FrameUpdateMode));

	ArStatus Status = ArSession_checkSupported(SessionHandle, NewConfigHandle);
	ArConfig_destroy(NewConfigHandle);

	return Status == ArStatus::AR_SUCCESS;
#endif
	return false;
}

EGoogleARCoreAPIStatus FGoogleARCoreSession::ConfigSession(FGoogleARCoreSessionConfig Config)
{
	SessionConfig = Config;
	EGoogleARCoreAPIStatus ConfigStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_FATAL;
	}
	ArConfig_setLightEstimationMode(SessionHandle, ConfigHandle, static_cast<ArLightEstimationMode>(Config.LightEstimationMode));
	ArConfig_setPlaneFindingMode(SessionHandle, ConfigHandle, static_cast<ArPlaneFindingMode>(Config.PlaneDetectionMode));
	ArConfig_setUpdateMode(SessionHandle, ConfigHandle, static_cast<ArUpdateMode>(Config.FrameUpdateMode));

	ConfigStatus = ToARCoreAPIStatus(ArSession_configure(SessionHandle, ConfigHandle));
#endif
	return ConfigStatus;
}

EGoogleARCoreAPIStatus FGoogleARCoreSession::Resume()
{
	EGoogleARCoreAPIStatus ResumeStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_FATAL;
	}

	ResumeStatus = ToARCoreAPIStatus(ArSession_resume(SessionHandle));
#endif
	return ResumeStatus;
}

EGoogleARCoreAPIStatus FGoogleARCoreSession::Pause()
{
	EGoogleARCoreAPIStatus PauseStatue = EGoogleARCoreAPIStatus::AR_SUCCESS;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_FATAL;
	}

	PauseStatue = ToARCoreAPIStatus(ArSession_pause(SessionHandle));
#endif
	return PauseStatue;
}

EGoogleARCoreAPIStatus FGoogleARCoreSession::Update(float WorldToMeterScale)
{
	EGoogleARCoreAPIStatus UpdateStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_FATAL;
	}

	UpdateStatus = ToARCoreAPIStatus(ArSession_update(SessionHandle, LatestFrame->FrameHandle));
#endif

	CachedWorldToMeterScale = WorldToMeterScale;
	LatestFrame->Update(WorldToMeterScale);

	return UpdateStatus;
}

const FGoogleARCoreFrame* FGoogleARCoreSession::GetLatestFrame()
{
	return LatestFrame;
}

void FGoogleARCoreSession::SetCameraTextureId(uint32_t TextureId)
{
	CameraTextureId = TextureId;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}
	ArSession_setCameraTextureName(SessionHandle, TextureId);
#endif
}

void FGoogleARCoreSession::SetDisplayGeometry(int Rotation, int Width, int Height)
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}
	ArSession_setDisplayGeometry(SessionHandle, Rotation, Width, Height);
#endif
}

// Anchors and Planes
EGoogleARCoreAPIStatus FGoogleARCoreSession::CreateARAnchor(FTransform AnchorWorldPose, UGoogleARCoreAnchor*& OutAnchor)
{
	EGoogleARCoreAPIStatus AnchorCreateStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
	OutAnchor = nullptr;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED;
	}
	ArPose* ARPoseHandle = nullptr;
	UnrealTransformToARCorePose(AnchorWorldPose, SessionHandle, &ARPoseHandle, CachedWorldToMeterScale);

	ArAnchor* NewAnchorHandle = nullptr;
	AnchorCreateStatus = ToARCoreAPIStatus(ArSession_acquireNewAnchor(SessionHandle, ARPoseHandle, &NewAnchorHandle));
	ArPose_destroy(ARPoseHandle);

	if (AnchorCreateStatus == EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		OutAnchor = CreateUEAnchor(NewAnchorHandle, AnchorWorldPose);
	}
#endif
	return AnchorCreateStatus;
}

EGoogleARCoreAPIStatus FGoogleARCoreSession::CreateARAnchorFromHitTestResult(FGoogleARCoreTraceResult HitTestResult, UGoogleARCoreAnchor*& OutAnchor)
{
	EGoogleARCoreAPIStatus AnchorCreateStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
	OutAnchor = nullptr;
	checkf(HitTestResult.TrackableObject, TEXT("Trackable in FGoogleARCoreTraceResult is null."));

#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED;
	}

	ArAnchor* NewAnchorHandle = nullptr;
	ArTrackable* TrackableHanlde = HitTestResult.TrackableObject->GetTrackableHandle();
	ArPose *Pose = nullptr;
	ArPose_create(SessionHandle, nullptr, &Pose);
	UnrealTransformToARCorePose(HitTestResult.HitPointPose, SessionHandle, &Pose, CachedWorldToMeterScale);
	AnchorCreateStatus = ToARCoreAPIStatus(ArTrackable_acquireNewAnchor(SessionHandle, HitTestResult.TrackableObject->GetTrackableHandle(), Pose, &NewAnchorHandle));
	ArPose_destroy(Pose);

	if (AnchorCreateStatus == EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		OutAnchor = CreateUEAnchor(NewAnchorHandle, HitTestResult.HitPointPose);
	}
#endif
	return AnchorCreateStatus;
}

#if PLATFORM_ANDROID
UGoogleARCoreAnchor* FGoogleARCoreSession::CreateUEAnchor(ArAnchor* AnchorHandle, FTransform AnchorWorldPose)
{
	UGoogleARCoreAnchor* NewAnchor = NewObject<UGoogleARCoreAnchor>();
	NewAnchor->AnchorHandle = AnchorHandle;
	NewAnchor->Pose = AnchorWorldPose;
	NewAnchor->TrackingState = EGoogleARCoreTrackingState::Tracking;

	UObjectManager->AllAnchors.Add(NewAnchor);
	UObjectManager->AnchorHandleMap.Add(AnchorHandle, NewAnchor);

	return NewAnchor;
}
#endif

void FGoogleARCoreSession::DetachAnchor(UGoogleARCoreAnchor* Anchor)
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	Anchor->TrackingState = EGoogleARCoreTrackingState::Stopped;
	ArAnchor_detach(SessionHandle, Anchor->AnchorHandle);

	ArAnchor_release(Anchor->AnchorHandle);

	UObjectManager->AnchorHandleMap.Remove(Anchor->AnchorHandle);
#endif
	UObjectManager->AllAnchors.Remove(Anchor);
}

void FGoogleARCoreSession::GetAllAnchors(TArray<UGoogleARCoreAnchor*>& OutAnchors) const
{
	OutAnchors = UObjectManager->AllAnchors;
}

/****************************************/
/*         FGoogleARCoreFrame           */
/****************************************/
FGoogleARCoreFrame::FGoogleARCoreFrame(FGoogleARCoreSession* InSession)
	: Session(InSession)
	, LatestCameraPose(FTransform::Identity)
	, LatestCameraTimestamp(0)
	, LatestCameraTrackingState(EGoogleARCoreTrackingState::Stopped)
	, LatestPointCloudStatus(EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED)
	, LatestImageMetadataStatus(EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED)
{
}

FGoogleARCoreFrame::~FGoogleARCoreFrame()
{
#if PLATFORM_ANDROID
	ArFrame_destroy(FrameHandle);
	ArPose_destroy(SketchPoseHandle);
#endif
}

void FGoogleARCoreFrame::Init()
{
#if PLATFORM_ANDROID
	if (Session->GetHandle())
	{
		SessionHandle = Session->GetHandle();
		ArFrame_create(SessionHandle, &FrameHandle);
		ArPose_create(SessionHandle, nullptr, &SketchPoseHandle);
	}
#endif
}



void FGoogleARCoreFrame::Update(float WorldToMeterScale)
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	ArCamera_release(CameraHandle);
	ArFrame_acquireCamera(SessionHandle, FrameHandle, &CameraHandle);

	ArCamera_getDisplayOrientedPose(SessionHandle, CameraHandle, SketchPoseHandle);

	ArTrackingState ARCoreTrackingState;
	ArCamera_getTrackingState(SessionHandle, CameraHandle, &ARCoreTrackingState);
	LatestCameraTrackingState = static_cast<EGoogleARCoreTrackingState>(ARCoreTrackingState);

	if (LatestCameraTrackingState == EGoogleARCoreTrackingState::Tracking)
	{
		int64_t FrameTimestamp = 0;
		ArFrame_getTimestamp(SessionHandle, FrameHandle, &FrameTimestamp);
		LatestCameraPose = ARCorePoseToUnrealTransform(SketchPoseHandle, SessionHandle, WorldToMeterScale);
		LatestCameraTimestamp = FrameTimestamp;
		// Update Point Cloud
		UGoogleARCorePointCloud* LatestPointCloud = Session->GetUObjectManager()->LatestPointCloud;
		LatestPointCloud->bIsUpdated = false;
		int64 PreviousTimeStamp = LatestPointCloud->GetUpdateTimestamp();
		ArPointCloud_release(LatestPointCloud->PointCloudHandle);
		LatestPointCloud->PointCloudHandle = nullptr;
		LatestPointCloudStatus = ToARCoreAPIStatus(ArFrame_acquirePointCloud(SessionHandle, FrameHandle, &LatestPointCloud->PointCloudHandle));

		if (PreviousTimeStamp != LatestPointCloud->GetUpdateTimestamp())
		{
			LatestPointCloud->bIsUpdated = true;
		}
	}

	// Update Image Metadata
	ArImageMetadata_release(LatestImageMetadata);
	LatestImageMetadata = nullptr;
	LatestImageMetadataStatus = ToARCoreAPIStatus(ArFrame_acquireImageMetadata(SessionHandle, FrameHandle, &LatestImageMetadata));

	// Update Anchors
	ArAnchorList* UpdatedAnchorListHandle = nullptr;
	ArAnchorList_create(SessionHandle, &UpdatedAnchorListHandle);
	ArFrame_getUpdatedAnchors(SessionHandle, FrameHandle, UpdatedAnchorListHandle);
	int AnchorListSize = 0;
	ArAnchorList_getSize(SessionHandle, UpdatedAnchorListHandle, &AnchorListSize);

	UpdatedAnchors.Empty();
	for (int i = 0; i < AnchorListSize; i++)
	{
		ArAnchor* AnchorHandle = nullptr;
		ArAnchorList_acquireItem(SessionHandle, UpdatedAnchorListHandle, i, &AnchorHandle);

		ArTrackingState AnchorTrackingState;
		ArAnchor_getTrackingState(SessionHandle, AnchorHandle, &AnchorTrackingState);
		if (!Session->GetUObjectManager()->AnchorHandleMap.Contains(AnchorHandle))
		{
			continue;
		}
		UGoogleARCoreAnchor* AnchorObject = Session->GetUObjectManager()->AnchorHandleMap[AnchorHandle];
		AnchorObject->TrackingState = static_cast<EGoogleARCoreTrackingState>(AnchorTrackingState);

		if (AnchorObject->TrackingState == EGoogleARCoreTrackingState::Tracking)
		{
			ArAnchor_getPose(SessionHandle, AnchorHandle, SketchPoseHandle);
			AnchorObject->Pose = ARCorePoseToUnrealTransform(SketchPoseHandle, SessionHandle, WorldToMeterScale);
		}
		UpdatedAnchors.Add(AnchorObject);

		ArAnchor_release(AnchorHandle);
	}
	ArAnchorList_destroy(UpdatedAnchorListHandle);
#endif
}

FTransform FGoogleARCoreFrame::GetCameraPose() const
{
	return LatestCameraPose;
}

int64 FGoogleARCoreFrame::GetCameraTimestamp() const
{
	return LatestCameraTimestamp;
}

EGoogleARCoreTrackingState FGoogleARCoreFrame::GetCameraTrackingState() const
{
	return LatestCameraTrackingState;
}

void FGoogleARCoreFrame::GetUpdatedAnchors(TArray<UGoogleARCoreAnchor*>& OutUpdatedAnchors) const
{
	OutUpdatedAnchors = UpdatedAnchors;
}

void FGoogleARCoreFrame::ARLineTrace(const FVector2D& ScreenPosition, TArray<FGoogleARCoreTraceResult>& OutHitResults) const
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	ArHitResultList *HitResultList = nullptr;
	ArHitResult *HitResultHandle = nullptr;
	ArPose *Pose = nullptr;
	int32_t HitResultCount = 0;

	ArHitResultList_create(SessionHandle, &HitResultList);
	ArPose_create(SessionHandle, nullptr, &Pose);

	ArFrame_hitTest(SessionHandle, FrameHandle, ScreenPosition.X, ScreenPosition.Y, HitResultList);

	ArHitResultList_getSize(SessionHandle, HitResultList, &HitResultCount);

	// HitResultHandle will be freed when the struct got destroyed. 
	ArHitResult_create(SessionHandle, &HitResultHandle);
	for(int32_t i = 0; i < HitResultCount; i++) {

		FGoogleARCoreTraceResult UEHitResult;

		ArHitResultList_getItem(SessionHandle, HitResultList, i, HitResultHandle);
		
		ArHitResult_getDistance(SessionHandle, HitResultHandle, &UEHitResult.Distance);
		UEHitResult.Distance *= Session->GetWorldToMeterScale();

		ArHitResult_getHitPose(SessionHandle, HitResultHandle, SketchPoseHandle);
		UEHitResult.HitPointPose = ARCorePoseToUnrealTransform(SketchPoseHandle, SessionHandle, Session->GetWorldToMeterScale());
		
		ArTrackable* TrackableHandle = nullptr;
		ArHitResult_acquireTrackable(SessionHandle, HitResultHandle, &TrackableHandle);
		UEHitResult.TrackableObject = Session->GetUObjectManager()->GetTrackableFromHandle<UGoogleARCoreTrackable>(TrackableHandle, Session);
		
		OutHitResults.Add(UEHitResult);
	}
	ArHitResult_destroy(HitResultHandle);
	ArPose_destroy(Pose);
	ArHitResultList_destroy(HitResultList);
#endif
}

bool FGoogleARCoreFrame::IsDisplayRotationChanged() const
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return false;
	}

	int Result = 0;
	ArFrame_getDisplayGeometryChanged(SessionHandle, FrameHandle, &Result);
	return Result == 0 ? false : true;
#endif
	return false;
}

FMatrix FGoogleARCoreFrame::GetProjectionMatrix() const
{
	FMatrix ProjectionMatrix;

#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return ProjectionMatrix;
	}

	ArCamera_getProjectionMatrix(SessionHandle, CameraHandle, GNearClippingPlane, 100.0f, ProjectionMatrix.M[0]);

	// Unreal uses the infinite far plane project matrix.
	ProjectionMatrix.M[2][2] = 0.0f;
	ProjectionMatrix.M[2][3] = 1.0f;
	ProjectionMatrix.M[3][2] = GNearClippingPlane;
#endif
	return ProjectionMatrix;
}

void FGoogleARCoreFrame::TransformDisplayUvCoords(const TArray<float>& UvCoords, TArray<float>& OutUvCoords) const
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	OutUvCoords.SetNumZeroed(8);
	ArFrame_transformDisplayUvCoords(SessionHandle, FrameHandle, 8, UvCoords.GetData(), OutUvCoords.GetData());
#endif
}

FGoogleARCoreLightEstimate FGoogleARCoreFrame::GetLightEstimate() const
{
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return FGoogleARCoreLightEstimate();
	}

	ArLightEstimate *LightEstimateHandle = nullptr;
	ArLightEstimate_create(SessionHandle, &LightEstimateHandle);
	ArFrame_getLightEstimate(SessionHandle, FrameHandle, LightEstimateHandle);

	FGoogleARCoreLightEstimate LightEstimate;
	ArLightEstimate_getPixelIntensity(SessionHandle, LightEstimateHandle, &LightEstimate.PixelIntensity);

	ArLightEstimateState LightEstimateState;
	ArLightEstimate_getState(SessionHandle, LightEstimateHandle, &LightEstimateState);

	LightEstimate.bIsValid = (LightEstimateState == AR_LIGHT_ESTIMATE_STATE_VALID) ? true : false;

	ArLightEstimate_destroy(LightEstimateHandle);

	return LightEstimate;
#else
	return FGoogleARCoreLightEstimate();
#endif
}

EGoogleARCoreAPIStatus FGoogleARCoreFrame::GetPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	OutLatestPointCloud = Session->GetUObjectManager()->LatestPointCloud;
	return LatestPointCloudStatus;
}

EGoogleARCoreAPIStatus FGoogleARCoreFrame::AcquirePointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	OutLatestPointCloud = nullptr;
	EGoogleARCoreAPIStatus AcquirePointCloudStatus = EGoogleARCoreAPIStatus::AR_SUCCESS;
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED;
	}

	ArPointCloud* PointCloudHandle = nullptr;
	AcquirePointCloudStatus = ToARCoreAPIStatus(ArFrame_acquirePointCloud(SessionHandle, FrameHandle, &PointCloudHandle));

	if (AcquirePointCloudStatus == EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		OutLatestPointCloud = NewObject<UGoogleARCorePointCloud>();
		OutLatestPointCloud->Session = Session->AsShared();
		OutLatestPointCloud->PointCloudHandle = PointCloudHandle;
		OutLatestPointCloud->bIsUpdated = true;
	}
	else
	{
		UE_LOG(LogGoogleARCoreAPI, Error, TEXT("AcquirePointCloud failed due to resource exhausted!"));
	}
#endif
	return AcquirePointCloudStatus;
}

#if PLATFORM_ANDROID
EGoogleARCoreAPIStatus FGoogleARCoreFrame::GetCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const
{
	if (SessionHandle == nullptr)
	{
		return EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED;
	}

	ArImageMetadata_getNdkCameraMetadata(SessionHandle, LatestImageMetadata, &OutCameraMetadata);
	
	return LatestImageMetadataStatus;
}
#endif

TSharedPtr<FGoogleARCoreSession> FGoogleARCoreSession::CreateARCoreSession()
{
	TSharedPtr<FGoogleARCoreSession> NewSession = MakeShared<FGoogleARCoreSession>();

	UGoogleARCoreUObjectManager* UObjectManager = NewObject<UGoogleARCoreUObjectManager>();
	UObjectManager->LatestPointCloud = NewObject<UGoogleARCorePointCloud>();
	UObjectManager->LatestPointCloud->Session = NewSession;
	UObjectManager->AddToRoot();

	NewSession->UObjectManager = UObjectManager;
	return NewSession;
}

/****************************************/
/*       UGoogleARCoreTrackable         */
/****************************************/
UGoogleARCoreTrackable::~UGoogleARCoreTrackable()
{
#if PLATFORM_ANDROID
	ArTrackable_release(TrackableHandle);
#endif
}

EGoogleARCoreTrackingState UGoogleARCoreTrackable::GetTrackingState()
{
	EGoogleARCoreTrackingState TrackingState = EGoogleARCoreTrackingState::Stopped;
	if (CheckIsSessionValid("ARCoreTrackable", Session))
	{
#if PLATFORM_ANDROID
		ArTrackingState ARTrackingState = ArTrackingState::AR_TRACKING_STATE_STOPPED;
		ArTrackable_getTrackingState(Session.Pin()->GetHandle(), TrackableHandle, &ARTrackingState);
		TrackingState = static_cast<EGoogleARCoreTrackingState>(ARTrackingState);
#endif
	}
	return TrackingState;
}

UClass* UGoogleARCoreTrackable::GetTrackableType()
{
	if (CheckIsSessionValid("ARCoreTrackable", Session))
	{
#if PLATFORM_ANDROID
		ArTrackableType TrackableType = ArTrackableType::AR_TRACKABLE_NOT_VALID;
		ArTrackable_getType(Session.Pin()->GetHandle(), TrackableHandle, &TrackableType);
		if (TrackableType == ArTrackableType::AR_TRACKABLE_PLANE)
		{
			return UGoogleARCorePlane::StaticClass();
		}
		else if (TrackableType == ArTrackableType::AR_TRACKABLE_POINT)
		{
			return UGoogleARCoreTrackablePoint::StaticClass();
		}
#endif
	}
	return UGoogleARCoreTrackable::StaticClass();
}

/****************************************/
/*     UGoogleARCoreTrackablePoint      */
/****************************************/
#if PLATFORM_ANDROID
ArPoint* UGoogleARCoreTrackablePoint::GetPointHandle() const
{
	return reinterpret_cast<ArPoint*>(TrackableHandle);
}
#endif

FTransform UGoogleARCoreTrackablePoint::GetPose()
{
	FTransform PointPose;
	if (CheckIsSessionValid("ARCoreTrackablePoint", Session))
	{
#if PLATFORM_ANDROID
		ArPose* ARPoseHandle = nullptr;
		ArPose_create(Session.Pin()->GetHandle(), nullptr, &ARPoseHandle);
		ArPoint_getPose(Session.Pin()->GetHandle(), GetPointHandle(), ARPoseHandle);
		PointPose = ARCorePoseToUnrealTransform(ARPoseHandle, Session.Pin()->GetHandle(), Session.Pin()->GetWorldToMeterScale());
		ArPose_destroy(ARPoseHandle);
#endif
	}
	return PointPose;
}

/****************************************/
/*           UGoogleARCorePlane         */
/****************************************/
#if PLATFORM_ANDROID
ArPlane* UGoogleARCorePlane::GetPlaneHandle() const
{
	return reinterpret_cast<ArPlane*>(TrackableHandle);
}
#endif

FPlane UGoogleARCorePlane::GetFPlane() const
{
	FTransform PlaneCenterPose = GetCenterPose();
	FPlane UnrealPlane(PlaneCenterPose.GetLocation(), PlaneCenterPose.GetRotation().GetUpVector());
	return UnrealPlane;
}

FTransform UGoogleARCorePlane::GetCenterPose() const
{
	FTransform PlanePose;
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		ArPose* ARPoseHandle = nullptr;
		ArPose_create(Session.Pin()->GetHandle(), nullptr, &ARPoseHandle);
		ArPlane_getCenterPose(Session.Pin()->GetHandle(), GetPlaneHandle(), ARPoseHandle);
		PlanePose = ARCorePoseToUnrealTransform(ARPoseHandle, Session.Pin()->GetHandle(), Session.Pin()->GetWorldToMeterScale());
		ArPose_destroy(ARPoseHandle);
#endif
	}
	return PlanePose;
}

FVector UGoogleARCorePlane::GetExtents() const
{
	FVector PlaneExtent = FVector::ZeroVector;
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		float ARCorePlaneExtentX = 0; // X is right vector
		float ARCorePlaneExtentZ = 0; // Z is backward vector
		ArPlane_getExtentX(Session.Pin()->GetHandle(), GetPlaneHandle(), &ARCorePlaneExtentX);
		ArPlane_getExtentZ(Session.Pin()->GetHandle(), GetPlaneHandle(), &ARCorePlaneExtentZ);

		// Convert OpenGL axis to Unreal axis.
		PlaneExtent = FVector(-ARCorePlaneExtentZ, ARCorePlaneExtentX, 0) * Session.Pin()->GetWorldToMeterScale();
#endif
	}
	return PlaneExtent;
}

void UGoogleARCorePlane::GetBoundaryPolygonInLocalSpace(TArray<FVector>& OutPlanePolygon) const
{
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		int PolygonSize = 0;
		ArPlane_getPolygonSize(Session.Pin()->GetHandle(), GetPlaneHandle(), &PolygonSize);
		OutPlanePolygon.Empty(PolygonSize / 2);

		TArray<float> PolygonPointsXZ;
		PolygonPointsXZ.SetNumUninitialized(PolygonSize);
		ArPlane_getPolygon(Session.Pin()->GetHandle(), GetPlaneHandle(), PolygonPointsXZ.GetData());

		for (int i = 0; i < PolygonSize / 2; i++)
		{
			FVector PointInLocalSpace(-PolygonPointsXZ[2 * i + 1] * Session.Pin()->GetWorldToMeterScale(), PolygonPointsXZ[2 * i] * Session.Pin()->GetWorldToMeterScale(), 0.0f);
			OutPlanePolygon.Add(PointInLocalSpace);
		}
#endif
	}
}

UGoogleARCorePlane* UGoogleARCorePlane::GetSubsumedBy() const
{
	UGoogleARCorePlane* SubsumedByPlane = nullptr;
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		ArPlane* SubsumedByPlaneHandle = nullptr;
		ArPlane_acquireSubsumedBy(Session.Pin()->GetHandle(), GetPlaneHandle(), &SubsumedByPlaneHandle);
		if (SubsumedByPlaneHandle == nullptr)
		{
			return nullptr;
		}
		ArTrackable* TrackableHandle = reinterpret_cast<ArTrackable*>(SubsumedByPlaneHandle);
		SubsumedByPlane = Session.Pin()->GetUObjectManager()->GetTrackableFromHandle<UGoogleARCorePlane>(TrackableHandle, Session.Pin().Get());
#endif
	}
	return SubsumedByPlane;
}

bool UGoogleARCorePlane::IsPointInExtents(FVector Point) const
{
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		ArPose* ARPoseHandle = nullptr;
		FTransform PointTransform(FQuat::Identity, Point);
		UnrealTransformToARCorePose(PointTransform, Session.Pin()->GetHandle(), &ARPoseHandle, Session.Pin()->GetWorldToMeterScale());

		int32_t PointInExtents = 0;
		ArPlane_isPoseInExtents(Session.Pin()->GetHandle(), reinterpret_cast<ArPlane*>(TrackableHandle), ARPoseHandle, &PointInExtents);
		ArPose_destroy(ARPoseHandle);

		return PointInExtents;
#endif
	}
	return false;
}

bool UGoogleARCorePlane::IsPointInPolygon(FVector Point) const
{
	if (CheckIsSessionValid("ARCorePlane", Session))
	{
#if PLATFORM_ANDROID
		ArPose* ARPoseHandle = nullptr;
		FTransform PointTransform(FQuat::Identity, Point);
		UnrealTransformToARCorePose(PointTransform, Session.Pin()->GetHandle(), &ARPoseHandle, Session.Pin()->GetWorldToMeterScale());

		int32_t PointInPolygon = 0;
		ArPlane_isPoseInPolygon(Session.Pin()->GetHandle(), reinterpret_cast<ArPlane*>(TrackableHandle), ARPoseHandle, &PointInPolygon);
		ArPose_destroy(ARPoseHandle);

		return PointInPolygon;
#endif
	}
	return false;
}

/****************************************/
/*       UGoogleARCorePointCloud        */
/****************************************/
UGoogleARCorePointCloud::~UGoogleARCorePointCloud()
{
	ReleasePointCloud();
}

int64 UGoogleARCorePointCloud::GetUpdateTimestamp()
{
	if (CheckIsSessionValid("ARCorePointCloud", Session))
	{
#if PLATFORM_ANDROID
		int64_t TimeStamp = 0;
		ArPointCloud_getTimestamp(Session.Pin()->GetHandle(), PointCloudHandle, &TimeStamp);
		return TimeStamp;
#endif
	}
	return 0;
}

bool UGoogleARCorePointCloud::IsUpdated()
{
	return bIsUpdated;
}

int UGoogleARCorePointCloud::GetPointNum()
{
	int PointNum = 0;
	if (CheckIsSessionValid("ARCorePointCloud", Session))
	{
#if PLATFORM_ANDROID
		ArPointCloud_getNumberOfPoints(Session.Pin()->GetHandle(), PointCloudHandle, &PointNum);
#endif
	}
	return PointNum;
}

FVector UGoogleARCorePointCloud::GetWorldSpacePoint(int Index)
{
	FVector Point = FVector::ZeroVector;
	if (CheckIsSessionValid("ARCorePointCloud", Session))
	{
#if PLATFORM_ANDROID
		const float* PointData = nullptr;
		ArPointCloud_getData(Session.Pin()->GetHandle(), PointCloudHandle, &PointData);

		Point.Y = PointData[Index * 4];
		Point.Z = PointData[Index * 4 + 1];
		Point.X = -PointData[Index * 4 + 2];
		Point = Point * Session.Pin()->GetWorldToMeterScale();
#endif
	}
	return Point;
}

float UGoogleARCorePointCloud::GetPointConfidence(int Index)
{
	float Confidence = 0.0;
	if (CheckIsSessionValid("ARCorePointCloud", Session))
	{
#if PLATFORM_ANDROID
		const float* PointData;
		ArPointCloud_getData(Session.Pin()->GetHandle(), PointCloudHandle, &PointData);
		Confidence = PointData[Index * 4 + 3];
#endif
	}
	return Confidence;
}

void UGoogleARCorePointCloud::ReleasePointCloud()
{
#if PLATFORM_ANDROID
	ArPointCloud_release(PointCloudHandle);
#endif
}
