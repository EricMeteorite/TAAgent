#include "Commands/EpicUnrealMCPEnvironmentCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Slate/SceneViewport.h"
#include "LevelEditor.h"
#include "ILevelEditor.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"

// Light includes
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"

// Viewport screenshot
TSharedPtr<FJsonObject> FEpicUnrealMCPEnvironmentCommands::HandleGetViewportScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get optional parameters
    FString Format = TEXT("png");
    Params->TryGetStringField(TEXT("format"), Format);
    
    int32 Quality = 85;
    if (Params->HasField(TEXT("quality")))
    {
        Quality = Params->GetIntegerField(TEXT("quality"));
        Quality = FMath::Clamp(Quality, 1, 100);
    }
    
    bool bIncludeUI = false;
    if (Params->HasField(TEXT("include_ui")))
    {
        bIncludeUI = Params->GetBoolField(TEXT("include_ui"));
    }

    // Get output path (required for saving to file)
    FString OutputPath;
    if (!Params->TryGetStringField(TEXT("output_path"), OutputPath) || OutputPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("output_path parameter is required"));
    }

    // Get the active viewport
    FSceneViewport* TargetViewport = nullptr;
    FString ViewportType;
    
    // Try to get editor viewport first
    if (GEditor)
    {
        // Check for PIE viewport
        if (GEditor->PlayWorld)
        {
            UWorld* PlayWorld = GEditor->PlayWorld;
            if (UGameViewportClient* GVC = PlayWorld->GetGameViewport())
            {
                TargetViewport = GVC->GetGameViewport();
                ViewportType = TEXT("PIE");
            }
        }
        
        // Fall back to level editor viewport
        if (!TargetViewport)
        {
            FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
            if (LevelEditorModule)
            {
                TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetFirstLevelEditor();
                if (LevelEditor.IsValid())
                {
                    TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
                    if (ActiveViewport.IsValid())
                    {
                        TSharedPtr<FSceneViewport> SceneViewport = ActiveViewport->GetSharedActiveViewport();
                        if (SceneViewport.IsValid())
                        {
                            TargetViewport = SceneViewport.Get();
                            ViewportType = TEXT("Editor");
                        }
                    }
                }
            }
        }
    }
    
    if (!TargetViewport)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active viewport found"));
    }
    
    FIntPoint ViewportSize = TargetViewport->GetSizeXY();
    if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid viewport size"));
    }
    
    // Read pixels from viewport
    TArray<FColor> PixelData;
    FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
    ReadFlags.SetLinearToGamma(true);
    
    bool bReadSuccess = TargetViewport->ReadPixels(PixelData, ReadFlags);
    
    if (!bReadSuccess || PixelData.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read viewport pixels"));
    }
    
    // Encode image
    TArray<uint8> ImageData;
    EImageFormat ImageFormat = EImageFormat::PNG;
    if (Format == TEXT("jpg") || Format == TEXT("jpeg"))
    {
        ImageFormat = EImageFormat::JPEG;
    }
    else if (Format == TEXT("bmp"))
    {
        ImageFormat = EImageFormat::BMP;
    }
    
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    
    if (!ImageWrapper.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create image wrapper"));
    }
    
    ImageWrapper->SetRaw(PixelData.GetData(), PixelData.Num() * sizeof(FColor), ViewportSize.X, ViewportSize.Y, ERGBFormat::BGRA, 8);
    ImageData = ImageWrapper->GetCompressed(Quality);
    
    if (ImageData.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to compress image"));
    }
    
    // Save image to file
    bool bSaved = FFileHelper::SaveArrayToFile(ImageData, *OutputPath);
    
    if (!bSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save image to: %s"), *OutputPath));
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("file_path"), OutputPath);
    ResultObj->SetStringField(TEXT("format"), Format);
    ResultObj->SetNumberField(TEXT("width"), ViewportSize.X);
    ResultObj->SetNumberField(TEXT("height"), ViewportSize.Y);
    ResultObj->SetNumberField(TEXT("size_bytes"), ImageData.Num());
    ResultObj->SetStringField(TEXT("viewport_type"), ViewportType);
    
    return ResultObj;
}

// Light management - TODO: Migrate from main file
TSharedPtr<FJsonObject> FEpicUnrealMCPEnvironmentCommands::HandleCreateLight(const TSharedPtr<FJsonObject>& Params)
{
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("create_light not yet migrated to EnvironmentCommands"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnvironmentCommands::HandleSetLightProperties(const TSharedPtr<FJsonObject>& Params)
{
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("set_light_properties not yet migrated to EnvironmentCommands"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnvironmentCommands::HandleGetLights(const TSharedPtr<FJsonObject>& Params)
{
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("get_lights not yet migrated to EnvironmentCommands"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEnvironmentCommands::HandleDeleteLight(const TSharedPtr<FJsonObject>& Params)
{
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("delete_light not yet migrated to EnvironmentCommands"));
}
