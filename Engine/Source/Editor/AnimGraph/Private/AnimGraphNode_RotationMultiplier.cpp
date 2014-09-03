// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_RotationMultiplier.h"
/////////////////////////////////////////////////////
// UAnimGraphNode_RotationMultiplier

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_RotationMultiplier::UAnimGraphNode_RotationMultiplier(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FText UAnimGraphNode_RotationMultiplier::GetControllerDescription() const
{
	return LOCTEXT("ApplyPercentageOfRotation", "Apply a percentage of Rotation");
}

FText UAnimGraphNode_RotationMultiplier::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_RotationMultiplier_Tooltip", "The Apply a Percentage of Rotation control drives the Rotation of a target bone at some specified percentage of the Rotation of another bone within the Skeleton.");
}

FText UAnimGraphNode_RotationMultiplier::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
	Args.Add(TEXT("BoneName"), FText::FromName(Node.TargetBone.BoneName));

	if(TitleType == ENodeTitleType::ListView)
	{
		if (Node.TargetBone.BoneName == NAME_None)
		{
			return FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_MenuTitle", "{ControllerDescription}"), Args);
		}
		return FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args);
	}
	else
	{
		return FText::Format(LOCTEXT("AnimGraphNode_ModifyBone_Title", "{ControllerDescription}\nBone: {BoneName}"), Args);
	}
}

#undef LOCTEXT_NAMESPACE
