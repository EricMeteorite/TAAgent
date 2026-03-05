#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Environment, Lighting and Viewport Commands
 */
class FEpicUnrealMCPEnvironmentCommands
{
public:
    // Viewport screenshot
    static TSharedPtr<FJsonObject> HandleGetViewportScreenshot(const TSharedPtr<FJsonObject>& Params);
    
    // Light management
    static TSharedPtr<FJsonObject> HandleCreateLight(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> HandleSetLightProperties(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> HandleGetLights(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> HandleDeleteLight(const TSharedPtr<FJsonObject>& Params);
};
