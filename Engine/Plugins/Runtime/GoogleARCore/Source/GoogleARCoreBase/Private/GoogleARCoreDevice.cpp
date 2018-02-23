// Copyright 2017 Google Inc.

#include "GoogleARCoreDevice.h"
#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeCounter64.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h" // for FWorldDelegates
#include "Engine/Engine.h"
#include "GeneralProjectSettings.h"
#include "GoogleARCoreXRTrackingSystem.h"
#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreBaseLogCategory.h"

#if PLATFORM_ANDROID
#include "AndroidApplication.h"
#endif

#include "GoogleARCorePermissionHandler.h"

namespace 
{
	EGoogleARCoreFunctionStatus ToARCoreFunctionStatus(EGoogleARCoreAPIStatus Status)
	{
		switch (Status)
		{
		case EGoogleARCoreAPIStatus::AR_SUCCESS:
			return EGoogleARCoreFunctionStatus::Success;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_TRACKING:
			return EGoogleARCoreFunctionStatus::NotTracking;
		case EGoogleARCoreAPIStatus::AR_ERROR_SESSION_PAUSED:
			return EGoogleARCoreFunctionStatus::SessionPaused;
		case EGoogleARCoreAPIStatus::AR_ERROR_RESOURCE_EXHAUSTED:
			return EGoogleARCoreFunctionStatus::ResourceExhausted;
		case EGoogleARCoreAPIStatus::AR_ERROR_NOT_YET_AVAILABLE:
			return EGoogleARCoreFunctionStatus::NotAvailable;
		default:
			ensureMsgf(false, TEXT("Unknown conversion from EGoogleARCoreAPIStatus %d to EGoogleARCoreFunctionStatus."), static_cast<int>(Status));
			return EGoogleARCoreFunctionStatus::Unknown;
		}
	}
}

FGoogleARCoreDevice* FGoogleARCoreDevice::GetInstance()
{
	static FGoogleARCoreDevice Inst;
	return &Inst;
}

FGoogleARCoreDevice::FGoogleARCoreDevice()
	: PassthroughCameraTexture(nullptr)
	, PassthroughCameraTextureId(-1)
	, bIsARCoreSessionRunning(false)
	, bForceLateUpdateEnabled(false)
	, bSessionConfigChanged(false)
	, bAndroidRuntimePermissionsRequested(false)
	, bAndroidRuntimePermissionsGranted(false)
	, bPermissionDeniedByUser(false)
	, bStartSessionRequested(false)
	, bShouldSessionRestart(false)
	, bARCoreInstallRequested(false)
	, bARCoreInstalled(false)
	, WorldToMeterScale(100.0f)
	, PermissionHandler(nullptr)
	, bDisplayOrientationChanged(false)
	, CurrentSessionStatus(EGoogleARCoreSessionStatus::Uninitialized)
	, CurrentSessionError(EGoogleARCoreSessionErrorReason::None)
{
}

void FGoogleARCoreDevice::OnModuleLoaded()
{
	// Init display orientation.
	OnDisplayOrientationChanged();

	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FGoogleARCoreDevice::OnWorldTickStart);
}

void FGoogleARCoreDevice::OnModuleUnloaded()
{
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
	// clear the unique ptr.
	ARCoreSession.Reset();
}

EGoogleARCoreAvailability FGoogleARCoreDevice::CheckARCoreAPKAvailability()
{
	return FGoogleARCoreAPKManager::CheckARCoreAPKAvailability();
}

EGoogleARCoreAPIStatus FGoogleARCoreDevice::RequestInstall(bool bUserRequestedInstall, EGoogleARCoreInstallStatus& OutInstallStatus)
{
	return FGoogleARCoreAPKManager::RequestInstall(bUserRequestedInstall, OutInstallStatus);
}

bool FGoogleARCoreDevice::GetIsARCoreSessionRunning()
{
	return bIsARCoreSessionRunning;
}

