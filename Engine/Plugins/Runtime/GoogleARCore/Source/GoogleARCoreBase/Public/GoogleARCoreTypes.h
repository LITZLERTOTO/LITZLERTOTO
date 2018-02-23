// Copyright 2017 Google Inc.
#pragma once

#include "CoreMinimal.h"

#include "GoogleARCoreTypes.generated.h"

/// @defgroup GoogleARCoreBase Google ARCore Base
/// The base module for Google ARCore plugin

#if PLATFORM_ANDROID
// Include <Camera/NdkCameraMetadata.h> to use this type.
typedef struct ACameraMetadata ACameraMetadata;

// Forward decalare type defined in arcore_c_api.h
typedef struct ArTrackable_ ArTrackable;
typedef struct ArPlane_ ArPlane;
typedef struct ArPoint_ ArPoint;
typedef struct ArPointCloud_ ArPointCloud;
typedef struct ArAnchor_ ArAnchor;
typedef struct ArHitResult_ ArHitResult;
#endif

UENUM(BlueprintType)
enum class EGoogleARCoreAvailability : uint8
{
	/* An internal error occurred while determining ARCore availability. */
	UnkownError = 0,
	/* ARCore is not installed, and a query has been issued to check if ARCore is is supported. */
	UnkownChecking = 1,
	/*
	* ARCore is not installed, and the query to check if ARCore is supported
	* timed out. This may be due to the device being offline.
	*/
	UnkownTimedOut = 2,
	/* ARCore is not supported on this device.*/
	UnsupportedDeviceNotCapable = 100,
	/* The device and Android version are supported, but the ARCore APK is not installed.*/
	SupportedNotInstalled = 201,
	/*
	* The device and Android version are supported, and a version of the
	* ARCore APK is installed, but that ARCore APK version is too old.
	*/
	SupportedApkTooOld = 202,
	/* ARCore is supported, installed, and available to use. */
	SupportedInstalled = 203
};

UENUM(BlueprintType)
enum class EGoogleARCoreInstallStatus : uint8
{
	/* The requested resource is already installed.*/
	Installed = 0,
	/* Installation of the resource was requested. The current activity will be paused. */
	Requrested = 1,
	/* Cannot start the install request because the platform is not supported. */
	NotAvailable = 200,
};

UENUM(BlueprintType)
enum class EGoogleARCoreInstallRequestResult : uint8
{
	/* The ARCore APK is installed*/
	Installed,
	/* ARCore APK install request failed because the device is not compatible. */
	DeviceNotCompatible,
	/* ARCore APK install request failed because the user declined the installation. */
	UserDeclinedInstallation,
	/* ARCore APK install request failed because unknown error happens while checking or requesting installation. */
	FatalError
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the status of most ARCore functions.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreFunctionStatus : uint8
{
	/** Function returned successfully. */
	Success,
	/** Function failed due to Fatal error. */
	Fatal,
	/** Function failed due to the session isn't running. */
	SessionPaused,
	/** Function failed due to ARCore session isn't in tracking state. */
	NotTracking,
	/** Function failed due to the requested resource is exhausted. */
	ResourceExhausted,
	/** Function failed due to the requested resource isn't available yet. */
	NotAvailable,
	/** Function failed due to the function augment has invalid type. */
	InvalidType,
	/** Function failed with unknown reason. */
	Unknown
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the ARCore session status.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreSessionStatus : uint8
{
	/** ARCore session hasn't started yet.*/
	Uninitialized,
	/** ARCore session is starting.*/
	Initializing,
	/** ARCore session is running. */
	Running,
	/** ARCore session is paused. Start ARCore session again will resume the paused session. */
	Paused,
	/** ARCore session runs into some error. Check the EGoogleARCoreSessionErrorReason enum for details. */
	Error
};

/**
* @ingroup GoogleARCoreBase
* Describes the ARCore session status.
*/
UENUM(BlueprintType)
enum class EGoogleARCoreSessionErrorReason : uint8
{
	/** ARCore session couldn't started because the device or the Android version isn't supported. */
	None,
	/** ARCore isn't available on this device because ARCore app isn't installed. */
	ARCoreUnavailable,
	/** ARCore session failed to start due to required runtime permission hasn't been granted. */
	PermissionNotGranted,
	/** ARCore session failed to start due to the request configuration isn't supported. */
	UnSupportedConfiguration,
	/** ARCore session encountered fatal error and need to restart. */
	FatalError,
	/** ARCore Session runs into unknown error. */
	Unknown,
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the tracking state of the current ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreTrackingState : uint8
{
	/** Tracking is valid. */
	Tracking = 0,
	/** Tracking is temporary lost but could recover in the future. */
	NotTracking = 1,
	/** Tracking is lost will not recover. */
	StoppedTracking = 2
};

/**
 * @ingroup GoogleARCoreBase
 * Describes what type of light estimation will be performed in ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreLightEstimationMode : uint8
{
	/** Light estimation disabled. */
	None = 0,
	/** Enable light estimation for ambient intensity. */
	AmbientIntensity = 1,
};

/**
 * @ingroup GoogleARCoreBase
 * Describes what type of plane detection will be performed in ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCorePlaneDetectionMode : uint8
{
	/** Disable plane detection. */
	None = 0,
	/** Track for horizontal plane. */
	HorizontalPlane = 1,
};

/**
 * @ingroup GoogleARCoreBase
 * Describes how the frame will be updated in ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreFrameUpdateMode : uint8
{
	/** Unreal tick will be synced with the camera image update rate. */
	SyncTickWithCameraImage = 0,
	/** Unreal tick will not related to the camera image update rate. */
	SyncTickWithoutCameraImage = 1,
};

