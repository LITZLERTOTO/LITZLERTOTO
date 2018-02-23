// Copyright 2017 Google Inc.

#include "GoogleARCorePlaneRendererComponent.h"
#include "DrawDebugHelpers.h"
#include "GoogleARCoreFunctionLibrary.h"
#include "GoogleARCoreTypes.h"

UGoogleARCorePlaneRendererComponent::UGoogleARCorePlaneRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	PlaneIndices.AddUninitialized(6);
	PlaneIndices[0] = 0; PlaneIndices[1] = 1; PlaneIndices[2] = 2;
	PlaneIndices[3] = 0; PlaneIndices[4] = 2; PlaneIndices[5] = 3;
}

void UGoogleARCorePlaneRendererComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	DrawPlanes();
}

void UGoogleARCorePlaneRendererComponent::DrawPlanes()
{
	UWorld* World = GetWorld();
	if (UGoogleARCoreFrameFunctionLibrary::GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
	{
		TArray<UGoogleARCorePlane*> PlaneList;
		UGoogleARCoreSessionFunctionLibrary::GetAllPlanes(PlaneList);
		for (UGoogleARCorePlane* Plane : PlaneList)
		{
			if (Plane->GetTrackingState() != EGoogleARCoreTrackingState::Tracking)
			{
				continue;
			}

			if (bRenderPlane)
			{
				FTransform BoundingBoxTransform = Plane->GetCenterPose();
				FVector BoundingBoxLocation = BoundingBoxTransform.GetLocation();
				FVector BoundingBoxExtent = Plane->GetExtents();

				PlaneVertices.Empty();
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxExtent.X / 2.0f, -BoundingBoxExtent.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxExtent.X / 2.0f, BoundingBoxExtent.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxExtent.X / 2.0f, BoundingBoxExtent.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxExtent.X / 2.0f, -BoundingBoxExtent.Y / 2.0f, 0)));
				// plane quad
				DrawDebugMesh(World, PlaneVertices, PlaneIndices, PlaneColor);
			}

			if (bRenderBoundaryPolygon)
			{
				TArray<FVector> BoundaryPolygonData;
				Plane->GetBoundaryPolygonInLocalSpace(BoundaryPolygonData);
				FTransform PlaneCenter = Plane->GetCenterPose();
				for (int i = 0; i < BoundaryPolygonData.Num(); i++)
				{
					FVector Start = PlaneCenter.TransformPosition(BoundaryPolygonData[i]);
					FVector End = PlaneCenter.TransformPosition(BoundaryPolygonData[(i + 1) % BoundaryPolygonData.Num()]);
					DrawDebugLine(World, Start, End, BoundaryPolygonColor, false, -1.f, 0, BoundaryPolygonThickness);
				}
			}
		}
	}
}
