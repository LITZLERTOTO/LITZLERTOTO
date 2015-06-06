// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMenu.h"

/**
 * Represents the base class of popup menus.
 */
class FMenuBase : public IMenu, public TSharedFromThis<FMenuBase>
{
public:
	virtual FOnMenuDismissed& GetOnMenuDismissed() override { return OnMenuDismissed; }
	virtual TSharedPtr<SWidget> GetContent() const override { return Content; }

protected:
	FMenuBase(TSharedRef<SWidget> InContent);

	FOnMenuDismissed OnMenuDismissed;
	TSharedRef<SWidget> Content;
	bool bDismissing;
};

/**
 * Represents a popup menu that is shown in its own SWindow.
 */
class FMenuInWindow : public FMenuBase
{
public:
	FMenuInWindow(TSharedRef<SWindow> InWindow, TSharedRef<SWidget> InContent);

	virtual EPopupMethod GetPopupMethod() const override { return EPopupMethod::CreateNewWindow; }
	virtual TSharedPtr<SWindow> GetParentWindow() const override;
	virtual TSharedPtr<SWindow> GetOwnedWindow() const override { return GetParentWindow(); }
	virtual void Dismiss() override;

private:
	TWeakPtr<SWindow> Window;
};

/**
 * Represents a popup menu that is shown in the same window as the widget that summons it.
 */
class FMenuInPopup : public FMenuBase
{
public:
	FMenuInPopup(TSharedRef<SWidget> InContent);

	virtual EPopupMethod GetPopupMethod() const { return EPopupMethod::UseCurrentWindow; }
	virtual TSharedPtr<SWindow> GetParentWindow() const;
	virtual TSharedPtr<SWindow> GetOwnedWindow() const { return TSharedPtr<SWindow>(); }
	virtual void Dismiss() override;
};

/**
* Represents a popup menu that is shown in a host widget (such as a menu anchor).
*/
class FMenuInHostWidget : public FMenuBase
{
public:
	FMenuInHostWidget(TSharedRef<IMenuHost> InHost, const TSharedRef<SWidget>& InContent);

	virtual EPopupMethod GetPopupMethod() const { return EPopupMethod::UseCurrentWindow; }
	virtual TSharedPtr<SWindow> GetParentWindow() const;
	virtual TSharedPtr<SWindow> GetOwnedWindow() const { return TSharedPtr<SWindow>(); }
	virtual void Dismiss() override;

private:
	TWeakPtr<IMenuHost> MenuHost;
};