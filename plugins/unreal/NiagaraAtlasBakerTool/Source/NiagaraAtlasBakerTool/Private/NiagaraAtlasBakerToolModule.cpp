#include "NiagaraAtlasBakerToolModule.h"

#include "NiagaraAtlasBakerToolWidget.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

const FName FNiagaraAtlasBakerToolModule::TabName(TEXT("NiagaraAtlasBakerTool"));

#define LOCTEXT_NAMESPACE "FNiagaraAtlasBakerToolModule"

void FNiagaraAtlasBakerToolModule::StartupModule()
{
    RegisterTabSpawner();
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNiagaraAtlasBakerToolModule::RegisterMenus));
}

void FNiagaraAtlasBakerToolModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UnregisterMenus();
    UnregisterTabSpawner();
}

void FNiagaraAtlasBakerToolModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");
    Section.AddMenuEntry(
        "OpenNiagaraAtlasBakerTool",
        LOCTEXT("MenuEntryLabel", "Niagara Atlas Baker"),
        LOCTEXT("MenuEntryTooltip", "Open the Niagara Atlas Baker tool."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.NiagaraSystem"),
        FUIAction(FExecuteAction::CreateRaw(this, &FNiagaraAtlasBakerToolModule::OnOpenPluginWindow)));
}

void FNiagaraAtlasBakerToolModule::UnregisterMenus()
{
    if (UToolMenus::TryGet())
    {
        UToolMenus::UnregisterOwner(this);
    }
}

void FNiagaraAtlasBakerToolModule::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                [
                    SNew(SNiagaraAtlasBakerToolWidget)
                ];
        }))
        .SetDisplayName(LOCTEXT("TabTitle", "Niagara Atlas Baker"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Bake Niagara Systems into atlas textures."))
        .SetMenuType(ETabSpawnerMenuType::Hidden)
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.NiagaraSystem"));
}

void FNiagaraAtlasBakerToolModule::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FNiagaraAtlasBakerToolModule::OnOpenPluginWindow()
{
    FGlobalTabmanager::Get()->TryInvokeTab(TabName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNiagaraAtlasBakerToolModule, NiagaraAtlasBakerTool)
