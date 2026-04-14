#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPEnvironmentCommands.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "AssetSelection.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "AssetImportTask.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

// Niagara System creation support
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"

namespace
{
    FString PythonFileExecutionScopeToString(const EPythonFileExecutionScope Scope)
    {
        switch (Scope)
        {
        case EPythonFileExecutionScope::Private:
            return TEXT("Private");
        case EPythonFileExecutionScope::Public:
            return TEXT("Public");
        default:
            return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Scope));
        }
    }

    bool TryParsePythonFileExecutionScope(const FString& ScopeString, EPythonFileExecutionScope& OutScope)
    {
        if (ScopeString.Equals(TEXT("Private"), ESearchCase::IgnoreCase))
        {
            OutScope = EPythonFileExecutionScope::Private;
            return true;
        }

        if (ScopeString.Equals(TEXT("Public"), ESearchCase::IgnoreCase))
        {
            OutScope = EPythonFileExecutionScope::Public;
            return true;
        }

        return false;
    }

    TSharedPtr<FJsonObject> CreateAssetSummary(UObject* Asset)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Asset)
        {
            Json->SetBoolField(TEXT("valid"), false);
            return Json;
        }

        UPackage* Package = Asset->GetOutermost();
        Json->SetBoolField(TEXT("valid"), true);
        Json->SetStringField(TEXT("name"), Asset->GetName());
        Json->SetStringField(TEXT("asset_name"), Asset->GetName());
        Json->SetStringField(TEXT("asset_path"), Asset->GetPathName());
        Json->SetStringField(TEXT("package_name"), Package ? Package->GetName() : TEXT(""));
        Json->SetStringField(TEXT("class_name"), Asset->GetClass()->GetName());
        Json->SetStringField(TEXT("class_path"), Asset->GetClass()->GetPathName());
        Json->SetBoolField(TEXT("is_dirty"), Package ? Package->IsDirty() : false);
        return Json;
    }

    TSharedPtr<FJsonObject> CreateAssetDataSummary(const FAssetData& AssetData)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("valid"), AssetData.IsValid());
        Json->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        Json->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
        Json->SetStringField(TEXT("asset_path"), AssetData.GetSoftObjectPath().ToString());
        Json->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
        Json->SetStringField(TEXT("class_name"), AssetData.AssetClassPath.GetAssetName().ToString());
        Json->SetStringField(TEXT("class_path"), AssetData.AssetClassPath.ToString());
        Json->SetBoolField(TEXT("is_loaded"), AssetData.IsAssetLoaded());
        return Json;
    }

    TArray<UObject*> GetAssetsEditedByInstance(UAssetEditorSubsystem* AssetEditorSubsystem, IAssetEditorInstance* Instance)
    {
        TArray<UObject*> EditedAssets;
        if (!AssetEditorSubsystem || !Instance)
        {
            return EditedAssets;
        }

        const TArray<UObject*> OpenAssets = AssetEditorSubsystem->GetAllEditedAssets();
        for (UObject* Asset : OpenAssets)
        {
            if (!Asset)
            {
                continue;
            }

            const TArray<IAssetEditorInstance*> EditorsForAsset = AssetEditorSubsystem->FindEditorsForAsset(Asset);
            if (EditorsForAsset.Contains(Instance))
            {
                EditedAssets.Add(Asset);
            }
        }

        return EditedAssets;
    }

    TSharedPtr<FJsonObject> CreateTabSummary(const TSharedPtr<SDockTab>& Tab)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("valid"), Tab.IsValid());
        if (!Tab.IsValid())
        {
            return Json;
        }

        Json->SetStringField(TEXT("label"), Tab->GetTabLabel().ToString());
        Json->SetStringField(TEXT("tab_id"), Tab->GetLayoutIdentifier().ToString());
        return Json;
    }

    TSharedPtr<FJsonObject> CreateEditorInstanceSummary(UAssetEditorSubsystem* AssetEditorSubsystem, IAssetEditorInstance* Instance)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("valid"), Instance != nullptr);
        if (!AssetEditorSubsystem || !Instance)
        {
            return Json;
        }

        Json->SetStringField(TEXT("editor_name"), Instance->GetEditorName().ToString());
        Json->SetStringField(TEXT("asset_type_name"), Instance->GetEditingAssetTypeName().ToString());
        Json->SetBoolField(TEXT("is_primary_editor"), Instance->IsPrimaryEditor());
        Json->SetNumberField(TEXT("last_activation_time"), Instance->GetLastActivationTime());

        TArray<TSharedPtr<FJsonValue>> AssetsJson;
        const TArray<UObject*> EditedAssets = GetAssetsEditedByInstance(AssetEditorSubsystem, Instance);
        for (UObject* Asset : EditedAssets)
        {
            AssetsJson.Add(MakeShared<FJsonValueObject>(CreateAssetSummary(Asset)));
        }
        Json->SetArrayField(TEXT("assets"), AssetsJson);

        TSharedPtr<FTabManager> TabManager = Instance->GetAssociatedTabManager();
        Json->SetBoolField(TEXT("has_tab_manager"), TabManager.IsValid());
        if (TabManager.IsValid())
        {
            Json->SetObjectField(TEXT("major_tab"), CreateTabSummary(FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef())));
        }

        return Json;
    }

    IAssetEditorInstance* FindBestActiveEditor(UAssetEditorSubsystem* AssetEditorSubsystem)
    {
        if (!AssetEditorSubsystem)
        {
            return nullptr;
        }

        const TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->GetAllOpenEditors();
        const TSharedPtr<SDockTab> ActiveGlobalTab = FGlobalTabmanager::Get()->GetActiveTab();

        IAssetEditorInstance* BestEditor = nullptr;
        double BestScore = -1.0;

        for (IAssetEditorInstance* Editor : OpenEditors)
        {
            if (!Editor)
            {
                continue;
            }

            double Score = Editor->GetLastActivationTime();
            TSharedPtr<FTabManager> TabManager = Editor->GetAssociatedTabManager();
            if (ActiveGlobalTab.IsValid() && TabManager.IsValid())
            {
                if (FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef()) == ActiveGlobalTab)
                {
                    Score += 500000.0;
                }
            }

            if (!BestEditor || Score > BestScore)
            {
                BestEditor = Editor;
                BestScore = Score;
            }
        }

        return BestEditor;
    }

    UWorld* GetEditorWorld()
    {
        if (!GEditor)
        {
            return nullptr;
        }

        return GEditor->GetEditorWorldContext().World();
    }
}

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_editor_context"))
    {
        return HandleGetEditorContext(Params);
    }
    else if (CommandType == TEXT("get_open_asset_editors"))
    {
        return HandleGetOpenAssetEditors(Params);
    }
    else if (CommandType == TEXT("get_selected_assets"))
    {
        return HandleGetSelectedAssets(Params);
    }
    else if (CommandType == TEXT("get_selected_actors"))
    {
        return HandleGetSelectedActors(Params);
    }
    else if (CommandType == TEXT("open_asset"))
    {
        return HandleOpenAsset(Params);
    }
    else if (CommandType == TEXT("focus_asset_editor"))
    {
        return HandleFocusAssetEditor(Params);
    }
    else if (CommandType == TEXT("close_asset_editors"))
    {
        return HandleCloseAssetEditors(Params);
    }
    else if (CommandType == TEXT("save_asset"))
    {
        return HandleSaveAsset(Params);
    }
    else if (CommandType == TEXT("execute_unreal_python"))
    {
        return HandleExecuteUnrealPython(Params);
    }

    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // FBX import
    else if (CommandType == TEXT("import_fbx"))
    {
        return HandleImportFBX(Params);
    }
    // Generic Asset Management (通用资产操作)
    else if (CommandType == TEXT("create_asset"))
    {
        return HandleCreateAsset(Params);
    }
    else if (CommandType == TEXT("delete_asset"))
    {
        return HandleDeleteAsset(Params);
    }
    else if (CommandType == TEXT("set_asset_properties"))
    {
        return HandleSetAssetProperties(Params);
    }
    else if (CommandType == TEXT("get_asset_properties"))
    {
        return HandleGetAssetProperties(Params);
    }
    else if (CommandType == TEXT("batch_create_assets"))
    {
        return HandleBatchCreateAssets(Params);
    }
    else if (CommandType == TEXT("batch_set_assets_properties"))
    {
        return HandleBatchSetAssetsProperties(Params);
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetEditorContext(const TSharedPtr<FJsonObject>& Params)
{
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);

    UWorld* World = GetEditorWorld();
    if (World)
    {
        ResultObj->SetStringField(TEXT("current_level"), World->GetOutermost()->GetName());
    }

    ResultObj->SetObjectField(TEXT("active_global_tab"), CreateTabSummary(FGlobalTabmanager::Get()->GetActiveTab()));

    TArray<TSharedPtr<FJsonValue>> SelectedAssetsJson;
    TArray<FAssetData> SelectedAssets;
    AssetSelectionUtils::GetSelectedAssets(SelectedAssets);
    for (const FAssetData& AssetData : SelectedAssets)
    {
        SelectedAssetsJson.Add(MakeShared<FJsonValueObject>(CreateAssetDataSummary(AssetData)));
    }
    ResultObj->SetArrayField(TEXT("selected_assets"), SelectedAssetsJson);
    ResultObj->SetNumberField(TEXT("selected_asset_count"), SelectedAssets.Num());

    TArray<TSharedPtr<FJsonValue>> SelectedActorsJson;
    if (GEditor)
    {
        for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
        {
            if (AActor* Actor = Cast<AActor>(*It))
            {
                SelectedActorsJson.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
            }
        }
    }
    ResultObj->SetArrayField(TEXT("selected_actors"), SelectedActorsJson);
    ResultObj->SetNumberField(TEXT("selected_actor_count"), SelectedActorsJson.Num());

    TArray<TSharedPtr<FJsonValue>> OpenAssetsJson;
    const TArray<UObject*> OpenAssets = AssetEditorSubsystem->GetAllEditedAssets();
    for (UObject* Asset : OpenAssets)
    {
        OpenAssetsJson.Add(MakeShared<FJsonValueObject>(CreateAssetSummary(Asset)));
    }
    ResultObj->SetArrayField(TEXT("open_assets"), OpenAssetsJson);
    ResultObj->SetNumberField(TEXT("open_asset_count"), OpenAssets.Num());

    TArray<TSharedPtr<FJsonValue>> OpenEditorsJson;
    const TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->GetAllOpenEditors();
    for (IAssetEditorInstance* Editor : OpenEditors)
    {
        OpenEditorsJson.Add(MakeShared<FJsonValueObject>(CreateEditorInstanceSummary(AssetEditorSubsystem, Editor)));
    }
    ResultObj->SetArrayField(TEXT("open_asset_editors"), OpenEditorsJson);
    ResultObj->SetNumberField(TEXT("open_asset_editor_count"), OpenEditors.Num());

    if (IAssetEditorInstance* ActiveEditor = FindBestActiveEditor(AssetEditorSubsystem))
    {
        ResultObj->SetObjectField(TEXT("active_asset_editor"), CreateEditorInstanceSummary(AssetEditorSubsystem, ActiveEditor));
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetOpenAssetEditors(const TSharedPtr<FJsonObject>& Params)
{
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    TArray<TSharedPtr<FJsonValue>> EditorsJson;
    const TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->GetAllOpenEditors();
    for (IAssetEditorInstance* Editor : OpenEditors)
    {
        EditorsJson.Add(MakeShared<FJsonValueObject>(CreateEditorInstanceSummary(AssetEditorSubsystem, Editor)));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("open_asset_editors"), EditorsJson);
    ResultObj->SetNumberField(TEXT("count"), OpenEditors.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FAssetData> SelectedAssets;
    AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

    TArray<TSharedPtr<FJsonValue>> AssetsJson;
    for (const FAssetData& AssetData : SelectedAssets)
    {
        AssetsJson.Add(MakeShared<FJsonValueObject>(CreateAssetDataSummary(AssetData)));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("selected_assets"), AssetsJson);
    ResultObj->SetNumberField(TEXT("count"), SelectedAssets.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> ActorsJson;
    if (GEditor)
    {
        for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
        {
            if (AActor* Actor = Cast<AActor>(*It))
            {
                ActorsJson.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("selected_actors"), ActorsJson);
    ResultObj->SetNumberField(TEXT("count"), ActorsJson.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleOpenAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    const bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);
    if (!bOpened)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open asset editor for: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetObjectField(TEXT("asset"), CreateAssetSummary(Asset));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFocusAssetEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Asset, true);
    if (!EditorInstance)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset editor is not open for: %s"), *AssetPath));
    }

    EditorInstance->FocusWindow(Asset);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetObjectField(TEXT("editor"), CreateEditorInstanceSummary(AssetEditorSubsystem, EditorInstance));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCloseAssetEditors(const TSharedPtr<FJsonObject>& Params)
{
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    bool bCloseAll = false;
    Params->TryGetBoolField(TEXT("close_all"), bCloseAll);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);

    if (bCloseAll)
    {
        ResultObj->SetBoolField(TEXT("closed_all"), AssetEditorSubsystem->CloseAllAssetEditors());
        return ResultObj;
    }

    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter or set 'close_all' to true"));
    }

    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    const int32 ClosedCount = AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetNumberField(TEXT("closed_editor_count"), ClosedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    const bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSaved);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetObjectField(TEXT("asset"), CreateAssetSummary(Asset));
    if (!bSaved)
    {
        ResultObj->SetStringField(TEXT("error"), TEXT("Failed to save asset"));
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleExecuteUnrealPython(const TSharedPtr<FJsonObject>& Params)
{
    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code))
    {
        if (!Params->TryGetStringField(TEXT("command"), Code))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'code' or 'command' parameter"));
        }
    }

    if (Code.TrimStartAndEnd().IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python code cannot be empty"));
    }

    FString ExecutionModeString = TEXT("ExecuteFile");
    Params->TryGetStringField(TEXT("execution_mode"), ExecutionModeString);

    EPythonCommandExecutionMode ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    if (!LexTryParseString(ExecutionMode, *ExecutionModeString))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Invalid execution_mode '%s'. Expected ExecuteFile, ExecuteStatement, or EvaluateStatement"),
            *ExecutionModeString));
    }

    FString FileExecutionScopeString = TEXT("Private");
    Params->TryGetStringField(TEXT("file_execution_scope"), FileExecutionScopeString);

    EPythonFileExecutionScope FileExecutionScope = EPythonFileExecutionScope::Private;
    if (!TryParsePythonFileExecutionScope(FileExecutionScopeString, FileExecutionScope))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Invalid file_execution_scope '%s'. Expected Private or Public"),
            *FileExecutionScopeString));
    }

    bool bUnattended = true;
    Params->TryGetBoolField(TEXT("unattended"), bUnattended);

    IPythonScriptPlugin* PythonScriptPlugin = FModuleManager::Get().LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
    if (!PythonScriptPlugin)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PythonScriptPlugin module is unavailable. Ensure the Python Script Plugin is enabled."));
    }

    const bool bWasPythonInitialized = PythonScriptPlugin->IsPythonInitialized();
    if (!bWasPythonInitialized)
    {
        PythonScriptPlugin->ForceEnablePythonAtRuntime();
    }

    if (!PythonScriptPlugin->IsPythonInitialized())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python Script Plugin is loaded but Python could not be initialized."));
    }

    FPythonCommandEx PythonCommand;
    PythonCommand.Command = Code;
    PythonCommand.ExecutionMode = ExecutionMode;
    PythonCommand.FileExecutionScope = FileExecutionScope;
    if (bUnattended)
    {
        PythonCommand.Flags |= EPythonCommandFlags::Unattended;
    }

    const bool bSuccess = PythonScriptPlugin->ExecPythonCommandEx(PythonCommand);

    TArray<TSharedPtr<FJsonValue>> LogOutputArray;
    for (const FPythonLogOutputEntry& LogEntry : PythonCommand.LogOutput)
    {
        TSharedPtr<FJsonObject> LogEntryObject = MakeShared<FJsonObject>();
        LogEntryObject->SetStringField(TEXT("type"), LexToString(LogEntry.Type));
        LogEntryObject->SetStringField(TEXT("output"), LogEntry.Output);
        LogOutputArray.Add(MakeShared<FJsonValueObject>(LogEntryObject));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetStringField(TEXT("execution_mode"), LexToString(ExecutionMode));
    ResultObj->SetStringField(TEXT("file_execution_scope"), PythonFileExecutionScopeToString(FileExecutionScope));
    ResultObj->SetBoolField(TEXT("unattended"), bUnattended);
    ResultObj->SetBoolField(TEXT("python_was_initialized"), bWasPythonInitialized);
    ResultObj->SetBoolField(TEXT("python_is_initialized"), PythonScriptPlugin->IsPythonInitialized());
    ResultObj->SetStringField(TEXT("command_result"), PythonCommand.CommandResult);
    ResultObj->SetArrayField(TEXT("log_output"), LogOutputArray);
    if (!bSuccess && !ResultObj->HasField(TEXT("error")))
    {
        ResultObj->SetStringField(TEXT("error"), PythonCommand.CommandResult.IsEmpty() ? TEXT("Python execution failed") : PythonCommand.CommandResult);
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Deprecated: Use FEpicUnrealMCPEnvironmentCommands::HandleSpawnActor instead (reflection-based)
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            // Check for an optional static_mesh parameter to assign a mesh
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh)
                {
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
                }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // This function will now correctly call the implementation in BlueprintCommands
    FEpicUnrealMCPBlueprintCommands BlueprintCommands;
    return BlueprintCommands.HandleCommand(TEXT("spawn_blueprint_actor"), Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleImportFBX(const TSharedPtr<FJsonObject>& Params)
{
    // Get FBX file path
    FString FBXPath;
    if (!Params->TryGetStringField(TEXT("fbx_path"), FBXPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'fbx_path' parameter"));
    }

    // Check if file exists
    IFileManager& FileManager = IFileManager::Get();
    if (!FileManager.FileExists(*FBXPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("FBX file not found: %s"), *FBXPath));
    }

    // Get optional destination path (default to /Game/ImportedMeshes/)
    FString DestinationPath = TEXT("/Game/ImportedMeshes/");
    if (Params->HasField(TEXT("destination_path")))
    {
        Params->TryGetStringField(TEXT("destination_path"), DestinationPath);
    }

    // Get optional asset name (default to filename without extension)
    FString AssetName;
    if (Params->HasField(TEXT("asset_name")))
    {
        Params->TryGetStringField(TEXT("asset_name"), AssetName);
    }
    else
    {
        AssetName = FPaths::GetBaseFilename(FBXPath);
    }

    // Get optional spawn in level flag
    bool bSpawnInLevel = true;
    Params->TryGetBoolField(TEXT("spawn_in_level"), bSpawnInLevel);

    // Get optional location for spawned actor
    FVector SpawnLocation(0.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("location")))
    {
        SpawnLocation = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }

    // Ensure destination path exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    // Get AssetTools module
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // Create FBX factory
    UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
    FbxFactory->AddToRoot();

    // Create import UI with proper settings
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>(FbxFactory);
    ImportUI->bImportMesh = true;
    ImportUI->bImportAnimations = false;
    ImportUI->bImportMaterials = false;
    ImportUI->bImportTextures = false;
    ImportUI->bImportAsSkeletal = false;
    ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
    ImportUI->bIsReimport = false;
    ImportUI->ReimportMesh = nullptr;
    ImportUI->bAllowContentTypeImport = true;
    ImportUI->bAutomatedImportShouldDetectType = false;
    ImportUI->bIsObjImport = false;
    
    // Create static mesh import data
    ImportUI->StaticMeshImportData = NewObject<UFbxStaticMeshImportData>(FbxFactory);
    ImportUI->StaticMeshImportData->bCombineMeshes = true;
    
    FbxFactory->ImportUI = ImportUI;
    FbxFactory->SetDetectImportTypeOnImport(false);

    // Create import task
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->AddToRoot();
    Task->bAutomated = true;
    Task->bReplaceExisting = true;
    Task->bSave = true;
    Task->DestinationPath = DestinationPath;
    Task->DestinationName = AssetName;
    Task->Filename = FBXPath;
    Task->Factory = FbxFactory;
    Task->Options = ImportUI;
    
    FbxFactory->SetAssetImportTask(Task);
    
    // Execute import
    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);
    AssetToolsModule.Get().ImportAssetTasks(Tasks);

    // Get imported objects
    UObject* ImportedAsset = nullptr;
    for (const FString& AssetPath : Task->ImportedObjectPaths)
    {
        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
        ImportedAsset = AssetData.GetAsset();
        if (ImportedAsset)
        {
            break;
        }
    }

    // Cleanup
    Task->RemoveFromRoot();
    FbxFactory->RemoveFromRoot();

    if (!ImportedAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to import FBX: %s"), *FBXPath));
    }

    // Get the static mesh
    UStaticMesh* ImportedMesh = Cast<UStaticMesh>(ImportedAsset);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    
    TArray<TSharedPtr<FJsonValue>> ImportedAssetPaths;
    ImportedAssetPaths.Add(MakeShared<FJsonValueString>(ImportedAsset->GetPathName()));
    ResultObj->SetArrayField(TEXT("imported_assets"), ImportedAssetPaths);

    // Optionally spawn actor in level
    if (bSpawnInLevel && ImportedMesh)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = *AssetName;
            
            AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                SpawnLocation,
                FRotator::ZeroRotator,
                SpawnParams
            );
            
            if (NewActor)
            {
                NewActor->GetStaticMeshComponent()->SetStaticMesh(ImportedMesh);
                NewActor->SetActorLabel(AssetName);
                ResultObj->SetStringField(TEXT("spawned_actor"), NewActor->GetName());
                ResultObj->SetObjectField(TEXT("actor_info"), FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true));
            }
        }
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("mesh_path"), ImportedMesh ? ImportedMesh->GetPathName() : TEXT(""));
    
    return ResultObj;
}

