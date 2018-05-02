// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ARSessionConfig.h"

UARSessionConfig::UARSessionConfig()
	: SessionType(EARSessionType::World)
	, PlaneDetectionMode_DEPRECATED(EARPlaneDetectionMode::HorizontalPlaneDetection)
	, bHorizontalPlaneDetection(true)
	, bVerticalPlaneDetection(true)
	, LightEstimationMode(EARLightEstimationMode::AmbientLightEstimate)
	, FrameSyncMode(EARFrameSyncMode::SyncTickWithoutCameraImage)
	, bEnableAutomaticCameraOverlay(true)
	, bEnableAutomaticCameraTracking(true)
{
}
EARSessionType UARSessionConfig::GetSessionType() const
{
	return SessionType;
}

EARPlaneDetectionMode UARSessionConfig::GetPlaneDetectionMode() const
{
	return static_cast<EARPlaneDetectionMode>(
		(bHorizontalPlaneDetection ? static_cast<int32>(EARPlaneDetectionMode::HorizontalPlaneDetection) : 0) |
		(bVerticalPlaneDetection ? static_cast<int32>(EARPlaneDetectionMode::VerticalPlaneDetection) : 0));
}

EARLightEstimationMode UARSessionConfig::GetLightEstimationMode() const
{
	return LightEstimationMode;
}

EARFrameSyncMode UARSessionConfig::GetFrameSyncMode() const
{
	return FrameSyncMode;
}

bool UARSessionConfig::ShouldRenderCameraOverlay() const
{
	return bEnableAutomaticCameraOverlay;
}

bool UARSessionConfig::ShouldEnableCameraTracking() const
{
	return bEnableAutomaticCameraTracking;
}
