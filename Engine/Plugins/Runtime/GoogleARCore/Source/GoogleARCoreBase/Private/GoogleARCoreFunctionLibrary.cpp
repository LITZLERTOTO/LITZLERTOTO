// Copyright 2017 Google Inc.

#include "GoogleARCoreFunctionLibrary.h"
#include "UnrealEngine.h"
#include "Engine/Engine.h"
#include "LatentActions.h"

#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreXRTrackingSystem.h"

namespace
{
	FGoogleARCoreXRTrackingSystem* GetARCoreHMD()
	{
		if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FName("FGoogleARCoreXRTrackingSystem")))
		{
			return static_cast<FGoogleARCoreXRTrackingSystem*>(GEngine->XRSystem.Get());
		}

		return nullptr;
	}

	const float DefaultLineTraceDistance = 100000; // 1000 meter
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | Lifecycle                     */
/************************************************************************/
EGoogleARCoreSupportStatus UGoogleARCoreSessionFunctionLibrary::GetARCoreSupportStatus()
{
	return FGoogleARCoreDevice::GetInstance()->GetSupportStatus();
}

void UGoogleARCoreSessionFunctionLibrary::GetCurrentSessionConfig(FGoogleARCoreSessionConfig& OutCurrentARCoreConfig)
{
	FGoogleARCoreDevice::GetInstance()->GetCurrentSessionConfig(OutCurrentARCoreConfig);
}

void UGoogleARCoreSessionFunctionLibrary::GetSessionRequiredRuntimPermissionsWithConfig(
	const FGoogleARCoreSessionConfig& Config,
	TArray<FString>& RuntimePermissions)
{
	FGoogleARCoreDevice::GetInstance()->GetRequiredRuntimePermissionsForConfiguration(Config, RuntimePermissions);
}

EGoogleARCoreSessionStatus UGoogleARCoreSessionFunctionLibrary::GetARCoreSessionStatus()
{
	return FGoogleARCoreDevice::GetInstance()->GetSessionStatue();
}

EGoogleARCoreTrackingState UGoogleARCoreFrameFunctionLibrary::GetTrackingState()
{
	return FGoogleARCoreDevice::GetInstance()->GetTrackingState();
}

struct FARCoreStartSessionAction : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FARCoreStartSessionAction(const FLatentActionInfo& InLatentInfo)
		: FPendingLatentAction()
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		bool bSessionStartedFinished = FGoogleARCoreDevice::GetInstance()->GetStartSessionRequestFinished();
		Response.FinishAndTriggerIf(bSessionStartedFinished, ExecutionFunction, OutputLink, CallbackTarget);
	}
#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Starting ARCore tracking session"));
	}
#endif
};