// ============================================================================
// Generic Asset Management (通用资产操作 - Refactored)
// ============================================================================

UClass* FEpicUnrealMCPEditorCommands::FindAssetClassByName(const FString& TypeName)
{
    if (TypeName.IsEmpty())
    {
        return nullptr;
    }

    // Common asset type mappings
    TMap<FString, FString> TypeMappings;
    TypeMappings.Add(TEXT("Material"), TEXT("/Script/Engine.Material"));
    TypeMappings.Add(TEXT("MaterialInstance"), TEXT("/Script/Engine.MaterialInstanceConstant"));
    TypeMappings.Add(TEXT("MaterialInstanceConstant"), TEXT("/Script/Engine.MaterialInstanceConstant"));
    TypeMappings.Add(TEXT("MaterialFunction"), TEXT("/Script/Engine.MaterialFunction"));
    TypeMappings.Add(TEXT("Texture"), TEXT("/Script/Engine.Texture2D"));
    TypeMappings.Add(TEXT("Texture2D"), TEXT("/Script/Engine.Texture2D"));
    TypeMappings.Add(TEXT("StaticMesh"), TEXT("/Script/Engine.StaticMesh"));

    // Try direct mapping first
    if (TypeMappings.Contains(TypeName))
    {
        UClass* FoundClass = FindObject<UClass>(nullptr, *TypeMappings[TypeName]);
        if (FoundClass)
        {
            return FoundClass;
        }
    }

    // Try with various prefixes
    TArray<FString> PossibleNames;
    PossibleNames.Add(TypeName);
    PossibleNames.Add(TEXT("U") + TypeName);
    PossibleNames.Add(TEXT("/Script/Engine.") + TypeName);
    PossibleNames.Add(TEXT("/Script/Engine.U") + TypeName);

    for (const FString& Name : PossibleNames)
    {
        UClass* FoundClass = FindObject<UClass>(nullptr, *Name);
        if (FoundClass)
        {
            return FoundClass;
        }
    }

    // Fallback: iterate all classes
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->GetName() == TypeName ||
            It->GetName() == TEXT("U") + TypeName)
        {
            return *It;
        }
    }

    return nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetType;
    if (!Params->TryGetStringField(TEXT("asset_type"), AssetType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_type' parameter"));
    }

    FString Name;
    if (!Params->TryGetStringField(TEXT("name"), Name))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString Path = TEXT("/Game/");
    Params->TryGetStringField(TEXT("path"), Path);

    // Find asset class
    UClass* AssetClass = FindAssetClassByName(AssetType);
    if (!AssetClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset type: %s"), *AssetType));
    }

    // Ensure path exists
    UEditorAssetLibrary::MakeDirectory(Path);

    // Create package
    FString PackageName = Path + Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    // Create the asset
    UObject* NewAsset = NewObject<UObject>(Package, AssetClass, *Name, RF_Public | RF_Standalone);
    if (!NewAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create asset"));
    }

    // Handle special initialization for known types
    if (AssetType == TEXT("MaterialInstance") || AssetType == TEXT("MaterialInstanceConstant"))
    {
        UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(NewAsset);
        if (MaterialInstance)
        {
            const TSharedPtr<FJsonObject>* PropertiesObj;
            if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
            {
                FString ParentPath;
                if (PropertiesObj->Get()->TryGetStringField(TEXT("parent_material"), ParentPath) ||
                    PropertiesObj->Get()->TryGetStringField(TEXT("parent"), ParentPath))
                {
                    UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
                    if (ParentMaterial)
                    {
                        MaterialInstance->SetParentEditorOnly(ParentMaterial);
                    }
                }
            }
            MaterialInstance->PostEditChange();
        }
    }
    else if (AssetType == TEXT("Material"))
    {
        UMaterial* Material = Cast<UMaterial>(NewAsset);
        if (Material)
        {
            Material->PostEditChange();
        }
    }
    else if (AssetType == TEXT("MaterialFunction"))
    {
        UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(NewAsset);
        if (MaterialFunction)
        {
            // MaterialFunction description is set via Description property directly
            MaterialFunction->PostEditChange();
        }
    }
    else if (AssetType == TEXT("NiagaraSystem"))
    {
        UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(NewAsset);
        if (NiagaraSystem)
        {
            // Initialize the Niagara system with default scripts and editor data
            UNiagaraSystemFactoryNew::InitializeSystem(NiagaraSystem, true);
            NiagaraSystem->PostEditChange();
        }
    }

    // Apply generic properties
    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
    {
        FString Error;
        for (const auto& Pair : PropertiesObj->Get()->Values)
        {
            SetUObjectProperty(NewAsset, Pair.Key, Pair.Value, Error);
        }
    }

    // Notify asset registry and save
    FAssetRegistryModule::AssetCreated(NewAsset);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
    ResultObj->SetStringField(TEXT("asset_type"), AssetClass->GetName());
    ResultObj->SetStringField(TEXT("name"), Name);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bDeleted);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);

    if (!bDeleted)
    {
        ResultObj->SetStringField(TEXT("error"), TEXT("Failed to delete asset"));
    }

    return ResultObj;
}

