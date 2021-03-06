// Estranged is a trade mark of Alan Edwardes.

#include "UI/EstMenuWidget.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PanelWidget.h"
#include "Components/OverlaySlot.h"
#include "Gameplay/EstGameinstance.h"
#include "Gameplay/EstGameplayStatics.h"
#include "Gameplay/EstPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "EstCore.h"

void UEstMenuWidget::ShowMenu_Implementation(FName RedirectToMenu)
{
}

void UEstMenuWidget::LoadLevel_Implementation(FName LevelName)
{
}

void UEstMenuWidget::Action(FEstMenuAction Action)
{
	switch (Action.Action)
	{
	case EEstMenuAction::ResumeGame:
		ResumeGame();
		break;
	case EEstMenuAction::ExitGame:
		FGenericPlatformMisc::RequestExit(false);
		break;
	case EEstMenuAction::TransitionToLevel:
		LoadLevel(Action.Context);
		break;
	}
}

void UEstMenuWidget::AsyncNavigate(TSoftClassPtr<UEstMenuSection> MenuSection, FName Context)
{
	TWeakObjectPtr<UEstMenuWidget> WeakThis(this);

	FSoftObjectPath StreamingObjectPath = MenuSection.ToSoftObjectPath();
	TSharedPtr<FStreamableHandle> StreamingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(StreamingObjectPath, [WeakThis, Context, MenuSection]() {
		if (UEstMenuWidget* StrongThis = WeakThis.Get())
		{
			if (!MenuSection.IsValid())
			{
				return;
			}

			UEstMenuSection* Widget = NewObject<UEstMenuSection>(StrongThis, MenuSection.Get(), NAME_None, RF_Transactional);
			Widget->SetPlayerContext(FLocalPlayerContext(StrongThis->GetOwningLocalPlayer(), StrongThis->GetWorld()));
			Widget->Context = Context;
			Widget->Initialize();
			StrongThis->Navigate(Widget, Context);
		}
	},
	FStreamableManager::AsyncLoadHighPriority);
}

void UEstMenuWidget::Navigate(UEstMenuSection* MenuSection, FName Context)
{
	if (MenuSectionContainer == nullptr)
	{
		return;
	}

	RemoveMenu();

	CurrentMenuSection = MenuSection;
	CurrentMenuSection->OnNavigate.AddDynamic(this, &UEstMenuWidget::AsyncNavigate);
	CurrentMenuSection->OnAction.AddDynamic(this, &UEstMenuWidget::Action);
	CurrentMenuSection->OnModal.AddDynamic(this, &UEstMenuWidget::AsyncModal);
	MenuSectionContainer->AddChild(CurrentMenuSection);

	if (CurrentMenuModal == nullptr)
	{
		FocusMenu();
	}
}

void UEstMenuWidget::RemoveMenu()
{
	if (CurrentMenuSection != nullptr)
	{
		CurrentMenuSection->OnNavigate.RemoveAll(this);
		CurrentMenuSection->OnAction.RemoveAll(this);
		CurrentMenuSection->OnModal.RemoveAll(this);
		MenuSectionContainer->RemoveChild(CurrentMenuSection);
		CurrentMenuSection = nullptr;
	}
}

void UEstMenuWidget::FocusMenu()
{
	if (CurrentMenuSection != nullptr)
	{
		CurrentMenuSection->SetUserFocus(GetOwningPlayer());
	}
}

void UEstMenuWidget::AsyncModal(TSoftClassPtr<UEstMenuModal> MenuModal, FName Context)
{
	TWeakObjectPtr<UEstMenuWidget> WeakThis(this);

	FSoftObjectPath StreamingObjectPath = MenuModal.ToSoftObjectPath();
	TSharedPtr<FStreamableHandle> StreamingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(StreamingObjectPath, [WeakThis, Context, MenuModal]() {
		if (UEstMenuWidget* StrongThis = WeakThis.Get())
		{
			if (!MenuModal.IsValid())
			{
				return;
			}

			UEstMenuModal* Widget = NewObject<UEstMenuModal>(StrongThis, MenuModal.Get(), NAME_None, RF_Transactional);
			Widget->SetPlayerContext(FLocalPlayerContext(StrongThis->GetOwningLocalPlayer(), StrongThis->GetWorld()));
			Widget->Context = Context;
			Widget->Initialize();
			StrongThis->Modal(Widget, Context);
		}
	},
	FStreamableManager::AsyncLoadHighPriority);
}

void UEstMenuWidget::Modal(UEstMenuModal* MenuModal, FName Context)
{
	if (MenuModalContainer == nullptr)
	{
		return;
	}

	RemoveModal();

	CurrentMenuModal = MenuModal;
	CurrentMenuModal->Context = Context;
	CurrentMenuModal->OnExit.AddDynamic(this, &UEstMenuWidget::ExitModal);
	UOverlaySlot* OverlaySlot = MenuModalContainer->AddChildToOverlay(CurrentMenuModal);
	OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
	OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	CurrentMenuModal->SetUserFocus(GetOwningPlayer());
}

void UEstMenuWidget::RemoveModal()
{
	if (CurrentMenuModal != nullptr)
	{
		CurrentMenuModal->OnExit.RemoveAll(this);
		MenuModalContainer->RemoveChild(CurrentMenuModal);
		CurrentMenuModal = nullptr;
	}
}

void UEstMenuWidget::ExitModal()
{
	RemoveModal();
	FocusMenu();
}

FReply UEstMenuWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	UWidget* WidgetToFocus = CurrentMenuModal != nullptr ? Cast<UWidget>(CurrentMenuModal) : CurrentMenuSection;
	if (WidgetToFocus != nullptr)
	{
		TSharedPtr<SWidget> CapturingSlateWidget = WidgetToFocus->GetCachedWidget();
		if (CapturingSlateWidget.IsValid())
		{
			return FReply::Handled().SetUserFocus(CapturingSlateWidget.ToSharedRef(), InFocusEvent.GetCause(), false);
		}
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UEstMenuWidget::ResumeGame()
{
	UEstGameInstance* EstGameInstance = Cast<UEstGameInstance>(GetWorld()->GetGameInstance());
	AEstPlayerController* EstPlayerController = Cast<AEstPlayerController>(GetOwningPlayer());

	if (EstGameInstance != nullptr && EstPlayerController != nullptr && !EstGameInstance->GetMenuVisibleForever())
	{
		EstGameInstance->SetMenuVisibility(FEstMenuVisibilityContext());
		EstPlayerController->SetMenuFocusState(false);
		EstPlayerController->SetPause(false);
	}
}
