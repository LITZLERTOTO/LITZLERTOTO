// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common.h"

THIRD_PARTY_INCLUDES_START
	#pragma pack(push, 8)
		#include "clang/Frontend/FrontendAction.h"
	#pragma pack(pop)
THIRD_PARTY_INCLUDES_END

namespace clang
{
	class CompilerInstance;
	class SourceManager;
}

namespace UnrealCodeAnalyzer
{
	class FMacroCallback;
	class FIncludeDirectiveCallback;
	class FConsumer;
	class FAction : public clang::ASTFrontendAction
	{
	public:
		virtual clang::ASTConsumer* CreateASTConsumer(clang::CompilerInstance& Compiler, clang::StringRef InFile) override;
		virtual bool BeginSourceFileAction(clang::CompilerInstance &CI, clang::StringRef Filename) override;
		virtual void EndSourceFileAction() override;

		friend class FIncludeDirectiveCallback;
	private:
		FIncludeDirectiveCallback* IncludeDirectiveCallback;
		FMacroCallback* MacroCallback;
		FConsumer* ASTConsumer;
		clang::StringRef Filename;
		clang::SourceManager* SourceManager;
		TMap<FString, TDoubleLinkedList<FString>> IncludeChains;
	};
}
