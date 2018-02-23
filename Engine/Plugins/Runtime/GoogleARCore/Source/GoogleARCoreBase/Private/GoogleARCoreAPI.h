// Copyright 2017 Google Inc.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ARHitTestingSupport.h"

#include "GoogleARCoreTypes.h"
#include "GoogleARCoreSessionConfig.h"
#include "GoogleARCoreCameraImageBlitter.h"

#if PLATFORM_ANDROID
#include "arcore_c_api.h"
#endif

#include "GoogleARCoreAPI.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogGoogleARCoreAPI, Log, All);

enum class EGoogleARCoreAPIStatus : int
{
	AR_SUCCESS = 0,
	// Invalid argument handling: null pointers and invalid enums for void
	// functions are handled by logging and returning best-effort value.
	// Non-void functions additionally return AR_ERROR_INVALID_ARGUMENT.
	AR_ERROR_INVALID_ARGUMENT = -1,
	AR_ERROR_FATAL = -2,
	AR_ERROR_SESSION_PAUSED = -3,
	AR_ERROR_SESSION_NOT_PAUSED = -4,
	AR_ERROR_NOT_TRACKING = -5,
	AR_ERROR_TEXTURE_NOT_SET = -6,
	AR_ERROR_MISSING_GL_CONTEXT = -7,
	AR_ERROR_UNSUPPORTED_CONFIGURATION = -8,
	AR_ERROR_CAMERA_PERMISSION_NOT_GRANTED = -9,

	// Acquire failed because the object being acquired is already released.
	// This happens e.g. if the developer holds an old frame for too long, and
	// then tries to acquire a point cloud from it.
	AR_ERROR_DEADLINE_EXCEEDED = -10,

	// Acquire failed because there are too many objects already acquired. For
	// example, the developer may acquire up to N point clouds.
	// N is determined by available resources, and is usually small, e.g. 8.
	// If the developer tries to acquire N+1 point clouds without releasing the
	// previously acquired ones, they will get this error.
	AR_ERROR_RESOURCE_EXHAUSTED = -11,

	// Acquire failed because the data isn't available yet for the current
	// frame. For example, acquire the image metadata may fail with this error
	// because the camera hasn't fully started.
	AR_ERROR_NOT_YET_AVAILABLE = -12,

	// The android camera has been reallocated to a higher priority app or is
	// otherwise unavailable.
	AR_ERROR_CAMERA_NOT_AVAILABLE = -13,

	AR_UNAVAILABLE_ARCORE_NOT_INSTALLED = -100,
	AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE = -101,
	AR_UNAVAILABLE_ANDROID_VERSION_NOT_SUPPORTED = -102,
	// The ARCore APK currently installed on device is too old and needs to be
	// updated. For example, SDK v2.0.0 when APK is v1.0.0.
	AR_UNAVAILABLE_APK_TOO_OLD = -103,
	// The ARCore APK currently installed no longer supports the sdk that the
	// app was built with. For example, SDK v1.0.0 when APK includes support for
	// v2.0.0+.
	AR_UNAVAILABLE_SDK_TOO_OLD = -104,

	// The user declined installation of the ARCore APK during this run of the
	// application and the current request was not marked as user-initiated.
	AR_UNAVAILABLE_USER_DECLINED_INSTALLATION = -105,
};

class FGoogleARCoreFrame;
class FGoogleARCoreSession;

UCLASS()
class UGoogleARCoreUObjectManager : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<UGoogleARCoreAnchor*> AllAnchors;

	UPROPERTY()
	UGoogleARCorePointCloud* LatestPointCloud;

#if PLATFORM_ANDROID
	TMap<ArAnchor*, UGoogleARCoreAnchor*> AnchorHandleMap;
	TMap<ArTrackable*, TWeakObjectPtr<UGoogleARCoreTrackable>> TrackableHandleMap;

	template< class T > T* GetTrackableFromHandle(ArTrackable* TrackableHandle, FGoogleARCoreSession* Session);
#endif
};

class FGoogleARCoreAPKManager
{
public:
	static EGoogleARCoreAvailability CheckARCoreAPKAvailability();
	static EGoogleARCoreAPIStatus RequestInstall(bool bUserRequestedInstall, EGoogleARCoreInstallStatus& OutInstallStatus);
};

class FGoogleARCoreSession : public TSharedFromThis<FGoogleARCoreSession>
{

public:
	static TSharedPtr<FGoogleARCoreSession> CreateARCoreSession();

	FGoogleARCoreSession();
	~FGoogleARCoreSession();

	// Properties
	EGoogleARCoreAPIStatus GetSessionCreateStatus();
	UGoogleARCoreUObjectManager* GetUObjectManager();
	float GetWorldToMeterScale();
#if PLATFORM_ANDROID
	ArSession* GetHandle();
#endif

