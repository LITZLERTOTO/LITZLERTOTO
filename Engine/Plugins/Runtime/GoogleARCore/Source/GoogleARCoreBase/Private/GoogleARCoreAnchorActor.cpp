// Copyright 2017 Google Inc.
#include "GoogleARCoreAnchorActor.h"
#include "GoogleARCoreFunctionLibrary.h"

void AGoogleARCoreAnchorActor::Tick(float DeltaSeconds)
{
	if (bTrackingEnabled && ARAnchorObject != nullptr && ARAnchorObject->GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
	{
		SetActorTransform(ARAnchorObject->GetLatestPose());
	}

	if ((bHideWhenNotCurrentlyTracking || bDestroyWhenStoppedTracking) && ARAnchorObject != nullptr)
	{
		switch (ARAnchorObject->GetTrackingState())
		{
		case EGoogleARCoreTrackingState::Tracking:
			SetActorHiddenInGame(false);
			break;
		case EGoogleARCoreTrackingState::NotTracking:
			SetActorHiddenInGame(bHideWhenNotCurrentlyTracking);
			break;
		case EGoogleARCoreTrackingState::StoppedTracking:
			if (bDestroyWhenStoppedTracking)
			{
				Destroy();
			}
			else
			{
				SetActorHiddenInGame(bHideWhenNotCurrentlyTracking);
			}
			break;
		default:
			// This case should never be reached, do nothing.
			break;
		}
	}

	Super::Tick(DeltaSeconds);
}

void AGoogleARCoreAnchorActor::BeginDestroy()
{
	if (bRemoveAnchorObjectWhenDestroyed && ARAnchorObject)
	{
		UGoogleARCoreSessionFunctionLibrary::RemoveARAnchorObject(ARAnchorObject);
	}

	Super::BeginDestroy();
}

void AGoogleARCoreAnchorActor::SetARAnchor(UGoogleARCoreAnchor* InARAnchorObject)
{
	if (ARAnchorObject != nullptr && bRemoveAnchorObjectWhenAnchorChanged)
	{
		UGoogleARCoreSessionFunctionLibrary::RemoveARAnchorObject(ARAnchorObject);
	}

	ARAnchorObject = InARAnchorObject;
	SetActorTransform(ARAnchorObject->GetLatestPose());
}

UGoogleARCoreAnchor* AGoogleARCoreAnchorActor::GetARAnchor()
{
	return ARAnchorObject;
}