void FGoogleARCoreDevice::GetSessionStatue(EGoogleARCoreSessionStatus& OutStatus, EGoogleARCoreSessionErrorReason& OutError)
{
	OutStatus = CurrentSessionStatus;
	OutError = CurrentSessionError;
}

bool FGoogleARCoreDevice::GetCurrentSessionConfig(FGoogleARCoreSessionConfig& OutARCoreConfig)
{
	if (GetIsARCoreSessionRunning())
	{
		// Return the last known config if the session is running.
		OutARCoreConfig = LastKnownARCoreConfig;
		return true;
	}
	else
	{
		return false;
	}
}

float FGoogleARCoreDevice::GetWorldToMetersScale()
{
	return WorldToMeterScale;
}

// This function will be called by public function to start AR core session request.
void FGoogleARCoreDevice::StartARCoreSessionRequest(const FGoogleARCoreSessionConfig& SessionConfig)
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Start ARCore session requested"));

	if (bIsARCoreSessionRunning)
	{
		if (SessionConfig == LastKnownARCoreConfig)
		{
			UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore session is already running with the requested ARCore config. Request aborted."));
			bStartSessionRequested = false;
			return;
		}

		PauseARCoreSession();
	}
		
	if (bStartSessionRequested)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore session is already starting. This will overriding the previous session config with the new one."))
	}

	RequestARCoreConfig = SessionConfig;
	bStartSessionRequested = true;
	// Re-request permission if necessary
	bPermissionDeniedByUser = false;
	bARCoreInstallRequested = false;

	// Try recreating the ARCoreSession to fix the fatal error.
	if (CurrentSessionStatus == EGoogleARCoreSessionStatus::Error && CurrentSessionError == EGoogleARCoreSessionErrorReason::FatalError)
	{
		UE_LOG(LogGoogleARCore, Warning, TEXT("ARCore session will be recreated due to fatal error happened"));
		ARCoreSession.Reset();
	}

	CurrentSessionStatus = EGoogleARCoreSessionStatus::Initializing;
	CurrentSessionError = EGoogleARCoreSessionErrorReason::None;
}

bool FGoogleARCoreDevice::GetStartSessionRequestFinished()
{
	return !bStartSessionRequested;
}

// Note that this function will only be registered when ARCore is supported.
void FGoogleARCoreDevice::OnWorldTickStart(ELevelTick TickType, float DeltaTime)
{
	WorldToMeterScale = GWorld->GetWorldSettings()->WorldToMeters;
	TFunction<void()> Func;
	while (RunOnGameThreadQueue.Dequeue(Func))
	{
		Func();
	}

	if (!bIsARCoreSessionRunning && bStartSessionRequested)
	{
		if (!bARCoreInstalled)
		{
			EGoogleARCoreInstallStatus InstallStatus = EGoogleARCoreInstallStatus::NotAvailable;
			EGoogleARCoreAPIStatus Status = FGoogleARCoreAPKManager::RequestInstall(!bARCoreInstallRequested, InstallStatus);
			UE_LOG(LogTemp, Log, TEXT("Requset ARCore Install(User Requested %d) in start session Status: %d, Install Status: %d"), !bARCoreInstallRequested, (int)Status, (int)InstallStatus);
			if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
			{
				bStartSessionRequested = false;
				CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
				CurrentSessionError = EGoogleARCoreSessionErrorReason::ARCoreUnavailable;
			}
			else if (InstallStatus == EGoogleARCoreInstallStatus::Installed)
			{
				bARCoreInstalled = true;
			}
			else
			{
				bARCoreInstallRequested = true;
			}
		}
		else if (bPermissionDeniedByUser)
		{
			CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
			CurrentSessionError = EGoogleARCoreSessionErrorReason::PermissionNotGranted;
			bStartSessionRequested = false;
		}
		else
		{
			CheckAndRequrestPermission(RequestARCoreConfig);
			// Either we don't need to request permission or the permission request is done.
			// Start the session request.
			if (!bAndroidRuntimePermissionsRequested)
			{
				StartSessionWithRequestedConfig();
			}
		}
	}

	if (bIsARCoreSessionRunning)
	{
		// Update ARFrame
		FVector2D ViewportSize(1, 1);
		if (GEngine && GEngine->GameViewport)
		{
			ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}
		ARCoreSession->SetDisplayGeometry(FGoogleARCoreAndroidHelper::GetDisplayRotation(), ViewportSize.X, ViewportSize.Y);

		EGoogleARCoreAPIStatus Status = ARCoreSession->Update(WorldToMeterScale);
		if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
		{
			ARCoreSession->Pause();
			bIsARCoreSessionRunning = false;
			CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
			CurrentSessionError = EGoogleARCoreSessionErrorReason::FatalError;
		}
		else
		{
			CameraBlitter.DoBlit(PassthroughCameraTextureId, FIntPoint(1080, 1920));
		}
	}
}