bool FEpicUnrealMCPEditorCommands::SetUObjectProperty(UObject* Object, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    if (!Object)
    {
        OutError = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        // Try case-insensitive search
        for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
        {
            if (PropIt->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                Property = *PropIt;
                break;
            }
        }
    }

    if (!Property)
    {
        OutError = FString::Printf(TEXT("Property '%s' not found"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

    // Bool
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool bValue = Value->AsBool();
        BoolProp->SetPropertyValue(PropertyAddr, bValue);
        return true;
    }
    // Int
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        IntProp->SetPropertyValue(PropertyAddr, IntValue);
        return true;
    }
    // Float
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        float FloatValue = static_cast<float>(Value->AsNumber());
        FloatProp->SetPropertyValue(PropertyAddr, FloatValue);
        return true;
    }
    // String
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString StrValue = Value->AsString();
        StrProp->SetPropertyValue(PropertyAddr, StrValue);
        return true;
    }
    // Name
    else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FName NameValue(*Value->AsString());
        NameProp->SetPropertyValue(PropertyAddr, NameValue);
        return true;
    }
    // Enum (Byte)
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->GetIntPropertyEnum())
        {
            int64 EnumValue = static_cast<int64>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
            return true;
        }
    }
    // Enum property
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        int64 EnumValue = static_cast<int64>(Value->AsNumber());
        EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(PropertyAddr, EnumValue);
        return true;
    }
    // Struct (Vector, Color, etc.)
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        // Handle LinearColor
        if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr;
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                float R = static_cast<float>((*Arr)[0]->AsNumber());
                float G = static_cast<float>((*Arr)[1]->AsNumber());
                float B = static_cast<float>((*Arr)[2]->AsNumber());
                float A = Arr->Num() > 3 ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.0f;
                FLinearColor* Color = (FLinearColor*)PropertyAddr;
                *Color = FLinearColor(R, G, B, A);
                return true;
            }
        }
    }

    OutError = FString::Printf(TEXT("Unsupported property type for '%s'"), *PropertyName);
    return false;
}

