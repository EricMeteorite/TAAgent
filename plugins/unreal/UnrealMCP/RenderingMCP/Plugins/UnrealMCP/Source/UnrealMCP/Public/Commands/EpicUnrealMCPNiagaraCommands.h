// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Niagara-related MCP commands
 * Handles spawning, controlling, and modifying Niagara particle systems
 */
class UNREALMCP_API FEpicUnrealMCPNiagaraCommands
{
public:
    FEpicUnrealMCPNiagaraCommands();

    // Handle Niagara commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Spawn a Niagara system at a location
    TSharedPtr<FJsonObject> HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
    
    // Spawn a Niagara system attached to an actor
    TSharedPtr<FJsonObject> HandleSpawnNiagaraSystemAttached(const TSharedPtr<FJsonObject>& Params);
    
    // Get all Niagara actors/components in the level
    TSharedPtr<FJsonObject> HandleGetNiagaraSystems(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara float parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraFloatParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara vector parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraVectorParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara color parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraColorParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara bool parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraBoolParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara int parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraIntParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Set Niagara texture parameter
    TSharedPtr<FJsonObject> HandleSetNiagaraTextureParameter(const TSharedPtr<FJsonObject>& Params);
    
    // Get Niagara component parameters
    TSharedPtr<FJsonObject> HandleGetNiagaraParameters(const TSharedPtr<FJsonObject>& Params);
    
    // Activate Niagara system
    TSharedPtr<FJsonObject> HandleActivateNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
    
    // Deactivate Niagara system
    TSharedPtr<FJsonObject> HandleDeactivateNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
    
    // Destroy Niagara component
    TSharedPtr<FJsonObject> HandleDestroyNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
    
    // Get available Niagara system assets
    TSharedPtr<FJsonObject> HandleGetNiagaraAssets(const TSharedPtr<FJsonObject>& Params);
    
    // Helper to find Niagara component by name or actor name
    class UNiagaraComponent* FindNiagaraComponent(const FString& ComponentName, const FString& ActorName);
    
    // Helper to load Niagara system asset
    class UNiagaraSystem* LoadNiagaraSystemAsset(const FString& AssetPath);
};
