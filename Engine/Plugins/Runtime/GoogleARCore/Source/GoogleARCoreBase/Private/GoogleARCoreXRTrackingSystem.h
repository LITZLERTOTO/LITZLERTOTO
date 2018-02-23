// Copyright 2017 Google Inc.

#pragma once
#include "XRTrackingSystemBase.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "SceneViewExtension.h"
#include "SceneViewport.h"
#include "SceneView.h"
#include "GoogleARCoreDevice.h"
#include "ARHitTestingSupport.h"
#include "ARTrackingQuality.h"

/**
* GoogleARCore implementation of FXRTrackingSystemBase
*/
class FGoogleARCoreXRTrackingSystem : public FXRTrackingSystemBase, public IARHitTestingSupport, public IARTrackingQuality, public TSharedFromThis<FGoogleARCoreXRTrackingSystem, ESPMode::ThreadSafe>
{
	friend class FGoogleARCoreXRCamera;

private:
	/** IXRTrackingSystem */
	virtual FName GetSystemName() const override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual FString GetVersionString() const override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual void RefreshPoses() override;
	virtual TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> GetXRCamera(int32 DeviceId = HMDDeviceId) override;

public:
	/** IHeadMountedDisplay interface */

	// @todo : revisit this function; do we need them?
	virtual bool HasValidTrackingPosition() override;
	// @todo : can I get rid of this? At least rename to IsCameraTracking / IsTrackingAllowed()
	virtual bool IsHeadTrackingAllowed() const override;

	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;

	// TODO: Figure out if we need to allow developer set/reset base orientation and position.
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override {}

	// @todo move this to some interface
	virtual float GetWorldToMetersScale() const override;


	//~ IARHitTestingSupport
	//virtual bool ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults) override;
	//~ IARHitTestingSupport

	//~ IARTrackingQuality
	virtual EARTrackingQuality ARGetTrackingQuality() const override;
	//~ IARTrackingQuality

public:
	FGoogleARCoreXRTrackingSystem();

	~FGoogleARCoreXRTrackingSystem();

	void ConfigARCoreXRCamera(bool bMatchCameraFOV, bool bEnablePassthroughRendering);

	void EnableColorCameraRendering(bool bEnableColorCameraRnedering);

	bool GetColorCameraRenderingEnabled();

private:
	FGoogleARCoreDevice* ARCoreDeviceInstance;

	bool bMatchDeviceCameraFOV;
	bool bEnablePassthroughCameraRendering;
	bool bHasValidPose;

	FVector CachedPosition;
	FQuat CachedOrientation;
	FRotator DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat DeltaControlOrientation; // same as DeltaControlRotation but as quat

	TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
};

DEFINE_LOG_CATEGORY_STATIC(LogGoogleARCoreTrackingSystem, Log, All);