TSharedPtr<FJsonValue> FEpicUnrealMCPEditorCommands::GetUObjectPropertyAsJson(UObject* Object, const FString& PropertyName,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    return FEpicUnrealMCPCommonUtils::GetObjectPropertyAsJson(Object, PropertyName, MaxDepth, bIncludeAllProperties);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::GetAllUObjectPropertiesAsJson(UObject* Object,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    return FEpicUnrealMCPCommonUtils::GetAllObjectPropertiesAsJson(Object, MaxDepth, bIncludeAllProperties);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' parameter"));
    }

    // Load the asset
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    TArray<FString> ModifiedProperties;
    TArray<FString> FailedProperties;

    for (const auto& Pair : PropertiesObj->Get()->Values)
    {
        FString Error;
        if (SetUObjectProperty(Asset, Pair.Key, Pair.Value, Error))
        {
            ModifiedProperties.Add(Pair.Key);
        }
        else
        {
            FailedProperties.Add(Pair.Key + TEXT(": ") + Error);
        }
    }

    // Mark package dirty
    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetNumberField(TEXT("modified_count"), ModifiedProperties.Num());
    ResultObj->SetNumberField(TEXT("failed_count"), FailedProperties.Num());

    TArray<TSharedPtr<FJsonValue>> ModifiedArray;
    for (const FString& Prop : ModifiedProperties)
    {
        ModifiedArray.Add(MakeShared<FJsonValueString>(Prop));
    }
    ResultObj->SetArrayField(TEXT("modified_properties"), ModifiedArray);

    if (FailedProperties.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> FailedArray;
        for (const FString& Fail : FailedProperties)
        {
            FailedArray.Add(MakeShared<FJsonValueString>(Fail));
        }
        ResultObj->SetArrayField(TEXT("failed_properties"), FailedArray);
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    // Load the asset
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    // Optional: specific properties to get
    TArray<FString> RequestedProperties;
    const TArray<TSharedPtr<FJsonValue>>* PropsArray;
    if (Params->TryGetArrayField(TEXT("properties"), PropsArray))
    {
        for (const auto& Val : *PropsArray)
        {
            RequestedProperties.Add(Val->AsString());
        }
    }

    int32 MaxDepth = 1;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
    MaxDepth = FMath::Clamp(MaxDepth, 0, 8);

    bool bIncludeAllProperties = false;
    Params->TryGetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
    ResultObj->SetNumberField(TEXT("max_depth"), MaxDepth);
    ResultObj->SetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);

    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();

    if (RequestedProperties.Num() > 0)
    {
        // Get specific properties
        for (const FString& PropName : RequestedProperties)
        {
            TSharedPtr<FJsonValue> PropValue = GetUObjectPropertyAsJson(Asset, PropName, MaxDepth, bIncludeAllProperties);
            if (!PropValue->IsNull())
            {
                PropertiesObj->SetField(PropName, PropValue);
            }
        }
    }
    else
    {
        // Get all editable properties
        PropertiesObj = GetAllUObjectPropertiesAsJson(Asset, MaxDepth, bIncludeAllProperties);
    }

    ResultObj->SetObjectField(TEXT("properties"), PropertiesObj);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleBatchCreateAssets(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* ItemsArray;
    if (!Params->TryGetArrayField(TEXT("items"), ItemsArray))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'items' parameter"));
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (int32 i = 0; i < ItemsArray->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ItemObj;
        if (!(*ItemsArray)[i]->TryGetObject(ItemObj))
        {
            TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
            ErrorResult->SetBoolField(TEXT("success"), false);
            ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid item format"));
            Results.Add(MakeShared<FJsonValueObject>(ErrorResult));
            FailCount++;
            continue;
        }

        TSharedPtr<FJsonObject> Result = HandleCreateAsset(*ItemObj);
        Results.Add(MakeShared<FJsonValueObject>(Result));

        if (Result->GetBoolField(TEXT("success")))
        {
            SuccessCount++;
        }
        else
        {
            FailCount++;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("results"), Results);
    ResultObj->SetNumberField(TEXT("success_count"), SuccessCount);
    ResultObj->SetNumberField(TEXT("fail_count"), FailCount);
    ResultObj->SetNumberField(TEXT("total_count"), ItemsArray->Num());

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleBatchSetAssetsProperties(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* ItemsArray;
    if (!Params->TryGetArrayField(TEXT("items"), ItemsArray))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'items' parameter"));
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (int32 i = 0; i < ItemsArray->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* ItemObj;
        if (!(*ItemsArray)[i]->TryGetObject(ItemObj))
        {
            TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
            ErrorResult->SetBoolField(TEXT("success"), false);
            ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid item format"));
            Results.Add(MakeShared<FJsonValueObject>(ErrorResult));
            FailCount++;
            continue;
        }

        TSharedPtr<FJsonObject> Result = HandleSetAssetProperties(*ItemObj);
        Results.Add(MakeShared<FJsonValueObject>(Result));

        if (Result->GetBoolField(TEXT("success")))
        {
            SuccessCount++;
        }
        else
        {
            FailCount++;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("results"), Results);
    ResultObj->SetNumberField(TEXT("success_count"), SuccessCount);
    ResultObj->SetNumberField(TEXT("fail_count"), FailCount);
    ResultObj->SetNumberField(TEXT("total_count"), ItemsArray->Num());

    return ResultObj;
}