void FGoogleARCoreDevice::CheckAndRequrestPermission(const FGoogleARCoreSessionConfig& ConfigurationData)
{
	if (!bAndroidRuntimePermissionsRequested)
	{
		TArray<FString> RuntimePermissions;
		TArray<FString> NeededPermissions;
		GetRequiredRuntimePermissionsForConfiguration(ConfigurationData, RuntimePermissions);
		if (RuntimePermissions.Num() > 0)
		{
			for (int32 i = 0; i < RuntimePermissions.Num(); i++)
			{
				if (!UARCoreAndroidPermissionHandler::CheckRuntimePermission(RuntimePermissions[i]))
				{
					NeededPermissions.Add(RuntimePermissions[i]);
				}
			}
		}
		if (NeededPermissions.Num() > 0)
		{
			bAndroidRuntimePermissionsGranted = false;
			bAndroidRuntimePermissionsRequested = true;
			if (PermissionHandler == nullptr)
			{
				PermissionHandler = NewObject<UARCoreAndroidPermissionHandler>();
				PermissionHandler->AddToRoot();
			}
			PermissionHandler->RequestRuntimePermissions(NeededPermissions);
		}
		else
		{
			bAndroidRuntimePermissionsGranted = true;
		}
	}
}

void FGoogleARCoreDevice::HandleRuntimePermissionsGranted(const TArray<FString>& RuntimePermissions, const TArray<bool>& Granted)
{
	bool bGranted = true;
	for (int32 i = 0; i < RuntimePermissions.Num(); i++)
	{
		if (!Granted[i])
		{
			bGranted = false;
			UE_LOG(LogGoogleARCore, Warning, TEXT("Android runtime permission denied: %s"), *RuntimePermissions[i]);
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Android runtime permission granted: %s"), *RuntimePermissions[i]);
		}
	}
	bAndroidRuntimePermissionsRequested = false;
	bAndroidRuntimePermissionsGranted = bGranted;

	if (!bGranted)
	{
		bPermissionDeniedByUser = true;
	}
}

void FGoogleARCoreDevice::StartSessionWithRequestedConfig()
{
	bStartSessionRequested = false;
	
	// Allocate passthrough camera texture if necessary.
	if (PassthroughCameraTexture == nullptr)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateCameraImageUV,
			FGoogleARCoreDevice*, ARCoreDevicePtr, this,
			{
				ARCoreDevicePtr->AllocatePassthroughCameraTexture_RenderThread();
			}
		);
		FlushRenderingCommands();
	}

	if (!ARCoreSession.IsValid())
	{
		ARCoreSession = FGoogleARCoreSession::CreateARCoreSession();
		EGoogleARCoreAPIStatus SessionCreateStatus = ARCoreSession->GetSessionCreateStatus();

		if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_SUCCESS)
		{
			ensureMsgf(false, TEXT("Failed to create ARCore session with error status: %d"), (int)SessionCreateStatus);
			CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
			if (SessionCreateStatus != EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
			{
				CurrentSessionError = EGoogleARCoreSessionErrorReason::ARCoreUnavailable;
			}
			else
			{
				CurrentSessionError = EGoogleARCoreSessionErrorReason::FatalError;
			}
			ARCoreSession.Reset();
			return;
		}
	}

	StartSession(RequestARCoreConfig);
}

