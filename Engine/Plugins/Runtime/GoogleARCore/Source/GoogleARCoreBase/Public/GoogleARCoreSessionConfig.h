// Copyright 2017 Google Inc.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GoogleARCoreTypes.h"

#include "GoogleARCoreSessionConfig.generated.h"
/**
 * Holds settings that are used to configure the ARCore session.
 */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCoreSessionConfig
{
	GENERATED_BODY()
public:
	/** The light estimation mode the ARCore session will use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|SessionConfiguration")
	EGoogleARCoreLightEstimationMode LightEstimationMode = EGoogleARCoreLightEstimationMode::AmbientIntensity;

	/** The type of plane detection the ARCore session will use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|SessionConfiguration")
	EGoogleARCorePlaneDetectionMode PlaneDetectionMode = EGoogleARCorePlaneDetectionMode::HorizontalPlane;

	/** The type of frame sync the ARCore session will use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|SessionConfiguration")
	EGoogleARCoreFrameUpdateMode FrameUpdateMode = EGoogleARCoreFrameUpdateMode::SyncTickWithoutCameraImage;

	/**
	 * Indicates whether to enable the passthrough camera rendering controlled by the GoogleARCoreXRCamera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|SessionConfiguration", meta = (EditCondition = "bLinkCameraToGoogleARDevice"))
	bool bEnablePassthroughCameraRendering = true;

	/** Indicates whether two FGoogleARCoreSessionConfig objects are equal. */
	inline bool operator==(const FGoogleARCoreSessionConfig& rhs) const
	{
		return LightEstimationMode == rhs.LightEstimationMode &&
			PlaneDetectionMode == rhs.PlaneDetectionMode &&
			FrameUpdateMode == rhs.FrameUpdateMode &&
			bEnablePassthroughCameraRendering == rhs.bEnablePassthroughCameraRendering;
	}
};