/**
 * @ingroup GoogleARCoreBase
 * Describes which channel ARLineTrace will be performed on.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreLineTraceChannel : uint8
{
	/** Trace against feature point cloud. */
	FeaturePoint,
	/** 
	 * Trace against feature point and attempt to estimate the normal of the surface centered around the trace hit point. 
	 * Surface normal estimation is most likely to succeed on textured surfaces and with camera motion.
	 */
	FeaturePointWithSurfaceNormal,
	/** Trace against the infinite plane. */
	InfinitePlane,
	/** Trace against the plane using its extent. */
	PlaneUsingExtent,
	/** Trace against the plane using its boundary polygon. */
	PlaneUsingBoundaryPolygon
};

/**
 * A struct describes the ARCore light estimation.
 */
USTRUCT(BlueprintType)
struct FGoogleARCoreLightEstimate
{
	GENERATED_BODY()

	/** Whether this light estimation is valid. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|LightEstimation")
	bool bIsValid;

	/** The average pixel intensity of the passthrough camera image. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|LightEstimation")
	float PixelIntensity;
};

class FGoogleARCoreSession;
/**
 * A abstract UObject that describes something that ARCore can track and that Anchors can be attached to.
 */
UCLASS(abstract)
class GOOGLEARCOREBASE_API UGoogleARCoreTrackable : public UObject
{
	friend class UGoogleARCoreUObjectManager;

	GENERATED_BODY()

public:
	/** Destructor */
	~UGoogleARCoreTrackable();

	/** Returns the current tracking state of this trackable object. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Trackable")
	EGoogleARCoreTrackingState GetTrackingState();

	/** Returns type of this trackable object. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Trackable")
	UClass* GetTrackableType();

#if PLATFORM_ANDROID
	ArTrackable* GetTrackableHandle() { return TrackableHandle; }
#endif

protected:
	TWeakPtr<FGoogleARCoreSession> Session;

#if PLATFORM_ANDROID
	ArTrackable* TrackableHandle;
#endif
};

UENUM(BlueprintType)
enum class EGoogleARCoreTrackablePointOrientationMode : uint8
{
	/* The orientation of the UGoogleARCoreTrackablePoint is initialized to identity but may adjust slightly over time. */
	Identity = 0,
	/* The orientation of the UGoogleARCoreTrackablePoint is the estimated normal of the surface centered around the line trace. */
	EstimatedSurfaceNormal = 1,
};

/**
 * A UObject that describes a point in space that ARCore is tracking.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreTrackablePoint : public UGoogleARCoreTrackable
{
	GENERATED_BODY()

public:

	/** Returns the pose of this point in Unreal world space. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePoint")
	FTransform GetPose();

	/** Return the orientation mode of this trackable point. */
	EGoogleARCoreTrackablePointOrientationMode GetPointOrientationMode();

private:
#if PLATFORM_ANDROID
	ArPoint* GetPointHandle() const;
#endif
};