void FGoogleARCoreDevice::StartSession(const FGoogleARCoreSessionConfig& ConfigurationData)
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Starting ARCore Session."));

	EGoogleARCoreAPIStatus Status = ARCoreSession->ConfigSession(ConfigurationData);

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));
		CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
		CurrentSessionError = EGoogleARCoreSessionErrorReason::UnSupportedConfiguration;
		return;
	}

	check(PassthroughCameraTextureId != -1);
	ARCoreSession->SetCameraTextureId(PassthroughCameraTextureId);

	Status = ARCoreSession->Resume();

	if (Status != EGoogleARCoreAPIStatus::AR_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("ARCore Session start failed with error status %d"), static_cast<int>(Status));
		ensureMsgf(Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL, TEXT("ARCoreSession resume with unhandled error status: %d"), Status);
		// If we failed here, the only reason would be fatal error.
		CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
		CurrentSessionError = EGoogleARCoreSessionErrorReason::FatalError;
		return;
	}
	
	if (GEngine->XRSystem.IsValid())
	{
		FGoogleARCoreXRTrackingSystem* ARCoreTrackingSystem = static_cast<FGoogleARCoreXRTrackingSystem*>(GEngine->XRSystem.Get());
		if (ARCoreTrackingSystem)
		{
			ARCoreTrackingSystem->ConfigARCoreXRCamera(ConfigurationData.bEnablePassthroughCameraRendering, ConfigurationData.bEnablePassthroughCameraRendering);
		}
		else
		{
			UE_LOG(LogGoogleARCore, Error, TEXT("ERROR: GoogleARCoreXRTrackingSystem is not available."));
		}
	}

	LastKnownARCoreConfig = ConfigurationData;
	bIsARCoreSessionRunning = true;
	CurrentSessionStatus = EGoogleARCoreSessionStatus::Running;
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session started successfully."));
}

void FGoogleARCoreDevice::PauseARCoreSession()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Pausing ARCore session."));
	if (!bIsARCoreSessionRunning)
	{
		if (bARCoreInstallRequested)
		{
			bARCoreInstallRequested = false;
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Could not pause ARCore tracking session because there is no running tracking session!"));
		}
		return;
	}

	EGoogleARCoreAPIStatus Status = ARCoreSession->Pause();

	if (Status == EGoogleARCoreAPIStatus::AR_ERROR_FATAL)
	{
		CurrentSessionStatus = EGoogleARCoreSessionStatus::Error;
		CurrentSessionError = EGoogleARCoreSessionErrorReason::FatalError;
	}

	bIsARCoreSessionRunning = false;
	CurrentSessionStatus = EGoogleARCoreSessionStatus::Paused;
	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session paused"));
}

void FGoogleARCoreDevice::ResetARCoreSession()
{
	ARCoreSession.Reset();
	CurrentSessionStatus = EGoogleARCoreSessionStatus::Uninitialized;

	UE_LOG(LogGoogleARCore, Log, TEXT("ARCore session stopped and reset."));
}

void FGoogleARCoreDevice::AllocatePassthroughCameraTexture_RenderThread()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRHIResourceCreateInfo CreateInfo;

	PassthroughCameraTexture = RHICmdList.CreateTextureExternal2D(1, 1, PF_R8G8B8A8, 1, 1, 0, CreateInfo);

	void* NativeResource = PassthroughCameraTexture->GetNativeResource();
	check(NativeResource);
	PassthroughCameraTextureId = *reinterpret_cast<uint32*>(NativeResource);
}

