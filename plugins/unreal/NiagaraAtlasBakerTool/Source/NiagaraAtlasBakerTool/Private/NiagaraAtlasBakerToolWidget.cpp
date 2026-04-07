#include "NiagaraAtlasBakerToolWidget.h"

#include "NiagaraAtlasBakerToolBaker.h"
#include "NiagaraAtlasBakerToolSettings.h"
#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraSystem.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
UNiagaraSystem* ResolvePreferredNiagara()
{
    const TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
    for (UObject* Asset : SelectedAssets)
    {
        if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Asset))
        {
            return NiagaraSystem;
        }
    }

    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            const TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Asset))
                {
                    return NiagaraSystem;
                }
            }
        }
    }

    return nullptr;
}

void ShowNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
{
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 5.0f;
    if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
    {
        Notification->SetCompletionState(CompletionState);
    }
}
}

void SNiagaraAtlasBakerToolWidget::Construct(const FArguments& InArgs)
{
    Settings.Reset(NewObject<UNiagaraAtlasBakerToolSettings>(GetTransientPackage(), NAME_None, RF_Transient));
    Settings->NiagaraSystem = ResolvePreferredNiagara();

    FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsViewArgs;
    DetailsViewArgs.bAllowSearch = true;
    DetailsViewArgs.bHideSelectionTip = true;
    DetailsViewArgs.bLockable = false;
    DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    DetailsViewArgs.bShowPropertyMatrixButton = false;
    DetailsViewArgs.bUpdatesFromSelection = false;

    DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
    DetailsView->SetObject(Settings.Get());

    ResetNamesToDefaults();
    StatusText = FText::FromString(TEXT("Select a Niagara System. Atlas framing follows the native Niagara Baker camera, so adjust that view in the Niagara editor before baking if you need to zoom or reposition the effect."));

    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Niagara Atlas Baker")))
                .Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 12)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Bake the selected Niagara System directly into a single atlas PNG and optionally import it back into Unreal as a Texture2D.")))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                DetailsView.ToSharedRef()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 12, 0, 12)
            [
                SNew(SSeparator)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text(this, &SNiagaraAtlasBakerToolWidget::GetSummaryText)
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 10)
            [
                SNew(STextBlock)
                .Text(this, &SNiagaraAtlasBakerToolWidget::GetBakerFramingText)
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f)))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SUniformGridPanel)
                .SlotPadding(6.0f)
                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Use Selected/Open Niagara")))
                    .OnClicked(this, &SNiagaraAtlasBakerToolWidget::OnUseSelectedNiagaraClicked)
                ]
                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Open Niagara Editor")))
                    .OnClicked(this, &SNiagaraAtlasBakerToolWidget::OnOpenNiagaraEditorClicked)
                ]
                + SUniformGridPanel::Slot(2, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Reset Names")))
                    .OnClicked(this, &SNiagaraAtlasBakerToolWidget::OnResetNamesClicked)
                ]
                + SUniformGridPanel::Slot(3, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Open Output Folder")))
                    .OnClicked(this, &SNiagaraAtlasBakerToolWidget::OnOpenOutputFolderClicked)
                ]
                + SUniformGridPanel::Slot(4, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Bake Atlas")))
                    .ButtonColorAndOpacity(FLinearColor(0.16f, 0.38f, 0.28f))
                    .OnClicked(this, &SNiagaraAtlasBakerToolWidget::OnBakeClicked)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 12, 0, 0)
            [
                SNew(STextBlock)
                .Text(this, &SNiagaraAtlasBakerToolWidget::GetStatusText)
                .ColorAndOpacity(this, &SNiagaraAtlasBakerToolWidget::GetStatusColor)
                .AutoWrapText(true)
            ]
        ]
    ];
}

FReply SNiagaraAtlasBakerToolWidget::OnUseSelectedNiagaraClicked()
{
    if (UNiagaraSystem* NiagaraSystem = ResolvePreferredNiagara())
    {
        Settings->NiagaraSystem = NiagaraSystem;
        ResetNamesToDefaults();
        DetailsView->ForceRefresh();
        SetStatus(FText::FromString(FString::Printf(TEXT("Using Niagara System: %s"), *NiagaraSystem->GetName())), false);
    }
    else
    {
        SetStatus(FText::FromString(TEXT("No Niagara System found in the content browser selection or open editors.")), true);
    }

    return FReply::Handled();
}

