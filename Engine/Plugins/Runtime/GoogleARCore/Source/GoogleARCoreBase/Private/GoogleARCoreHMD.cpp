// Copyright 2017 Google Inc.

#include "GoogleARCoreHMD.h"
#include "Engine/Engine.h"
#include "RHIDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreMotionManager.h"


FGoogleARCoreHMD::FGoogleARCoreHMD()
	: TangoDeviceInstance(nullptr)
	, bHMDEnabled(true)
	, bSceneViewExtensionRegistered(false)
	, bARCameraEnabled(false)
	, bColorCameraRenderingEnabled(false)
	, bHasValidPose(false)
	, bLateUpdateEnabled(false)
	, bNeedToFlipCameraImage(false)
	, CachedPosition(FVector::ZeroVector)
	, CachedOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, TangoHMDEnableCommand(TEXT("ar.tango.HMD.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_HMDEnable",
			"Tango specific extension.\n"
			"Enable or disable Tango ARHMD.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::TangoHMDEnableCommandHandler))
	, ARCameraModeEnableCommand(TEXT("ar.tango.ARCameraMode.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_ARCameraEnable",
			"Tango specific extension.\n"
			"Enable or disable Tango AR Camera Mode.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::ARCameraModeEnableCommandHandler))
	, ColorCamRenderingEnableCommand(TEXT("ar.tango.ColorCamRendering.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_ColorCamRenderingEnable",
			"Tango specific extension.\n"
			"Enable or disable color camera rendering.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::ColorCamRenderingEnableCommandHandler))
	, LateUpdateEnableCommand(TEXT("ar.tango.LateUpdate.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_LateUpdateEnable",
			"Tango specific extension.\n"
			"Enable or disable late update in TangoARHMD.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::LateUpdateEnableCommandHandler))
{
	UE_LOG(LogGoogleARCoreHMD, Log, TEXT("Creating Tango HMD"));
	TangoDeviceInstance = FGoogleARCoreDevice::GetInstance();
	check(TangoDeviceInstance);
}

FGoogleARCoreHMD::~FGoogleARCoreHMD()
{
	// Manually unregister the SceneViewExtension.
	if (ViewExtension.IsValid() && GEngine && bSceneViewExtensionRegistered)
	{
		GEngine->ViewExtensions.Remove(ViewExtension);
	}
}

///////////////////////////////////////////////////////////////
// Begin FGoogleARCoreHMD IHeadMountedDisplay Virtual Interface   //
///////////////////////////////////////////////////////////////
FName FGoogleARCoreHMD::GetDeviceName() const
{
	static FName DefaultName(TEXT("FGoogleARCoreHMD"));
	return DefaultName;
}

bool FGoogleARCoreHMD::IsHMDConnected()
{
	//TODO: Figure out if we need to set it to disconnect based on Tango Tracking status
	return true;
}

bool FGoogleARCoreHMD::IsHMDEnabled() const
{
	return bHMDEnabled;
}

void FGoogleARCoreHMD::EnableHMD(bool allow)
{
	bHMDEnabled = allow;
}

EHMDDeviceType::Type FGoogleARCoreHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_ES2GenericStereoMesh;
}

bool FGoogleARCoreHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	// We don't need to change the size of viewport window so return false
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	return false;
}

bool FGoogleARCoreHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FGoogleARCoreHMD::HasValidTrackingPosition()
{
	return bHasValidPose;
}

void FGoogleARCoreHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	CurrentOrientation = CachedOrientation;
	CurrentPosition = CachedPosition;
}

TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> FGoogleARCoreHMD::GetViewExtension()
{
	TSharedPtr<FGoogleARCoreHMD, ESPMode::ThreadSafe> ptr(AsShared());
	return StaticCastSharedPtr<ISceneViewExtension>(ptr);
}

void FGoogleARCoreHMD::ApplyHmdRotation(APlayerController * PC, FRotator & ViewRotation)
{
	ViewRotation.Normalize();

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * CachedOrientation);
}

bool FGoogleARCoreHMD::UpdatePlayerCamera(FQuat & CurrentOrientation, FVector & CurrentPosition)
{
	if (bHMDEnabled)
	{
		CurrentOrientation = CachedOrientation;
		CurrentPosition = CachedPosition;
		return true;
	}
	return false;
}

bool FGoogleARCoreHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	// Manually register the SceneViewExtension since we are not enable stereo rendering.
	if (GEngine)
	{
		if (!ViewExtension.IsValid())
		{
			ViewExtension = GetViewExtension();
		}

		if (bHMDEnabled && !bSceneViewExtensionRegistered)
		{
			GEngine->ViewExtensions.Add(ViewExtension);
			bSceneViewExtensionRegistered = true;
		}

		// We should unregister the scene view extension when HMD is disabled.
		if (!bHMDEnabled && bSceneViewExtensionRegistered)
		{
			GEngine->ViewExtensions.Remove(ViewExtension);
			bSceneViewExtensionRegistered = false;
		}
	}

	FGoogleARCorePose CurrentPose;
	if (TangoDeviceInstance->GetIsTangoRunning())
	{
		if (bARCameraEnabled)
		{
			if (bLateUpdateEnabled)
			{
				// When using late update, we simple query the latest camera pose, we will make sure camera is synced with the camera texture in late update.
				bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::CAMERA_COLOR, CurrentPose);
			}
			else
			{
				// Use blocking call to grab the camera pose at the current camera image timestamp.
				double CameraTimestamp = TangoDeviceInstance->TangoARCameraManager.GetColorCameraImageTimestamp();
				bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::CAMERA_COLOR, CameraTimestamp, CurrentPose);
			}
		}
		else
		{
			bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::DEVICE, CurrentPose);
		}

		if (bHasValidPose)
		{
			CachedOrientation = CurrentPose.Pose.GetRotation();
			CachedPosition = CurrentPose.Pose.GetTranslation();
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////
// Begin FGoogleARCoreHMD ISceneViewExtension Virtual Interface //
/////////////////////////////////////////////////////////////

// Game Thread
void FGoogleARCoreHMD::SetupViewFamily(FSceneViewFamily & InViewFamily)
{
}

void FGoogleARCoreHMD::SetupView(FSceneViewFamily & InViewFamily, FSceneView & InView)
{
	InView.BaseHmdOrientation = CachedOrientation;
	InView.BaseHmdLocation = CachedPosition;
}

void FGoogleARCoreHMD::SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData)
{
	if (TangoDeviceInstance->GetIsTangoRunning() && bARCameraEnabled)
	{
		FIntRect ViewRect = InOutProjectionData.GetViewRect();
		InOutProjectionData.ProjectionMatrix = TangoDeviceInstance->TangoARCameraManager.CalculateColorCameraProjectionMatrix(ViewRect.Size());
	}
}

void FGoogleARCoreHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (TangoDeviceInstance->GetIsTangoRunning() && bARCameraEnabled)
	{
		TangoDeviceInstance->TangoARCameraManager.OnBeginRenderView();
	}
}

// Render Thread
void FGoogleARCoreHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate & RHICmdList, FSceneViewFamily & InViewFamily)
{
	if (TangoDeviceInstance->GetIsTangoRunning() && bLateUpdateEnabled)
	{
		if (bARCameraEnabled)
		{
			// If Ar camera enabled, we need to sync the camera pose with the camera image timestamp.
			TangoDeviceInstance->TangoARCameraManager.LateUpdateColorCameraTexture_RenderThread();

			double Timestamp = TangoDeviceInstance->TangoARCameraManager.GetColorCameraImageTimestamp();

			bLateUpdatePoseIsValid = TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::CAMERA_COLOR, Timestamp, LateUpdatePose);

			if (!bLateUpdatePoseIsValid)
			{
#if ENABLE_ARCORE_DEBUG_LOG
				UE_LOG(LogGoogleARCoreHMD, Warning, TEXT("Failed to late update tango color camera pose at timestamp %f."), Timestamp);
#endif
			}
		}
		else
		{
			// If we are not using Ar camera mode, we just need to late update the camera pose to the latest tango device pose.
			bLateUpdatePoseIsValid = TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, 0, LateUpdatePose);
		}

		if (bLateUpdatePoseIsValid)
		{
			const FSceneView* MainView = InViewFamily.Views[0];
			const FTransform OldRelativeTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);

			ApplyLateUpdate(InViewFamily.Scene, OldRelativeTransform, LateUpdatePose.Pose);
		}
	}
}

void FGoogleARCoreHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	// Late Update Camera Poses
	if (TangoDeviceInstance->GetIsTangoRunning() && bLateUpdateEnabled && bLateUpdatePoseIsValid)
	{
		const FTransform OldLocalCameraTransform(InView.BaseHmdOrientation, InView.BaseHmdLocation);
		const FTransform OldWorldCameraTransform(InView.ViewRotation, InView.ViewLocation);
		const FTransform CameraParentTransform = OldLocalCameraTransform.Inverse() * OldWorldCameraTransform;
		const FTransform NewWorldCameraTransform = LateUpdatePose.Pose * CameraParentTransform;

		InView.ViewLocation = NewWorldCameraTransform.GetLocation();
		InView.ViewRotation = NewWorldCameraTransform.Rotator();
		InView.UpdateViewMatrix();
	}
}

// Render the color camera overlay after the mobile base pass (opaque).
void FGoogleARCoreHMD::PostRenderMobileBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	if (TangoDeviceInstance->GetIsTangoRunning() && bARCameraEnabled && bColorCameraRenderingEnabled)
	{
		TangoDeviceInstance->TangoARCameraManager.RenderColorCameraOverlay_RenderThread(RHICmdList, InView);
	}
}

////////////////////////////////////
// Begin FGoogleARCoreHMD Class Method //
////////////////////////////////////

void FGoogleARCoreHMD::ConfigTangoHMD(bool bEnableHMD, bool bEnableARCamera, bool bEnableLateUpdate)
{
	EnableHMD (bEnableHMD);
	bARCameraEnabled = bEnableARCamera;
	bColorCameraRenderingEnabled = bEnableARCamera;
	bLateUpdateEnabled = bEnableLateUpdate;
}


void FGoogleARCoreHMD::EnableColorCameraRendering(bool bEnableColorCameraRnedering)
{
	bColorCameraRenderingEnabled = bEnableColorCameraRnedering;
}

bool FGoogleARCoreHMD::GetColorCameraRenderingEnabled()
{
	return bColorCameraRenderingEnabled;
}

bool FGoogleARCoreHMD::GetTangoHMDARModeEnabled()
{
	return bARCameraEnabled;
}

bool FGoogleARCoreHMD::GetTangoHMDLateUpdateEnabled()
{
	return bLateUpdateEnabled;
}

/** Console command Handles */
void FGoogleARCoreHMD::TangoHMDEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		EnableHMD(bShouldEnable);
	}
}
void FGoogleARCoreHMD::ARCameraModeEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bARCameraEnabled = bShouldEnable;
	}
}
void FGoogleARCoreHMD::ColorCamRenderingEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bColorCameraRenderingEnabled = bShouldEnable;
	}
}
void FGoogleARCoreHMD::LateUpdateEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bLateUpdateEnabled = bShouldEnable;
		TangoDeviceInstance->SetForceLateUpdateEnable(bShouldEnable);
	}
}