/**
 * A UObject that describes the current best knowledge of a real-world planar surface.
 *
 * <h1>Plane Merging/Subsumption</h1>
 * Two or more planes may be automatically merged into a single parent plane, resulting in each child
 * plane's GetSubsumedBy() returning the parent plane.
 *
 * A subsumed plane becomes effectively a transformed view of the parent plane. The pose and
 * bounding geometry will still update, but only in response to changes to the subsuming (parent)
 * plane's properties.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCorePlane : public UGoogleARCoreTrackable
{
	friend class FGoogleARCoreFrame;
	GENERATED_BODY()

public:
	/** Returns the infinite plane. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	FPlane GetFPlane() const;

	/** Returns the pose of the plane center in Unreal world space. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	FTransform GetCenterPose() const;

	/** Returns the extents of the plane. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	FVector GetExtents() const;

	/** Returns the estimated boundary polygon in local space of the plane. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	void GetBoundaryPolygonInLocalSpace(TArray<FVector>& OutPlanePolygon) const;

	/**
	 * Checks if a point in world space is on this plane and within its extents.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	bool IsPointInExtents(FVector Point) const;

	/**
	 * Checks if a point in world space is on this plane and within its polygon.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	bool IsPointInPolygon(FVector Point) const;

	/**
	 * Returns the UGoogleARCorePlane reference that subsumes this plane.
	 *
	 * @return The pointer to the plane that subsumes this plane. nullptr if this plane hasn't been subsumed.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|TrackablePlane")
	UGoogleARCorePlane* GetSubsumedBy() const;

private:
#if PLATFORM_ANDROID
	ArPlane* GetPlaneHandle() const;
#endif
};

/**
 * A UObject that contains a set of observed 3D points and confidence values.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCorePointCloud : public UObject
{
	friend class FGoogleARCoreFrame;
	friend class FGoogleARCoreSession;

	GENERATED_BODY()
public:
	/** Destructor */
	~UGoogleARCorePointCloud();

	/** Returns the timestamp in nanosecond when this point cloud was observed. */
	int64 GetUpdateTimestamp();

	/** Checks if this point cloud has been updated in this frame. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	bool IsUpdated();

	/** Returns the number of point inside this point cloud. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	int GetPointNum();

	/** Returns the point position in Unreal world space and it's confidence value from 0 ~ 1. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	void GetPoint(int Index, FVector& OutWorldPosition, float& OutConfidence);

	/** Release PointCloud's resources back to ArCore. Data will not be available after releasePointCloud is called. */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PointCloud")
	void ReleasePointCloud();

private:
	TWeakPtr<FGoogleARCoreSession> Session;
	bool bIsUpdated = false;
#if PLATFORM_ANDROID
	ArPointCloud* PointCloudHandle;
#endif
};

/**
* A UObject that describes a fixed location and orientation in the real world.
* To stay at a fixed location in physical space, the numerical description of this position will update
* as ARCore's understanding of the space improves. Use GetLatestPose() to get the latest updated numerical
* location of this anchor.
*/
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreAnchor : public UObject
{
	GENERATED_BODY()

	friend class FGoogleARCoreSession;
	friend class FGoogleARCoreFrame;
public:

	/**
	* Returns the current state of the pose of this anchor object. If this
	* state is anything but <code>Tracking</code> the
	* pose may be dramatically incorrect.
	*/
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|ARAnchor")
	EGoogleARCoreTrackingState GetTrackingState()
	{
		return TrackingState;
	};

	/**
	* Returns the pose of the anchor in Unreal world space. This pose
	* should only be considered valid if GetTrackingState() returns
	* <code>Tracking</code>.
	*/
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|ARAnchor")
	FTransform GetLatestPose()
	{
		return Pose;
	};

private:
#if PLATFORM_ANDROID
	ArAnchor* AnchorHandle;
#endif
	/** The anchor's latest pose in Unreal world space. */
	FTransform Pose;
	/** The anchor's current tracking state. */
	EGoogleARCoreTrackingState TrackingState;
};

/**
 * A struct describes the ARLineTrace result.
 */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCoreTraceResult
{
	GENERATED_BODY()

	/** 
	 * The pose of the hit point. The position is the location in space where the ray intersected the geometry. 
	 * The orientation is a best effort to face the user's device, and its exact definition differs depending 
	 * on the Trackable that was hit.
	 */
	UPROPERTY(Category = "GoogleARCore|TraceResult", VisibleAnywhere, BlueprintReadOnly)
	FTransform HitPointPose;
	
	/** The distance from the current camera pose to the hit point in Unreal world unit. */
	UPROPERTY(Category = "GoogleARCore|TraceResult", VisibleAnywhere, BlueprintReadOnly)
	float Distance;
	
	/** The trackable object the hit happened on. */
	UPROPERTY(Category = "GoogleARCore|TraceResult", VisibleAnywhere, BlueprintReadOnly)
	UGoogleARCoreTrackable* TrackableObject;

	/** The trackable channle the line trace hit. */
	UPROPERTY(Category = "GoogleARCore|TraceResult", VisibleAnywhere, BlueprintReadOnly)
	EGoogleARCoreLineTraceChannel HitChannel;
};

// @cond EXCLUDE_FROM_DOXYGEN
/**
* Helper class used to expose FGoogleARCoreSessionConfig setting in the Editor plugin settings.
*/
UCLASS(config = Engine, defaultconfig)
class GOOGLEARCOREBASE_API UGoogleARCoreEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Check this option if your app requires ARCore to run on Android. */
	UPROPERTY(EditAnywhere, config, Category = "GoogleARCore|PluginSettings", meta = (ShowOnlyInnerProperties))
	bool bARCoreRequiredApp = true;
};

// @endcond