FReply SNiagaraAtlasBakerToolWidget::OnOpenNiagaraEditorClicked()
{
    if (UNiagaraSystem* NiagaraSystem = GetCurrentNiagaraSystem(); NiagaraSystem == nullptr)
    {
        SetStatus(FText::FromString(TEXT("Select a Niagara System first.")), true);
        return FReply::Handled();
    }

    if (!GEditor)
    {
        SetStatus(FText::FromString(TEXT("The Unreal Editor instance is not available.")), true);
        return FReply::Handled();
    }

    if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        AssetEditorSubsystem->OpenEditorForAsset(GetCurrentNiagaraSystem());
        SetStatus(FText::FromString(TEXT("Opened the Niagara asset editor. Adjust the native Baker preview framing there, then return here and click Bake Atlas.")), false);
    }

    return FReply::Handled();
}

FReply SNiagaraAtlasBakerToolWidget::OnBakeClicked()
{
    if (!Settings.Get())
    {
        SetStatus(FText::FromString(TEXT("Tool settings are not available.")), true);
        return FReply::Handled();
    }

    FNiagaraAtlasBakeResult Result;
    FText Error;
    if (!FNiagaraAtlasBakerToolBaker::BakeAtlas(Settings.Get(), Result, Error))
    {
        SetStatus(Error, true);
        ShowNotification(Error, SNotificationItem::CS_Fail);
        FMessageDialog::Open(EAppMsgType::Ok, Error);
        return FReply::Handled();
    }

    LastOutputDirectory = FPaths::GetPath(Result.OutputFilePath);

    FString SuccessMessage = FString::Printf(
        TEXT("Atlas baked successfully.\nPNG: %s\nFrames: %d (%dx%d)\nAtlas Size: %dx%d"),
        *Result.OutputFilePath,
        Result.FrameCount,
        Result.AtlasGrid.X,
        Result.AtlasGrid.Y,
        Result.AtlasSize.X,
        Result.AtlasSize.Y);

    if (!Result.ImportedAssetPath.IsEmpty())
    {
        SuccessMessage += FString::Printf(TEXT("\nImported Asset: %s"), *Result.ImportedAssetPath);
    }

    SetStatus(FText::FromString(SuccessMessage), false);
    ShowNotification(FText::FromString(TEXT("Niagara atlas bake completed.")), SNotificationItem::CS_Success);
    return FReply::Handled();
}

FReply SNiagaraAtlasBakerToolWidget::OnOpenOutputFolderClicked()
{
    FString FolderToOpen = LastOutputDirectory;
    if (FolderToOpen.IsEmpty())
    {
        if (UNiagaraSystem* NiagaraSystem = GetCurrentNiagaraSystem(); NiagaraSystem != nullptr && Settings.Get())
        {
            if (Settings->bUseNiagaraFolderForOutput)
            {
                const FString PackageFolder = FPaths::GetPath(NiagaraSystem->GetOutermost()->GetName());
                FPackageName::TryConvertLongPackageNameToFilename(PackageFolder, FolderToOpen, TEXT(""));
            }
            else
            {
                FolderToOpen = FPaths::ConvertRelativePathToFull(Settings->OutputDirectory.Path);
            }
        }
    }

    if (FolderToOpen.IsEmpty())
    {
        SetStatus(FText::FromString(TEXT("No output folder is available yet.")), true);
        return FReply::Handled();
    }

    FPlatformProcess::ExploreFolder(*FolderToOpen);
    return FReply::Handled();
}

FReply SNiagaraAtlasBakerToolWidget::OnResetNamesClicked()
{
    ResetNamesToDefaults();
    DetailsView->ForceRefresh();
    SetStatus(FText::FromString(TEXT("Atlas file and asset names have been reset to the Niagara-based defaults.")), false);
    return FReply::Handled();
}

FText SNiagaraAtlasBakerToolWidget::GetStatusText() const
{
    return StatusText;
}

