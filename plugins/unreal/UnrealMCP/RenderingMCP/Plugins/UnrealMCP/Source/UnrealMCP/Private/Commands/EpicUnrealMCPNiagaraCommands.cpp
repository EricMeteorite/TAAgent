// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"

FEpicUnrealMCPNiagaraCommands::FEpicUnrealMCPNiagaraCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("spawn_niagara_system"))
    {
        return HandleSpawnNiagaraSystem(Params);
    }
    else if (CommandType == TEXT("spawn_niagara_system_attached"))
    {
        return HandleSpawnNiagaraSystemAttached(Params);
    }
    else if (CommandType == TEXT("get_niagara_systems"))
    {
        return HandleGetNiagaraSystems(Params);
    }
    else if (CommandType == TEXT("set_niagara_float_parameter"))
    {
        return HandleSetNiagaraFloatParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_vector_parameter"))
    {
        return HandleSetNiagaraVectorParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_color_parameter"))
    {
        return HandleSetNiagaraColorParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_bool_parameter"))
    {
        return HandleSetNiagaraBoolParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_int_parameter"))
    {
        return HandleSetNiagaraIntParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_texture_parameter"))
    {
        return HandleSetNiagaraTextureParameter(Params);
    }
    else if (CommandType == TEXT("get_niagara_parameters"))
    {
        return HandleGetNiagaraParameters(Params);
    }
    else if (CommandType == TEXT("activate_niagara_system"))
    {
        return HandleActivateNiagaraSystem(Params);
    }
    else if (CommandType == TEXT("deactivate_niagara_system"))
    {
        return HandleDeactivateNiagaraSystem(Params);
    }
    else if (CommandType == TEXT("destroy_niagara_system"))
    {
        return HandleDestroyNiagaraSystem(Params);
    }
    else if (CommandType == TEXT("get_niagara_assets"))
    {
        return HandleGetNiagaraAssets(Params);
    }
    else
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Unknown Niagara command: %s"), *CommandType));
        return Result;
    }
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Get parameters
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: asset_path");
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* LocationJsonValueArray;
    if (!Params->TryGetArrayField(TEXT("location"), LocationJsonValueArray) || LocationJsonValueArray->Num() != 3)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing or invalid parameter: location (expected [x, y, z])");
        return Result;
    }
    FVector Location((*LocationJsonValueArray)[0]->AsNumber(), (*LocationJsonValueArray)[1]->AsNumber(), (*LocationJsonValueArray)[2]->AsNumber());
    
    // Optional rotation (default: zero)
    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>>* RotationJsonValueArray;
    if (Params->TryGetArrayField(TEXT("rotation"), RotationJsonValueArray) && RotationJsonValueArray->Num() == 3)
    {
        Rotation = FRotator((*RotationJsonValueArray)[0]->AsNumber(), (*RotationJsonValueArray)[1]->AsNumber(), (*RotationJsonValueArray)[2]->AsNumber());
    }
    
    // Optional scale (default: 1,1,1)
    FVector Scale(1.0f);
    const TArray<TSharedPtr<FJsonValue>>* ScaleJsonValueArray;
    if (Params->TryGetArrayField(TEXT("scale"), ScaleJsonValueArray) && ScaleJsonValueArray->Num() == 3)
    {
        Scale = FVector((*ScaleJsonValueArray)[0]->AsNumber(), (*ScaleJsonValueArray)[1]->AsNumber(), (*ScaleJsonValueArray)[2]->AsNumber());
    }
    
    // Optional auto_destroy (default: true)
    bool bAutoDestroy = true;
    Params->TryGetBoolField(TEXT("auto_destroy"), bAutoDestroy);
    
    // Optional auto_activate (default: true)
    bool bAutoActivate = true;
    Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);
    
    // Optional name for the actor
    FString ActorName;
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    
    // Load Niagara system asset
    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
        return Result;
    }
    
    // Get world
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "No world available");
        return Result;
    }
    
    // Spawn Niagara system at location
    UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        NiagaraSystem,
        Location,
        Rotation,
        Scale,
        bAutoDestroy,
        bAutoActivate
    );
    
    if (!NiagaraComponent)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Failed to spawn Niagara system");
        return Result;
    }
    
    // Set actor name if specified
    if (!ActorName.IsEmpty() && NiagaraComponent->GetOwner())
    {
        NiagaraComponent->GetOwner()->Rename(*ActorName);
    }
    
    Result->SetBoolField("success", true);
    Result->SetStringField("component_name", NiagaraComponent->GetName());
    Result->SetStringField("actor_name", NiagaraComponent->GetOwner() ? NiagaraComponent->GetOwner()->GetName() : TEXT(""));
    Result->SetStringField("system_name", NiagaraSystem->GetName());
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSpawnNiagaraSystemAttached(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Get parameters
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: asset_path");
        return Result;
    }
    
    FString AttachToActorName;
    if (!Params->TryGetStringField(TEXT("attach_to_actor"), AttachToActorName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: attach_to_actor");
        return Result;
    }
    
    // Find the actor to attach to
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "No world available");
        return Result;
    }
    
    AActor* AttachActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if ((*It)->GetName() == AttachToActorName || (*It)->GetActorLabel() == AttachToActorName)
        {
            AttachActor = *It;
            break;
        }
    }
    
    if (!AttachActor)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Actor not found: %s"), *AttachToActorName));
        return Result;
    }
    
    // Get attachment point (optional)
    FName AttachPointName = NAME_None;
    FString AttachPointString;
    if (Params->TryGetStringField(TEXT("attach_point"), AttachPointString))
    {
        AttachPointName = *AttachPointString;
    }
    
    // Get location offset (optional)
    FVector LocationOffset = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>>* LocationJsonValueArray;
    if (Params->TryGetArrayField(TEXT("location_offset"), LocationJsonValueArray) && LocationJsonValueArray->Num() == 3)
    {
        LocationOffset = FVector((*LocationJsonValueArray)[0]->AsNumber(), (*LocationJsonValueArray)[1]->AsNumber(), (*LocationJsonValueArray)[2]->AsNumber());
    }
    
    // Get rotation (optional)
    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>>* RotationJsonValueArray;
    if (Params->TryGetArrayField(TEXT("rotation"), RotationJsonValueArray) && RotationJsonValueArray->Num() == 3)
    {
        Rotation = FRotator((*RotationJsonValueArray)[0]->AsNumber(), (*RotationJsonValueArray)[1]->AsNumber(), (*RotationJsonValueArray)[2]->AsNumber());
    }
    
    // Optional auto_destroy (default: true)
    bool bAutoDestroy = true;
    Params->TryGetBoolField(TEXT("auto_destroy"), bAutoDestroy);
    
    // Optional auto_activate (default: true)
    bool bAutoActivate = true;
    Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);
    
    // Load Niagara system asset
    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
        return Result;
    }
    
    // Find root component or use first available scene component
    USceneComponent* AttachComponent = AttachActor->GetRootComponent();
    if (!AttachComponent)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Target actor has no root component");
        return Result;
    }
    
    // Spawn attached Niagara system
    UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        NiagaraSystem,
        AttachComponent,
        AttachPointName,
        LocationOffset,
        Rotation,
        EAttachLocation::KeepRelativeOffset,
        bAutoDestroy,
        bAutoActivate
    );
    
    if (!NiagaraComponent)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Failed to spawn attached Niagara system");
        return Result;
    }
    
    Result->SetBoolField("success", true);
    Result->SetStringField("component_name", NiagaraComponent->GetName());
    Result->SetStringField("attached_to", AttachToActorName);
    Result->SetStringField("system_name", NiagaraSystem->GetName());
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraSystems(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "No world available");
        return Result;
    }
    
    TArray<TSharedPtr<FJsonValue>> SystemsArray;
    
    // Find all Niagara actors
    for (TActorIterator<ANiagaraActor> It(World); It; ++It)
    {
        ANiagaraActor* NiagaraActor = *It;
        if (NiagaraActor)
        {
            TSharedPtr<FJsonObject> SystemInfo = MakeShareable(new FJsonObject);
            SystemInfo->SetStringField("actor_name", NiagaraActor->GetName());
            SystemInfo->SetStringField("actor_label", NiagaraActor->GetActorLabel());
            
            UNiagaraComponent* NiagaraComp = NiagaraActor->GetNiagaraComponent();
            if (NiagaraComp)
            {
                SystemInfo->SetStringField("component_name", NiagaraComp->GetName());
                
                if (NiagaraComp->GetAsset())
                {
                    SystemInfo->SetStringField("system_asset", NiagaraComp->GetAsset()->GetPathName());
                    SystemInfo->SetStringField("system_name", NiagaraComp->GetAsset()->GetName());
                }
                
                SystemInfo->SetBoolField("is_active", NiagaraComp->IsActive());
                
                // Get transform
                TArray<TSharedPtr<FJsonValue>> LocationArray;
                FVector Location = NiagaraActor->GetActorLocation();
                LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.X)));
                LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Y)));
                LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Z)));
                SystemInfo->SetArrayField("location", LocationArray);
            }
            
            SystemsArray.Add(MakeShareable(new FJsonValueObject(SystemInfo)));
        }
    }
    
    // Also find any Niagara components on other actors
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Cast<ANiagaraActor>(Actor))
        {
            TArray<UNiagaraComponent*> NiagaraComponents;
            Actor->GetComponents<UNiagaraComponent>(NiagaraComponents);
            
            for (UNiagaraComponent* NiagaraComp : NiagaraComponents)
            {
                TSharedPtr<FJsonObject> SystemInfo = MakeShareable(new FJsonObject);
                SystemInfo->SetStringField("actor_name", Actor->GetName());
                SystemInfo->SetStringField("actor_label", Actor->GetActorLabel());
                SystemInfo->SetStringField("component_name", NiagaraComp->GetName());
                
                if (NiagaraComp->GetAsset())
                {
                    SystemInfo->SetStringField("system_asset", NiagaraComp->GetAsset()->GetPathName());
                    SystemInfo->SetStringField("system_name", NiagaraComp->GetAsset()->GetName());
                }
                
                SystemInfo->SetBoolField("is_active", NiagaraComp->IsActive());
                SystemsArray.Add(MakeShareable(new FJsonValueObject(SystemInfo)));
            }
        }
    }
    
    Result->SetBoolField("success", true);
    Result->SetArrayField("systems", SystemsArray);
    Result->SetNumberField("count", SystemsArray.Num());
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraFloatParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName;
    float Value;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: value");
        return Result;
    }
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->SetFloatParameter(*ParameterName, Value);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    Result->SetNumberField("value", Value);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* ValueJsonValueArray;
    if (!Params->TryGetArrayField(TEXT("value"), ValueJsonValueArray) || ValueJsonValueArray->Num() < 3)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing or invalid parameter: value (expected [x, y, z])");
        return Result;
    }
    
    FVector Value((*ValueJsonValueArray)[0]->AsNumber(), (*ValueJsonValueArray)[1]->AsNumber(), (*ValueJsonValueArray)[2]->AsNumber());
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->SetVectorParameter(*ParameterName, Value);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraColorParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* ValueJsonValueArray;
    if (!Params->TryGetArrayField(TEXT("value"), ValueJsonValueArray) || ValueJsonValueArray->Num() < 4)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing or invalid parameter: value (expected [r, g, b, a])");
        return Result;
    }
    
    FLinearColor Value((*ValueJsonValueArray)[0]->AsNumber(), (*ValueJsonValueArray)[1]->AsNumber(), (*ValueJsonValueArray)[2]->AsNumber(), (*ValueJsonValueArray)[3]->AsNumber());
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->SetColorParameter(*ParameterName, Value);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraBoolParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName;
    bool Value;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    if (!Params->TryGetBoolField(TEXT("value"), Value))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: value");
        return Result;
    }
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->SetBoolParameter(*ParameterName, Value);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    Result->SetBoolField("value", Value);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraIntParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName;
    int32 Value;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: value");
        return Result;
    }
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->SetIntParameter(*ParameterName, Value);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    Result->SetNumberField("value", Value);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName, ParameterName, TexturePath;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: parameter_name");
        return Result;
    }
    
    if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Missing required parameter: texture_path");
        return Result;
    }
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    // Load texture
    UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Failed to load texture: %s"), *TexturePath));
        return Result;
    }
    
    UNiagaraFunctionLibrary::SetTextureObject(NiagaraComp, ParameterName, Texture);
    
    Result->SetBoolField("success", true);
    Result->SetStringField("parameter_name", ParameterName);
    Result->SetStringField("texture_path", TexturePath);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraParameters(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    // Get system info
    if (NiagaraComp->GetAsset())
    {
        Result->SetStringField("system_asset", NiagaraComp->GetAsset()->GetPathName());
    }
    
    Result->SetBoolField("is_active", NiagaraComp->IsActive());
    
    // Note: Getting parameter values requires accessing the override parameters store
    // This is a simplified version - full implementation would iterate over all parameters
    
    Result->SetBoolField("success", true);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleActivateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    bool bReset = false;
    Params->TryGetBoolField(TEXT("reset"), bReset);
    
    NiagaraComp->Activate(bReset);
    
    Result->SetBoolField("success", true);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDeactivateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    NiagaraComp->Deactivate();
    
    Result->SetBoolField("success", true);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDestroyNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString ActorName, ComponentName;
    
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    
    UNiagaraComponent* NiagaraComp = FindNiagaraComponent(ComponentName, ActorName);
    if (!NiagaraComp)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Niagara component not found");
        return Result;
    }
    
    AActor* Owner = NiagaraComp->GetOwner();
    if (Owner && Cast<ANiagaraActor>(Owner))
    {
        // If it's a Niagara actor, destroy the whole actor
        Owner->Destroy();
    }
    else
    {
        // Otherwise just destroy the component
        NiagaraComp->DestroyComponent();
    }
    
    Result->SetBoolField("success", true);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraAssets(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString SearchPath = "/Game";
    Params->TryGetStringField(TEXT("search_path"), SearchPath);
    
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    
    // Use asset registry to find all Niagara systems
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.PackagePaths.Add(*SearchPath);
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Niagara.NiagaraSystem")));
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);
    
    for (const FAssetData& Asset : AssetList)
    {
        TSharedPtr<FJsonObject> AssetInfo = MakeShareable(new FJsonObject);
        AssetInfo->SetStringField("name", Asset.AssetName.ToString());
        AssetInfo->SetStringField("path", Asset.PackageName.ToString());
        AssetInfo->SetStringField("full_path", Asset.GetFullName());
        AssetsArray.Add(MakeShareable(new FJsonValueObject(AssetInfo)));
    }
    
    Result->SetBoolField("success", true);
    Result->SetArrayField("assets", AssetsArray);
    Result->SetNumberField("count", AssetsArray.Num());
    
    return Result;
}

UNiagaraComponent* FEpicUnrealMCPNiagaraCommands::FindNiagaraComponent(const FString& ComponentName, const FString& ActorName)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return nullptr;
    }
    
    // If component name is provided, search by component name
    if (!ComponentName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TArray<UNiagaraComponent*> NiagaraComponents;
            (*It)->GetComponents<UNiagaraComponent>(NiagaraComponents);
            
            for (UNiagaraComponent* Comp : NiagaraComponents)
            {
                if (Comp->GetName() == ComponentName)
                {
                    return Comp;
                }
            }
        }
    }
    
    // If actor name is provided, search by actor name
    if (!ActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetName() == ActorName || (*It)->GetActorLabel() == ActorName)
            {
                UNiagaraComponent* Comp = (*It)->FindComponentByClass<UNiagaraComponent>();
                if (Comp)
                {
                    return Comp;
                }
            }
        }
    }
    
    return nullptr;
}

UNiagaraSystem* FEpicUnrealMCPNiagaraCommands::LoadNiagaraSystemAsset(const FString& AssetPath)
{
    return LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
}