	// Lifecycle
	bool IsConfigSupported(FGoogleARCoreSessionConfig Config);
	EGoogleARCoreAPIStatus ConfigSession(FGoogleARCoreSessionConfig Config);
	EGoogleARCoreAPIStatus Resume();
	EGoogleARCoreAPIStatus Pause();
	EGoogleARCoreAPIStatus Update(float WorldToMeterScale);
	const FGoogleARCoreFrame* GetLatestFrame();

	void SetCameraTextureId(uint32_t TextureId);
	void SetDisplayGeometry(int Rotation, int Width, int Height);

	// Anchor API
	EGoogleARCoreAPIStatus CreateARAnchor(FTransform Transform, UGoogleARCoreAnchor*& OutAnchor);
	EGoogleARCoreAPIStatus CreateARAnchorFromHitTestResult(FGoogleARCoreTraceResult HitTestResult, UGoogleARCoreAnchor*& OutAnchor);
	void DetachAnchor(UGoogleARCoreAnchor* Anchor);

	void GetAllAnchors(TArray<UGoogleARCoreAnchor*>& OutAnchors) const;
	template< class T > void GetAllTrackables(TArray<T*>& OutARCoreTrackableList);
private:
	EGoogleARCoreAPIStatus SessionCreateStatus;
	FGoogleARCoreSessionConfig SessionConfig;
	FGoogleARCoreFrame* LatestFrame;
	UGoogleARCoreUObjectManager* UObjectManager;
	uint32_t CameraTextureId;
	float CachedWorldToMeterScale;

#if PLATFORM_ANDROID
	ArSession* SessionHandle = nullptr;
	ArConfig* ConfigHandle = nullptr;

	UGoogleARCoreAnchor* CreateUEAnchor(ArAnchor* AnchorHandle, FTransform AnchorWorldPose);
#endif
};

class FGoogleARCoreFrame
{
	friend class FGoogleARCoreSession;

public:
	FGoogleARCoreFrame(FGoogleARCoreSession* Session);
	~FGoogleARCoreFrame();
	
	void Init();

	void Update(float WorldToMeterScale);

	FTransform GetCameraPose() const;
	int64 GetCameraTimestamp() const;
	EGoogleARCoreTrackingState GetCameraTrackingState() const;

	void GetUpdatedAnchors(TArray<UGoogleARCoreAnchor*>& OutUpdatedAnchors) const;
	template< class T > void GetUpdatedTrackables(TArray<T*>& OutARCoreTrackableList) const;
	
	void ARLineTrace(const FVector2D& ScreenPosition, TArray<FGoogleARCoreTraceResult>& OutHitResults) const;


	bool IsDisplayRotationChanged() const;
	FMatrix GetProjectionMatrix() const;
	void TransformDisplayUvCoords(const TArray<float>& UvCoords, TArray<float>& OutUvCoords) const;

	FGoogleARCoreLightEstimate GetLightEstimate() const;
	EGoogleARCoreAPIStatus GetPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const;
	EGoogleARCoreAPIStatus AcquirePointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud) const;
#if PLATFORM_ANDROID
	EGoogleARCoreAPIStatus GetCameraMetadata(const ACameraMetadata*& OutCameraMetadata) const;
#endif

private:
	FGoogleARCoreSession* Session;
	FTransform LatestCameraPose;
	int64 LatestCameraTimestamp;
	EGoogleARCoreTrackingState LatestCameraTrackingState;

	EGoogleARCoreAPIStatus LatestPointCloudStatus;
	EGoogleARCoreAPIStatus LatestImageMetadataStatus;

	TArray<UGoogleARCoreAnchor*> UpdatedAnchors;

#if PLATFORM_ANDROID
	const ArSession* SessionHandle = nullptr;
	ArFrame* FrameHandle = nullptr;
	ArCamera* CameraHandle = nullptr;
	ArPose* SketchPoseHandle = nullptr;
	ArImageMetadata* LatestImageMetadata = nullptr;
#endif
};

#if PLATFORM_ANDROID
static ArTrackableType GetTrackableType(UClass* ClassType)
{
	if (ClassType == UGoogleARCoreTrackable::StaticClass())
	{
		return ArTrackableType::AR_TRACKABLE_BASE_TRACKABLE;
	}
	else if (ClassType == UGoogleARCorePlane::StaticClass())
	{
		return ArTrackableType::AR_TRACKABLE_PLANE;
	}
	else if (ClassType == UGoogleARCoreTrackablePoint::StaticClass())
	{
		return ArTrackableType::AR_TRACKABLE_POINT;
	}
	else
	{
		return ArTrackableType::AR_TRACKABLE_NOT_VALID;
	}
}

