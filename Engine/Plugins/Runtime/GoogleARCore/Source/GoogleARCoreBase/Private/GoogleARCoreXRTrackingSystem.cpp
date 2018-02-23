// Copyright 2017 Google Inc.

#include "GoogleARCoreXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "RHIDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreXRCamera.h"

FGoogleARCoreXRTrackingSystem::FGoogleARCoreXRTrackingSystem()
	: ARCoreDeviceInstance(nullptr)
	, bMatchDeviceCameraFOV(false)
	, bEnablePassthroughCameraRendering(false)
	, bHasValidPose(false)
	, CachedPosition(FVector::ZeroVector)
	, CachedOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
{
	UE_LOG(LogGoogleARCoreTrackingSystem, Log, TEXT("Creating GoogleARCore Tracking System."));
	ARCoreDeviceInstance = FGoogleARCoreDevice::GetInstance();
	check(ARCoreDeviceInstance);

	// Register our ability to hit-test in AR with Unreal
	IModularFeatures::Get().RegisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().RegisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
}

FGoogleARCoreXRTrackingSystem::~FGoogleARCoreXRTrackingSystem()
{
	// Unregister our ability to hit-test in AR with Unreal
	IModularFeatures::Get().UnregisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().UnregisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
}

/////////////////////////////////////////////////////////////////////////////////
// Begin FGoogleARCoreXRTrackingSystem IHeadMountedDisplay Virtual Interface   //
////////////////////////////////////////////////////////////////////////////////
FName FGoogleARCoreXRTrackingSystem::GetSystemName() const
{
	static FName DefaultName(TEXT("FGoogleARCoreXRTrackingSystem"));
	return DefaultName;
}

bool FGoogleARCoreXRTrackingSystem::HasValidTrackingPosition()
{
	return bHasValidPose;
}

bool FGoogleARCoreXRTrackingSystem::IsHeadTrackingAllowed() const
{
#if PLATFORM_ANDROID
	return true;
#else
	return false;
#endif
}

bool FGoogleARCoreXRTrackingSystem::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId)
	{
		OutOrientation = CachedOrientation;
		OutPosition = CachedPosition;
		return true;
	}
	else
	{
		return false;
	}
}

FString FGoogleARCoreXRTrackingSystem::GetVersionString() const
{
	FString s = FString::Printf(TEXT("ARCoreHMD - %s, built %s, %s"), *FEngineVersion::Current().ToString(),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));

	return s;
}

bool FGoogleARCoreXRTrackingSystem::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

void FGoogleARCoreXRTrackingSystem::RefreshPoses()
{
	// @todo viewext we should move stuff from OnStartGameFrame into here?
}

bool FGoogleARCoreXRTrackingSystem::OnStartGameFrame(FWorldContext& WorldContext)
{
	FTransform CurrentPose;
	if (ARCoreDeviceInstance->GetIsARCoreSessionRunning())
	{
		if (ARCoreDeviceInstance->GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
		{
			CurrentPose = ARCoreDeviceInstance->GetLatestPose();
			bHasValidPose = true;
		}
		else
		{
			bHasValidPose = false;
		}

		if (bHasValidPose)
		{
			CachedOrientation = CurrentPose.GetRotation();
			CachedPosition = CurrentPose.GetTranslation();
		}
	}

	return true;
}

void FGoogleARCoreXRTrackingSystem::ConfigARCoreXRCamera(bool bInMatchDeviceCameraFOV, bool bInEnablePassthroughCameraRendering)
{
	bMatchDeviceCameraFOV = bInMatchDeviceCameraFOV;
	bEnablePassthroughCameraRendering = bInEnablePassthroughCameraRendering;

	static_cast<FGoogleARCoreXRCamera*>(GetXRCamera().Get())->ConfigXRCamera(bEnablePassthroughCameraRendering, bEnablePassthroughCameraRendering);
}


void FGoogleARCoreXRTrackingSystem::EnableColorCameraRendering(bool bInEnablePassthroughCameraRendering)
{
	bEnablePassthroughCameraRendering = bInEnablePassthroughCameraRendering;
	static_cast<FGoogleARCoreXRCamera*>(GetXRCamera().Get())->ConfigXRCamera(bEnablePassthroughCameraRendering, bEnablePassthroughCameraRendering);
}

bool FGoogleARCoreXRTrackingSystem::GetColorCameraRenderingEnabled()
{
	return bEnablePassthroughCameraRendering;
}

float FGoogleARCoreXRTrackingSystem::GetWorldToMetersScale() const
{
	if (IsInGameThread() && GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

//bool FGoogleARCoreXRTrackingSystem::ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults)
//{
//	return false;
//}

EARTrackingQuality FGoogleARCoreXRTrackingSystem::ARGetTrackingQuality() const
{

	if (!FGoogleARCoreDevice::GetInstance()->GetIsARCoreSessionRunning())
	{
		return EARTrackingQuality::NotAvailable;
	}

	// @todo arcore : HasValidTrackingPosition() is non-const. Why?
	if (!bHasValidPose)
	{
		return EARTrackingQuality::Limited;
	}

	return EARTrackingQuality::Normal;
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FGoogleARCoreXRTrackingSystem::GetXRCamera(int32 DeviceId /*= HMDDeviceId*/)
{
	check(DeviceId == HMDDeviceId);

	if (!XRCamera.IsValid())
	{
		XRCamera = FSceneViewExtensions::NewExtension<FGoogleARCoreXRCamera>(*this, DeviceId);
	}
	return XRCamera;
}