FTextureRHIRef FGoogleARCoreDevice::GetPassthroughCameraTexture()
{
	return PassthroughCameraTexture;
}

FMatrix FGoogleARCoreDevice::GetPassthroughCameraProjectionMatrix(FIntPoint ViewRectSize) const
{
	if (!bIsARCoreSessionRunning)
	{
		return FMatrix::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetProjectionMatrix();
}

void FGoogleARCoreDevice::GetPassthroughCameraImageUVs(const TArray<float>& InUvs, TArray<float>& OutUVs) const
{
	if (!bIsARCoreSessionRunning)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->TransformDisplayUvCoords(InUvs, OutUVs);
}

EGoogleARCoreTrackingState FGoogleARCoreDevice::GetTrackingState() const
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreTrackingState::StoppedTracking;
	}
	return ARCoreSession->GetLatestFrame()->GetCameraTrackingState();
}

FTransform FGoogleARCoreDevice::GetLatestPose() const
{
	if (!bIsARCoreSessionRunning)
	{
		return FTransform::Identity;
	}
	return ARCoreSession->GetLatestFrame()->GetCameraPose();
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetPointCloud(OutLatestPointCloud));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::AcquireLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->AcquirePointCloud(OutLatestPointCloud));
}

#if PLATFORM_ANDROID
EGoogleARCoreFunctionStatus FGoogleARCoreDevice::GetLatestCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	return ToARCoreFunctionStatus(ARCoreSession->GetLatestFrame()->GetCameraMetadata(OutCameraMetadata));
}
#endif
FGoogleARCoreLightEstimate FGoogleARCoreDevice::GetLatestLightEstimate() const
{
	if (!bIsARCoreSessionRunning)
	{
		return FGoogleARCoreLightEstimate();
	}

	return ARCoreSession->GetLatestFrame()->GetLightEstimate();
}

void FGoogleARCoreDevice::ARLineTrace(const FVector2D& ScreenPosition, TSet<EGoogleARCoreLineTraceChannel> ARLineTraceChannels, TArray<FGoogleARCoreTraceResult>& OutHitResults)
{
	if (!bIsARCoreSessionRunning)
	{
		return;
	}

	OutHitResults.Empty();

	// Run hit test
	TArray<FGoogleARCoreTraceResult> AllHitResults;
	ARCoreSession->GetLatestFrame()->ARLineTrace(ScreenPosition, AllHitResults);

	// Filter hit result based on the requested channels.
	for (FGoogleARCoreTraceResult HitResult : AllHitResults)
	{
		UClass* TrackableType = HitResult.TrackableObject->GetTrackableType();
		if (TrackableType == UGoogleARCoreTrackablePoint::StaticClass())
		{
			if (ARLineTraceChannels.Contains(EGoogleARCoreLineTraceChannel::FeaturePointWithSurfaceNormal))
			{
				UGoogleARCoreTrackablePoint* TrackablePoint = Cast<UGoogleARCoreTrackablePoint>(HitResult.TrackableObject);
				if (TrackablePoint->GetPointOrientationMode() == EGoogleARCoreTrackablePointOrientationMode::EstimatedSurfaceNormal)
				{
					HitResult.HitChannel = EGoogleARCoreLineTraceChannel::FeaturePointWithSurfaceNormal;
					OutHitResults.Add(HitResult);
				}
			}
			else if (ARLineTraceChannels.Contains(EGoogleARCoreLineTraceChannel::FeaturePoint))
			{
				HitResult.HitChannel = EGoogleARCoreLineTraceChannel::FeaturePoint;
				OutHitResults.Add(HitResult);
			}
		}

		else if (TrackableType == UGoogleARCorePlane::StaticClass())
		{
			UGoogleARCorePlane* Plane = dynamic_cast<UGoogleARCorePlane*>(HitResult.TrackableObject);
			FVector HitPoint = HitResult.HitPointPose.GetLocation();
			if (ARLineTraceChannels.Contains(EGoogleARCoreLineTraceChannel::PlaneUsingBoundaryPolygon) && Plane->IsPointInPolygon(HitPoint))
			{
				HitResult.HitChannel = EGoogleARCoreLineTraceChannel::PlaneUsingBoundaryPolygon;
				OutHitResults.Add(HitResult);
			}
			else if (ARLineTraceChannels.Contains(EGoogleARCoreLineTraceChannel::PlaneUsingExtent) && Plane->IsPointInExtents(HitPoint))
			{
				HitResult.HitChannel = EGoogleARCoreLineTraceChannel::PlaneUsingExtent;
				OutHitResults.Add(HitResult);
			}
			else if (ARLineTraceChannels.Contains(EGoogleARCoreLineTraceChannel::InfinitePlane))
			{
				HitResult.HitChannel = EGoogleARCoreLineTraceChannel::InfinitePlane;
				OutHitResults.Add(HitResult);
			}
		}
	}
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::CreateARAnchor(const FTransform& ARAnchorWorldTransform, UGoogleARCoreAnchor*& OutARAnchorObject)
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	return ToARCoreFunctionStatus(ARCoreSession->CreateARAnchor(ARAnchorWorldTransform, OutARAnchorObject));
}