void UGoogleARCoreSessionFunctionLibrary::StartSession(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FARCoreStartSessionAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FGoogleARCoreDevice::GetInstance()->StartARCoreSessionRequest();
			FARCoreStartSessionAction* NewAction = new FARCoreStartSessionAction(LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UGoogleARCoreSessionFunctionLibrary::StartSessionWithConfig(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, const FGoogleARCoreSessionConfig& Configuration)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FARCoreStartSessionAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FGoogleARCoreDevice::GetInstance()->StartARCoreSessionRequest(Configuration);
			FARCoreStartSessionAction* NewAction = new FARCoreStartSessionAction(LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UGoogleARCoreSessionFunctionLibrary::StopSession()
{
	FGoogleARCoreDevice::GetInstance()->StopARCoreSession();
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | PassthroughCamera             */
/************************************************************************/
bool UGoogleARCoreSessionFunctionLibrary::IsPassthroughCameraRenderingEnabled()
{
	FGoogleARCoreXRTrackingSystem* ARCoreHMD = GetARCoreHMD();
	if (ARCoreHMD)
	{
		return ARCoreHMD->GetColorCameraRenderingEnabled();
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to find GoogleARCoreXRTrackingSystem: GoogleARCoreXRTrackingSystem is not available."));
	}
	return false;
}

void UGoogleARCoreSessionFunctionLibrary::SetPassthroughCameraRenderingEnabled(bool bEnable)
{
	FGoogleARCoreXRTrackingSystem* ARCoreHMD = GetARCoreHMD();
	if (ARCoreHMD)
	{
		ARCoreHMD->EnableColorCameraRendering(bEnable);
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to config ARCoreXRCamera: GoogleARCoreXRTrackingSystem is not available."));
	}
}

void UGoogleARCoreSessionFunctionLibrary::GetPassthroughCameraImageUV(const TArray<float>& InUV, TArray<float>& OutUV)
{
	FGoogleARCoreDevice::GetInstance()->GetPassthroughCameraImageUVs(InUV, OutUV);
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | ARAnchor                      */
/************************************************************************/
EGoogleARCoreFunctionStatus UGoogleARCoreSessionFunctionLibrary::SpawnARAnchorActor(UObject* WorldContextObject, UClass* ARAnchorActorClass, const FTransform& ARAnchorWorldTransform, AGoogleARCoreAnchorActor*& OutARAnchorActor)
{
	UGoogleARCoreAnchor* NewARAnchorObject = nullptr;

	if (!ARAnchorActorClass->IsChildOf(AGoogleARCoreAnchorActor::StaticClass()))
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to spawn GoogleARAnchorActor. The requested ARAnchorActorClass is not a child of AGoogleARCoreAnchorActor."));
		return EGoogleARCoreFunctionStatus::InvalidType;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return EGoogleARCoreFunctionStatus::Unknown;
	}

	EGoogleARCoreFunctionStatus Status;
	Status = UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObject(ARAnchorWorldTransform, NewARAnchorObject);
	if(Status != EGoogleARCoreFunctionStatus::Success)
	{
		return Status;
	}

	OutARAnchorActor = World->SpawnActor<AGoogleARCoreAnchorActor>(ARAnchorActorClass, NewARAnchorObject->GetLatestPose());
	if (OutARAnchorActor)
	{
		OutARAnchorActor->SetARAnchor(NewARAnchorObject);
	}
	
	return EGoogleARCoreFunctionStatus::Success;
}

EGoogleARCoreFunctionStatus UGoogleARCoreSessionFunctionLibrary::SpawnARAnchorActorFromHitTest(UObject* WorldContextObject, UClass* ARAnchorActorClass, FGoogleARCoreTraceResult HitTestResult, AGoogleARCoreAnchorActor*& OutARAnchorActor)
{
	UGoogleARCoreAnchor* NewARAnchorObject = nullptr;

	if (!ARAnchorActorClass->IsChildOf(AGoogleARCoreAnchorActor::StaticClass()))
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to spawn GoogleARAnchorActor. The requested ARAnchorActorClass is not a child of AGoogleARCoreAnchorActor."));
		return EGoogleARCoreFunctionStatus::InvalidType;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return EGoogleARCoreFunctionStatus::Unknown;
	}

	EGoogleARCoreFunctionStatus Status;
	Status = UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObjectFromHitTestResult(HitTestResult, NewARAnchorObject);
	if (Status != EGoogleARCoreFunctionStatus::Success)
	{
		return Status;
	}

	OutARAnchorActor = World->SpawnActor<AGoogleARCoreAnchorActor>(ARAnchorActorClass, NewARAnchorObject->GetLatestPose());
	if (OutARAnchorActor)
	{
		OutARAnchorActor->SetARAnchor(NewARAnchorObject);
	}

	return EGoogleARCoreFunctionStatus::Success;
}

EGoogleARCoreFunctionStatus UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObject(const FTransform& ARAnchorWorldTransform, UGoogleARCoreAnchor*& OutARAnchorObject)
{
	return FGoogleARCoreDevice::GetInstance()->CreateARAnchor(ARAnchorWorldTransform, OutARAnchorObject);
}

EGoogleARCoreFunctionStatus UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObjectFromHitTestResult(FGoogleARCoreTraceResult HitTestResult, UGoogleARCoreAnchor*& OutARAnchorObject)
{
	return FGoogleARCoreDevice::GetInstance()->CreateARAnchorObjectFromHitTestResult(HitTestResult, OutARAnchorObject);
}

void UGoogleARCoreSessionFunctionLibrary::RemoveGoogleARAnchorObject(UGoogleARCoreAnchor* ARAnchorObject)
{
	return FGoogleARCoreDevice::GetInstance()->DetachARAnchor(static_cast<UGoogleARCoreAnchor*> (ARAnchorObject));
}

void UGoogleARCoreSessionFunctionLibrary::GetAllPlanes(TArray<UGoogleARCorePlane*>& OutPlaneList)
{
	FGoogleARCoreDevice::GetInstance()->GetAllTrackables<UGoogleARCorePlane>(OutPlaneList);
}

void UGoogleARCoreSessionFunctionLibrary::GetAllTrackablePoints(TArray<UGoogleARCoreTrackablePoint*>& OutTrackablePointList)
{
	FGoogleARCoreDevice::GetInstance()->GetAllTrackables<UGoogleARCoreTrackablePoint>(OutTrackablePointList);
}

void UGoogleARCoreSessionFunctionLibrary::GetAllAnchors(TArray<UGoogleARCoreAnchor*>& OutAnchorList)
{
	FGoogleARCoreDevice::GetInstance()->GetAllAnchors(OutAnchorList);
}
template< class T > 
void UGoogleARCoreSessionFunctionLibrary::GetAllTrackable(TArray<T*>& OutTrackableList) 
{
	FGoogleARCoreDevice::GetInstance()->GetAllTrackables<T>(OutTrackableList);
}

template void UGoogleARCoreSessionFunctionLibrary::GetAllTrackable<UGoogleARCoreTrackable>(TArray<UGoogleARCoreTrackable*>& OutTrackableList);
template void UGoogleARCoreSessionFunctionLibrary::GetAllTrackable<UGoogleARCorePlane>(TArray<UGoogleARCorePlane*>& OutTrackableList);
template void UGoogleARCoreSessionFunctionLibrary::GetAllTrackable<UGoogleARCoreTrackablePoint>(TArray<UGoogleARCoreTrackablePoint*>& OutTrackableList);

/************************************************************************/
/*  UGoogleARCoreFrameFunctionLibrary                                   */
/************************************************************************/
void UGoogleARCoreFrameFunctionLibrary::GetLatestPose(FTransform& LastePose)
{
	LastePose = FGoogleARCoreDevice::GetInstance()->GetLatestPose();
}

bool UGoogleARCoreFrameFunctionLibrary::ARLineTrace(UObject* WorldContextObject, const FVector2D& ScreenPosition, TSet<EGoogleARCoreLineTraceChannel> ARLineTraceChannels, TArray<FGoogleARCoreTraceResult>& OutHitResults)
{
	FGoogleARCoreDevice::GetInstance()->ARLineTrace(ScreenPosition, ARLineTraceChannels, OutHitResults);
	return OutHitResults.Num() > 0;
}

void UGoogleARCoreFrameFunctionLibrary::GetUpdatedAnchors(TArray<UGoogleARCoreAnchor*>& OutAnchorList)
{
	FGoogleARCoreDevice::GetInstance()->GetUpdatedAnchors(OutAnchorList);
}

void UGoogleARCoreFrameFunctionLibrary::GetUpdatedPlanes(TArray<UGoogleARCorePlane*>& OutPlaneList)
{
	FGoogleARCoreDevice::GetInstance()->GetUpdatedTrackables<UGoogleARCorePlane>(OutPlaneList);
}

void UGoogleARCoreFrameFunctionLibrary::GetUpdatedTrackablePoints(TArray<UGoogleARCoreTrackablePoint*>& OutTrackablePointList)
{
	FGoogleARCoreDevice::GetInstance()->GetUpdatedTrackables<UGoogleARCoreTrackablePoint>(OutTrackablePointList);
}

void UGoogleARCoreFrameFunctionLibrary::GetLatestLightEstimation(FGoogleARCoreLightEstimate& LightEstimation)
{
	LightEstimation = FGoogleARCoreDevice::GetInstance()->GetLatestLightEstimate();
}

EGoogleARCoreFunctionStatus UGoogleARCoreFrameFunctionLibrary::GetLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud)
{
	return FGoogleARCoreDevice::GetInstance()->GetLatestPointCloud(OutLatestPointCloud);
}

EGoogleARCoreFunctionStatus UGoogleARCoreFrameFunctionLibrary::AcquireLatestPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud)
{
	return FGoogleARCoreDevice::GetInstance()->AcquireLatestPointCloud(OutLatestPointCloud);
}

#if PLATFORM_ANDROID
EGoogleARCoreFunctionStatus UGoogleARCoreFrameFunctionLibrary::GetLatestCameraMetadata(const ACameraMetadata*& OutCameraMetadata)
{
	return FGoogleARCoreDevice::GetInstance()->GetLatestCameraMetadata(OutCameraMetadata);
}
#endif

template< class T >
void UGoogleARCoreFrameFunctionLibrary::GetUpdatedTrackable(TArray<T*>& OutTrackableList)
{
	FGoogleARCoreDevice::GetInstance()->GetUpdatedTrackables<T>(OutTrackableList);
}

template void UGoogleARCoreFrameFunctionLibrary::GetUpdatedTrackable<UGoogleARCoreTrackable>(TArray<UGoogleARCoreTrackable*>& OutTrackableList);
template void UGoogleARCoreFrameFunctionLibrary::GetUpdatedTrackable<UGoogleARCorePlane>(TArray<UGoogleARCorePlane*>& OutTrackableList);
template void UGoogleARCoreFrameFunctionLibrary::GetUpdatedTrackable<UGoogleARCoreTrackablePoint>(TArray<UGoogleARCoreTrackablePoint*>& OutTrackableList);