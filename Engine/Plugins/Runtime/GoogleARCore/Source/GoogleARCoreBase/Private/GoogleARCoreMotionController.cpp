// Copyright 2017 Google Inc.

#include "GoogleARCoreMotionController.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreDevice.h"

FGoogleARCoreMotionController::FGoogleARCoreMotionController()
	: ARCoreDeviceInstance(nullptr)
{
	ARCoreDeviceInstance = FGoogleARCoreDevice::GetInstance();
}

void FGoogleARCoreMotionController::RegisterController()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void FGoogleARCoreMotionController::UnregisterController()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

bool FGoogleARCoreMotionController::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	// Only update in game thread and opt out updating on render thread since we only update the ARCore pose on game thread.
	if (IsInGameThread() && ARCoreDeviceInstance->GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
	{
		FTransform LatestPose = ARCoreDeviceInstance->GetLatestPose();
		OutOrientation = FRotator(LatestPose.GetRotation());
		OutPosition = LatestPose.GetTranslation();

		return true;
	}

	return false;
}

ETrackingStatus FGoogleARCoreMotionController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	if (ARCoreDeviceInstance->GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
	{	
		return ETrackingStatus::Tracked;
	}
	else
	{
		return ETrackingStatus::NotTracked;
	}
}

FName FGoogleARCoreMotionController::GetMotionControllerDeviceTypeName() const
{
	static const FName DeviceName(TEXT("ARcoreCameraMotionController"));
	return DeviceName;
}