FSlateColor SNiagaraAtlasBakerToolWidget::GetStatusColor() const
{
    return bStatusIsError ? FSlateColor(FLinearColor(0.85f, 0.2f, 0.2f)) : FSlateColor(FLinearColor(0.2f, 0.6f, 0.3f));
}

FText SNiagaraAtlasBakerToolWidget::GetBakerFramingText() const
{
    if (!Settings.Get())
    {
        return FText::FromString(TEXT("Framing/zoom comes from the selected Niagara asset's native Baker camera."));
    }

    UNiagaraSystem* NiagaraSystem = GetCurrentNiagaraSystem();
    if (!NiagaraSystem)
    {
        return FText::FromString(TEXT("Framing/zoom comes from the selected Niagara asset's native Baker camera."));
    }

    const UNiagaraBakerSettings* BakerSettings = NiagaraSystem->GetBakerSettings();
    if (!BakerSettings)
    {
        return FText::FromString(TEXT("This Niagara asset does not have Baker settings yet. Open it in the Niagara editor and configure Baker framing there first."));
    }

    const FNiagaraBakerCameraSettings& CurrentCamera = BakerSettings->GetCurrentCamera();
    if (CurrentCamera.IsOrthographic())
    {
        return FText::FromString(FString::Printf(
            TEXT("Framing uses the native Niagara Baker camera. Current mode: Orthographic, Ortho Width: %.2f. If the effect looks too large or too small in the atlas, open the Niagara editor and adjust the Baker preview framing there first."),
            CurrentCamera.OrthoWidth));
    }

    return FText::FromString(FString::Printf(
        TEXT("Framing uses the native Niagara Baker camera. Current mode: Perspective, FOV: %.2f. If the effect looks too large or too small in the atlas, open the Niagara editor and adjust the Baker preview framing there first."),
        CurrentCamera.FOV));
}

FText SNiagaraAtlasBakerToolWidget::GetSummaryText() const
{
    if (!Settings.Get())
    {
        return FText::FromString(TEXT("Current target: none"));
    }

    UNiagaraSystem* NiagaraSystem = GetCurrentNiagaraSystem();
    if (!NiagaraSystem)
    {
        return FText::FromString(TEXT("Current target: none"));
    }

    const UNiagaraBakerSettings* BakerSettings = NiagaraSystem->GetBakerSettings();
    const int32 FramesX = (Settings->bUseBakerGrid && BakerSettings) ? BakerSettings->FramesPerDimension.X : Settings->FramesX;
    const int32 FramesY = (Settings->bUseBakerGrid && BakerSettings) ? BakerSettings->FramesPerDimension.Y : Settings->FramesY;
    const int32 AtlasWidth = Settings->FrameWidth * FMath::Max(1, FramesX);
    const int32 AtlasHeight = Settings->FrameHeight * FMath::Max(1, FramesY);

    return FText::FromString(FString::Printf(
        TEXT("Target: %s | Grid: %dx%d | Frame: %dx%d | Atlas: %dx%d"),
        *NiagaraSystem->GetName(),
        FramesX,
        FramesY,
        Settings->FrameWidth,
        Settings->FrameHeight,
        AtlasWidth,
        AtlasHeight));
}

UNiagaraSystem* SNiagaraAtlasBakerToolWidget::GetCurrentNiagaraSystem() const
{
    if (!Settings.Get())
    {
        return nullptr;
    }

    return Settings->NiagaraSystem.Get();
}

void SNiagaraAtlasBakerToolWidget::SetStatus(const FText& InStatus, bool bIsError)
{
    StatusText = InStatus;
    bStatusIsError = bIsError;
}

void SNiagaraAtlasBakerToolWidget::ResetNamesToDefaults()
{
    if (!Settings.Get())
    {
        return;
    }

    Settings->AtlasFileName = Settings->ResolveDefaultAtlasFileName();
    Settings->AtlasAssetName = Settings->ResolveDefaultAtlasAssetName();
    if (Settings->bUseNiagaraFolderForImport || Settings->ImportDestinationPath.IsEmpty())
    {
        Settings->ImportDestinationPath = Settings->ResolveDefaultImportPath();
    }
}