// Template function definition
template< class T >
T* UGoogleARCoreUObjectManager::GetTrackableFromHandle(ArTrackable* TrackableHandle, FGoogleARCoreSession* Session)
{
	if (!TrackableHandleMap.Contains(TrackableHandle) || !TrackableHandleMap[TrackableHandle].IsValid())
	{
		UGoogleARCoreTrackable* NewTrackableObject = nullptr;
		ArTrackableType TrackableType = ArTrackableType::AR_TRACKABLE_NOT_VALID;
		ArTrackable_getType(Session->GetHandle(), TrackableHandle, &TrackableType);
		if (TrackableType == ArTrackableType::AR_TRACKABLE_PLANE)
		{
			UGoogleARCorePlane* PlaneObject = NewObject<UGoogleARCorePlane>();
			NewTrackableObject = dynamic_cast<UGoogleARCoreTrackable*>(PlaneObject);
		}
		else if (TrackableType == ArTrackableType::AR_TRACKABLE_POINT)
		{
			UGoogleARCoreTrackablePoint* PointObject = NewObject<UGoogleARCoreTrackablePoint>();
			NewTrackableObject = dynamic_cast<UGoogleARCoreTrackable*>(PointObject);
		}

		// We should have a valid trackable object now.
		checkf(NewTrackableObject, TEXT("Unknow ARCore Trackable Type: %d"), TrackableType);

		NewTrackableObject->Session = Session->AsShared();
		NewTrackableObject->TrackableHandle = TrackableHandle;
		TrackableHandleMap.Add(TrackableHandle, TWeakObjectPtr<UGoogleARCoreTrackable>(NewTrackableObject));
	}
	else
	{
		// If we are not create new trackable object, release the trackable handle.
		ArTrackable_release(TrackableHandle);
	}
	T* Result = dynamic_cast<T*>(TrackableHandleMap[TrackableHandle].Get());
	checkf(Result, TEXT("UGoogleARCoreUObjectManager failed to get trackable %p from the map."), TrackableHandle);

	return Result;
}
#endif

template< class T >
void FGoogleARCoreFrame::GetUpdatedTrackables(TArray<T*>& OutARCoreTrackableList) const
{
	OutARCoreTrackableList.Empty();
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	ArTrackableType TrackableType = GetTrackableType(T::StaticClass());
	if (TrackableType == ArTrackableType::AR_TRACKABLE_NOT_VALID)
	{
		UE_LOG(LogGoogleARCoreAPI, Error, TEXT("Invalid Trackable type: %s"), *T::StaticClass()->GetName());
		return;
	}

	ArTrackableList* TrackableListHandle = nullptr;
	ArTrackableList_create(Session->GetHandle(), &TrackableListHandle);
	ArFrame_getUpdatedTrackables(Session->GetHandle(), FrameHandle, TrackableType, TrackableListHandle);

	int TrackableListSize = 0;
	ArTrackableList_getSize(Session->GetHandle(), TrackableListHandle, &TrackableListSize);

	for (int i = 0; i < TrackableListSize; i++)
	{
		ArTrackable* TrackableHandle = nullptr;
		ArTrackableList_acquireItem(Session->GetHandle(), TrackableListHandle, i, &TrackableHandle);
		T* TrackableObject = Session->GetUObjectManager()->template GetTrackableFromHandle<T>(TrackableHandle, Session);
		OutARCoreTrackableList.Add(TrackableObject);
	}
	ArTrackableList_destroy(TrackableListHandle);
#endif
}

template< class T >
void FGoogleARCoreSession::GetAllTrackables(TArray<T*>& OutARCoreTrackableList)
{
	OutARCoreTrackableList.Empty();
#if PLATFORM_ANDROID
	if (SessionHandle == nullptr)
	{
		return;
	}

	ArTrackableType TrackableType = GetTrackableType(T::StaticClass());
	if (TrackableType == ArTrackableType::AR_TRACKABLE_NOT_VALID)
	{
		UE_LOG(LogGoogleARCoreAPI, Error, TEXT("Invalid Trackable type: %s"), *T::StaticClass()->GetName());
		return;
	}

	ArTrackableList* TrackableListHandle = nullptr;
	ArTrackableList_create(SessionHandle, &TrackableListHandle);
	ArSession_getAllTrackables(SessionHandle, TrackableType, TrackableListHandle);

	int TrackableListSize = 0;
	ArTrackableList_getSize(SessionHandle, TrackableListHandle, &TrackableListSize);

	for (int i = 0; i < TrackableListSize; i++)
	{
		ArTrackable* TrackableHandle = nullptr;
		ArTrackableList_acquireItem(SessionHandle, TrackableListHandle, i, &TrackableHandle);

		T* TrackableObject = UObjectManager->template GetTrackableFromHandle<T>(TrackableHandle, this);
		OutARCoreTrackableList.Add(TrackableObject);
	}
	ArTrackableList_destroy(TrackableListHandle);
#endif
}