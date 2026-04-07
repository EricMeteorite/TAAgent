#pragma once

#include "CoreMinimal.h"
#include "NiagaraAtlasBakerToolSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UNiagaraSystem;

class SNiagaraAtlasBakerToolWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNiagaraAtlasBakerToolWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnUseSelectedNiagaraClicked();
    FReply OnOpenNiagaraEditorClicked();
    FReply OnBakeClicked();
    FReply OnOpenOutputFolderClicked();
    FReply OnResetNamesClicked();

    FText GetBakerFramingText() const;
    FText GetStatusText() const;
    FSlateColor GetStatusColor() const;
    FText GetSummaryText() const;
    UNiagaraSystem* GetCurrentNiagaraSystem() const;

    void SetStatus(const FText& InStatus, bool bIsError);
    void ResetNamesToDefaults();

    TSharedPtr<IDetailsView> DetailsView;
    TStrongObjectPtr<UNiagaraAtlasBakerToolSettings> Settings;
    FText StatusText;
    bool bStatusIsError = false;
    FString LastOutputDirectory;
};