EGoogleARCoreFunctionStatus FGoogleARCoreDevice::CreateARAnchorObjectFromHitTestResult(FGoogleARCoreTraceResult HitTestResult,
	UGoogleARCoreAnchor*& OutARAnchorObject)
{
	if (!bIsARCoreSessionRunning)
	{
		return EGoogleARCoreFunctionStatus::SessionPaused;
	}

	return ToARCoreFunctionStatus(ARCoreSession->CreateARAnchorFromHitTestResult(HitTestResult, OutARAnchorObject));
}

void FGoogleARCoreDevice::DetachARAnchor(UGoogleARCoreAnchor* ARAnchorObject)
{
	if (!bIsARCoreSessionRunning)
	{
		return;
	}
	ARCoreSession->DetachAnchor(ARAnchorObject);
}

void FGoogleARCoreDevice::GetAllAnchors(TArray<UGoogleARCoreAnchor*>& ARCoreAnchorList)
{
	if (!bIsARCoreSessionRunning)
	{
		return;
	}
	ARCoreSession->GetAllAnchors(ARCoreAnchorList);
}

void FGoogleARCoreDevice::GetUpdatedAnchors(TArray<UGoogleARCoreAnchor*>& ARCoreAnchorList)
{
	if (!bIsARCoreSessionRunning)
	{
		return;
	}
	ARCoreSession->GetLatestFrame()->GetUpdatedAnchors(ARCoreAnchorList);
}

// Functions that are called on Android lifecycle events.
void FGoogleARCoreDevice::OnApplicationCreated()
{
}

void FGoogleARCoreDevice::OnApplicationDestroyed()
{
}

void FGoogleARCoreDevice::OnApplicationPause()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnPause Called: %d"), bIsARCoreSessionRunning);
	bShouldSessionRestart = bIsARCoreSessionRunning;
	if (bIsARCoreSessionRunning)
	{
		PauseARCoreSession();
	}
}

void FGoogleARCoreDevice::OnApplicationResume()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("OnResume Called: %d"), bShouldSessionRestart);
	// Try to ask for permission if it is denied by user.
	if (bShouldSessionRestart)
	{
		bShouldSessionRestart = false;
		StartSession(LastKnownARCoreConfig);
	}
}

void FGoogleARCoreDevice::OnApplicationStop()
{
}

void FGoogleARCoreDevice::OnApplicationStart()
{
}

// TODO: we probably don't need this.
void FGoogleARCoreDevice::OnDisplayOrientationChanged()
{
	FGoogleARCoreAndroidHelper::UpdateDisplayRotation();
	bDisplayOrientationChanged = true;
}
