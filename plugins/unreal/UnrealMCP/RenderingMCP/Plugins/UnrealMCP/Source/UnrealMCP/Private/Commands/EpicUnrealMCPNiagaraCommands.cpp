// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraSystem.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraBakerOutputTexture2D.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraWorldManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "AdvancedPreviewScene.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageWrapperHelper.h"
#include "RenderCore.h"

// UE 5.7+ Stateless Niagara support. Guard on header presence as well because
// some source branches expose unexpected version macros while lacking these headers.
#if (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)) && \
    __has_include("Stateless/NiagaraStatelessEmitter.h") && \
    __has_include("Stateless/NiagaraStatelessModule.h") && \
    __has_include("Stateless/NiagaraStatelessEmitterTemplate.h")
#define TAAGENT_WITH_NIAGARA_STATELESS 1
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterTemplate.h"
#else
#define TAAGENT_WITH_NIAGARA_STATELESS 0
#endif

// Standard Niagara Graph support
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOp.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraDataInterfaceParticleRead.h"
#include "NiagaraScriptSource.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/UObjectHash.h"

// Niagara Stack utilities for module operations
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

// Runtime particle data access
#include "NiagaraComponent.h"
#include "NiagaraSimCache.h"
#include "EngineUtils.h"

FEpicUnrealMCPNiagaraCommands::FEpicUnrealMCPNiagaraCommands()
{
}

namespace
{
struct FScopedBakerSettingsRestore
{
    explicit FScopedBakerSettingsRestore(UNiagaraBakerSettings* InSettings)
        : Settings(InSettings)
    {
        if (Settings)
        {
            StartSeconds = Settings->StartSeconds;
            DurationSeconds = Settings->DurationSeconds;
            FramesPerSecond = Settings->FramesPerSecond;
            FramesPerDimension = Settings->FramesPerDimension;
            bLockToSimulationFrameRate = Settings->bLockToSimulationFrameRate;
            bRenderComponentOnly = Settings->bRenderComponentOnly;
        }
    }

    ~FScopedBakerSettingsRestore()
    {
        if (Settings)
        {
            Settings->StartSeconds = StartSeconds;
            Settings->DurationSeconds = DurationSeconds;
            Settings->FramesPerSecond = FramesPerSecond;
            Settings->FramesPerDimension = FramesPerDimension;
            Settings->bLockToSimulationFrameRate = bLockToSimulationFrameRate;
            Settings->bRenderComponentOnly = bRenderComponentOnly;
        }
    }

    UNiagaraBakerSettings* Settings = nullptr;
    float StartSeconds = 0.0f;
    float DurationSeconds = 0.0f;
    int32 FramesPerSecond = 60;
    FIntPoint FramesPerDimension = FIntPoint(8, 8);
    bool bLockToSimulationFrameRate = false;
    bool bRenderComponentOnly = true;
};

class FLocalNiagaraBakeRenderer
{
public:
    explicit FLocalNiagaraBakeRenderer(UNiagaraSystem* InNiagaraSystem)
        : NiagaraSystem(InNiagaraSystem)
    {
        check(NiagaraSystem);

        PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
        PreviewComponent->CastShadow = true;
        PreviewComponent->bCastDynamicShadow = true;
        PreviewComponent->SetAllowScalability(false);
        PreviewComponent->SetAsset(NiagaraSystem);
        PreviewComponent->SetForceSolo(true);
        PreviewComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
        PreviewComponent->SetCanRenderWhileSeeking(true);
        PreviewComponent->SetMaxSimTime(0.0f);
        PreviewComponent->Activate(true);

        PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
        PreviewScene->SetFloorVisibility(false);
        PreviewScene->AddComponent(PreviewComponent, PreviewComponent->GetRelativeTransform());
    }

    ~FLocalNiagaraBakeRenderer()
    {
        if (PreviewScene.IsValid() && PreviewComponent)
        {
            PreviewScene->RemoveComponent(PreviewComponent);
            PreviewScene.Reset();
        }

        if (PreviewComponent)
        {
            PreviewComponent->DestroyComponent();
            PreviewComponent = nullptr;
        }

        if (SceneCaptureComponent)
        {
            SceneCaptureComponent->DestroyComponent();
            SceneCaptureComponent = nullptr;
        }
    }

    void SetAbsoluteTime(UNiagaraBakerSettings* BakerSettings, float AbsoluteTime, bool bShouldTickComponent = true)
    {
        if (!BakerSettings || !PreviewComponent)
        {
            return;
        }

        if (!PreviewComponent->IsActive())
        {
            PreviewComponent->Activate(true);
        }

        if (AbsoluteTime < PreviewComponent->GetDesiredAge())
        {
            PreviewComponent->ReinitializeSystem();
        }

        UWorld* World = PreviewComponent->GetWorld();
        if (World)
        {
            World->TimeSeconds = AbsoluteTime;
            World->UnpausedTimeSeconds = AbsoluteTime;
            World->RealTimeSeconds = AbsoluteTime;
            World->DeltaRealTimeSeconds = BakerSettings->GetSeekDelta();
            World->DeltaTimeSeconds = BakerSettings->GetSeekDelta();
        }

        if (bShouldTickComponent)
        {
            PreviewComponent->ReinitializeSystem();
            if (AbsoluteTime > SMALL_NUMBER)
            {
                PreviewComponent->AdvanceSimulationByTime(AbsoluteTime, BakerSettings->GetSeekDelta());
            }

            if (World)
            {
                World->SendAllEndOfFrameUpdates();
                if (FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World))
                {
                    WorldManager->FlushComputeAndDeferredQueues(false);
                }
            }
        }
    }

    void RenderSceneCapture(UNiagaraBakerSettings* BakerSettings, UTextureRenderTarget2D* RenderTarget, ESceneCaptureSource CaptureSource)
    {
        if (!BakerSettings || !PreviewComponent || !RenderTarget)
        {
            return;
        }

        UWorld* World = PreviewComponent->GetWorld();
        if (!World)
        {
            return;
        }

        if (SceneCaptureComponent == nullptr)
        {
            SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
            SceneCaptureComponent->bTickInEditor = false;
            SceneCaptureComponent->SetComponentTickEnabled(false);
            SceneCaptureComponent->SetVisibility(true);
            SceneCaptureComponent->bCaptureEveryFrame = false;
            SceneCaptureComponent->bCaptureOnMovement = false;
        }

        SceneCaptureComponent->RegisterComponentWithWorld(World);
        SceneCaptureComponent->TextureTarget = RenderTarget;
        SceneCaptureComponent->CaptureSource = CaptureSource;

        const FNiagaraBakerCameraSettings& CurrentCamera = BakerSettings->GetCurrentCamera();
        if (CurrentCamera.IsOrthographic())
        {
            SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
            SceneCaptureComponent->OrthoWidth = CurrentCamera.OrthoWidth;
        }
        else
        {
            SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
            SceneCaptureComponent->FOVAngle = CurrentCamera.FOV;
        }

        const FMatrix SceneCaptureMatrix = FMatrix(
            FPlane(0, 0, 1, 0),
            FPlane(1, 0, 0, 0),
            FPlane(0, 1, 0, 0),
            FPlane(0, 0, 0, 1)
        );
        const FMatrix ViewMatrix =
            SceneCaptureMatrix *
            BakerSettings->GetViewportMatrix().Inverse() *
            FRotationTranslationMatrix(BakerSettings->GetCameraRotation(), BakerSettings->GetCameraLocation());
        SceneCaptureComponent->SetWorldLocationAndRotation(ViewMatrix.GetOrigin(), ViewMatrix.Rotator());
        SceneCaptureComponent->bUseCustomProjectionMatrix = true;
        SceneCaptureComponent->CustomProjectionMatrix = BakerSettings->GetProjectionMatrix();

        if (BakerSettings->bRenderComponentOnly)
        {
            const TArray<TObjectPtr<USceneComponent>>& AttachChildren = PreviewComponent->GetAttachChildren();
            SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            SceneCaptureComponent->ShowOnlyComponents.Empty(1 + AttachChildren.Num());
            SceneCaptureComponent->ShowOnlyComponents.Add(PreviewComponent);
            for (TWeakObjectPtr<USceneComponent> WeakChildComponent : AttachChildren)
            {
                if (UPrimitiveComponent* ChildComponent = Cast<UPrimitiveComponent>(WeakChildComponent.Get()))
                {
                    SceneCaptureComponent->ShowOnlyComponents.Add(ChildComponent);
                }
            }
        }
        else
        {
            SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
        }

        SceneCaptureComponent->CaptureScene();
        SceneCaptureComponent->TextureTarget = nullptr;
        SceneCaptureComponent->UnregisterComponent();
    }

private:
    TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;
    TObjectPtr<UNiagaraComponent> PreviewComponent = nullptr;
    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
    TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent = nullptr;
};

static FString NormalizeBakeFileExtension(const FString& FileExtension)
{
    if (FileExtension.IsEmpty())
    {
        return TEXT(".png");
    }
    return FileExtension.StartsWith(TEXT(".")) ? FileExtension : TEXT(".") + FileExtension;
}

static void CleanupExistingBakeSequence(const FString& OutputDir, const FString& OutputPrefix)
{
    TArray<FString> ExistingFiles;
    const FString SearchPattern = FPaths::Combine(OutputDir, OutputPrefix + TEXT("_*.*"));
    IFileManager::Get().FindFiles(ExistingFiles, *SearchPattern, true, false);

    const FString ExpectedPrefix = OutputPrefix + TEXT("_");
    for (const FString& ExistingFile : ExistingFiles)
    {
        const FString BaseFileName = FPaths::GetBaseFilename(ExistingFile);
        if (!BaseFileName.StartsWith(ExpectedPrefix))
        {
            continue;
        }

        const FString FrameSuffix = BaseFileName.RightChop(ExpectedPrefix.Len());
        if (FrameSuffix.IsEmpty())
        {
            continue;
        }

        bool bAllDigits = true;
        for (TCHAR Character : FrameSuffix)
        {
            if (!FChar::IsDigit(Character))
            {
                bAllDigits = false;
                break;
            }
        }

        if (!bAllDigits)
        {
            continue;
        }

        IFileManager::Get().Delete(*FPaths::Combine(OutputDir, ExistingFile), false, true, true);
    }
}

static FIntPoint ResolveBakeFrameSize(UNiagaraBakerSettings* BakerSettings, int32 RequestedWidth, int32 RequestedHeight)
{
    int32 Width = RequestedWidth;
    int32 Height = RequestedHeight;

    if ((Width <= 0 || Height <= 0) && BakerSettings)
    {
        for (UNiagaraBakerOutput* Output : BakerSettings->Outputs)
        {
            if (const UNiagaraBakerOutputTexture2D* TextureOutput = Cast<UNiagaraBakerOutputTexture2D>(Output))
            {
                if (Width <= 0)
                {
                    Width = TextureOutput->FrameSize.X;
                }
                if (Height <= 0)
                {
                    Height = TextureOutput->FrameSize.Y;
                }
                break;
            }
        }
    }

    if (Width <= 0)
    {
        Width = 512;
    }
    if (Height <= 0)
    {
        Height = 512;
    }

    return FIntPoint(Width, Height);
}

static bool TryResolveBakeOutputDirectory(const FString& AssetPath, const FString& RequestedOutputDir, FString& OutOutputDir, FString& OutError)
{
    if (!RequestedOutputDir.IsEmpty())
    {
        OutOutputDir = FPaths::ConvertRelativePathToFull(RequestedOutputDir);
        return true;
    }

    const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    FString PackageFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, PackageFilename, TEXT(".uasset")))
    {
        OutError = FString::Printf(TEXT("Failed to resolve disk path for Niagara asset: %s"), *AssetPath);
        return false;
    }

    OutOutputDir = FPaths::GetPath(PackageFilename);
    return true;
}

static UTextureRenderTarget2D* CreateBakeRenderTarget(const FIntPoint& FrameSize)
{
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
    RenderTarget->ClearColor = FLinearColor::Transparent;
    RenderTarget->InitCustomFormat(FrameSize.X, FrameSize.Y, PF_FloatRGBA, false);
    RenderTarget->UpdateResourceImmediate(true);
    return RenderTarget;
}

static bool ReadBakePixels(UTextureRenderTarget2D* RenderTarget, TArray<FFloat16Color>& OutPixels)
{
    if (!RenderTarget)
    {
        return false;
    }

    FlushRenderingCommands();
    if (FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource())
    {
        return Resource->ReadFloat16Pixels(OutPixels);
    }
    return false;
}

static bool ExportBakeImage(const FString& FilePath, FIntPoint ImageSize, TArrayView<FFloat16Color> ImageData)
{
    const FString FileExtension = FPaths::GetExtension(FilePath, true);
    const EImageFormat ImageFormat = ImageWrapperHelper::GetImageFormat(FileExtension);
    if (ImageFormat == EImageFormat::Invalid)
    {
        return false;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    if (ImageFormat == EImageFormat::EXR || ImageFormat == EImageFormat::HDR)
    {
        TArray<FLinearColor> TempImageData;
        TempImageData.Reserve(ImageData.Num());
        for (const FFloat16Color& HalfColor : ImageData)
        {
            TempImageData.Emplace(HalfColor.GetFloats());
        }

        if (!ImageWrapper->SetRaw(
            TempImageData.GetData(),
            TempImageData.Num() * TempImageData.GetTypeSize(),
            ImageSize.X,
            ImageSize.Y,
            ERGBFormat::RGBAF,
            32))
        {
            return false;
        }
    }
    else
    {
        TArray<FColor> TempImageData;
        TempImageData.Reserve(ImageData.Num());
        for (const FFloat16Color& HalfColor : ImageData)
        {
            TempImageData.Add(HalfColor.GetFloats().ToFColor(true));
        }

        if (!ImageWrapper->SetRaw(
            TempImageData.GetData(),
            TempImageData.Num() * TempImageData.GetTypeSize(),
            ImageSize.X,
            ImageSize.Y,
            ERGBFormat::BGRA,
            8))
        {
            return false;
        }
    }

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed();
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

static void MergeBeautyAndAlphaPixels(
    const TArray<FFloat16Color>& BeautyPixels,
    const TArray<FFloat16Color>& AlphaPixels,
    TArray<FFloat16Color>& OutMergedPixels)
{
    check(BeautyPixels.Num() == AlphaPixels.Num());

    OutMergedPixels.SetNumUninitialized(BeautyPixels.Num());
    for (int32 PixelIndex = 0; PixelIndex < BeautyPixels.Num(); ++PixelIndex)
    {
        const FLinearColor BeautyColor = BeautyPixels[PixelIndex].GetFloats();
        const FLinearColor AlphaColor = AlphaPixels[PixelIndex].GetFloats();
        OutMergedPixels[PixelIndex] = FFloat16Color(FLinearColor(
            BeautyColor.R,
            BeautyColor.G,
            BeautyColor.B,
            FMath::Clamp(1.0f - AlphaColor.A, 0.0f, 1.0f)
        ));
    }
}
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Graph Form Tools
    if (CommandType == TEXT("get_niagara_graph"))
    {
        return HandleGetNiagaraGraph(Params);
    }
    else if (CommandType == TEXT("update_niagara_graph"))
    {
        return HandleUpdateNiagaraGraph(Params);
    }
    // Emitter Form Tools
    else if (CommandType == TEXT("get_niagara_emitter"))
    {
        return HandleGetNiagaraEmitter(Params);
    }
    else if (CommandType == TEXT("update_niagara_emitter"))
    {
        return HandleUpdateNiagaraEmitter(Params);
    }
    else if (CommandType == TEXT("get_niagara_compiled_code"))
    {
        return HandleGetNiagaraCompiledCode(Params);
    }
    else if (CommandType == TEXT("get_niagara_particle_attributes"))
    {
        return HandleGetNiagaraParticleAttributes(Params);
    }
    else if (CommandType == TEXT("bake_niagara_system"))
    {
        return HandleBakeNiagaraSystem(Params);
    }
    else
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        Result->SetBoolField("success", false);
        Result->SetStringField("error", FString::Printf(TEXT("Unknown Niagara command: %s"), *CommandType));
        return Result;
    }
}

UNiagaraSystem* FEpicUnrealMCPNiagaraCommands::LoadNiagaraSystemAsset(const FString& AssetPath)
{
    return LoadObject<UNiagaraSystem>(nullptr, *AssetPath);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleBakeNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

#if WITH_EDITOR
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required parameter: asset_path"));
        return Result;
    }

    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
        return Result;
    }

    UNiagaraBakerSettings* BakerSettings = NiagaraSystem->GetBakerSettings();
    if (!BakerSettings)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Niagara system does not provide baker settings"));
        return Result;
    }

    FString RequestedOutputDir;
    Params->TryGetStringField(TEXT("output_dir"), RequestedOutputDir);

    FString RequestedOutputPrefix;
    Params->TryGetStringField(TEXT("output_prefix"), RequestedOutputPrefix);

    FString FileExtension = TEXT(".png");
    Params->TryGetStringField(TEXT("file_extension"), FileExtension);
    FileExtension = NormalizeBakeFileExtension(FileExtension);

    double NumberValue = 0.0;

    int32 FrameWidth = 0;
    if (Params->TryGetNumberField(TEXT("frame_width"), NumberValue))
    {
        FrameWidth = FMath::RoundToInt(NumberValue);
    }

    int32 FrameHeight = 0;
    if (Params->TryGetNumberField(TEXT("frame_height"), NumberValue))
    {
        FrameHeight = FMath::RoundToInt(NumberValue);
    }

    float StartSeconds = BakerSettings->StartSeconds;
    if (Params->TryGetNumberField(TEXT("start_seconds"), NumberValue))
    {
        StartSeconds = static_cast<float>(NumberValue);
    }

    float DurationSeconds = BakerSettings->DurationSeconds;
    if (Params->TryGetNumberField(TEXT("duration_seconds"), NumberValue))
    {
        DurationSeconds = static_cast<float>(NumberValue);
    }

    int32 FramesPerSecond = BakerSettings->FramesPerSecond;
    if (Params->TryGetNumberField(TEXT("frames_per_second"), NumberValue))
    {
        FramesPerSecond = FMath::RoundToInt(NumberValue);
    }

    int32 FramesX = BakerSettings->FramesPerDimension.X;
    if (Params->TryGetNumberField(TEXT("frames_x"), NumberValue))
    {
        FramesX = FMath::RoundToInt(NumberValue);
    }

    int32 FramesY = BakerSettings->FramesPerDimension.Y;
    if (Params->TryGetNumberField(TEXT("frames_y"), NumberValue))
    {
        FramesY = FMath::RoundToInt(NumberValue);
    }

    bool bRenderComponentOnly = BakerSettings->bRenderComponentOnly;
    Params->TryGetBoolField(TEXT("render_component_only"), bRenderComponentOnly);

    bool bLockToSimulationFrameRate = BakerSettings->bLockToSimulationFrameRate;
    Params->TryGetBoolField(TEXT("lock_to_simulation_frame_rate"), bLockToSimulationFrameRate);

    if (DurationSeconds <= 0.0f)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("duration_seconds must be greater than 0"));
        return Result;
    }

    if (FramesPerSecond <= 0)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("frames_per_second must be greater than 0"));
        return Result;
    }

    if (FramesX <= 0 || FramesY <= 0)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("frames_x and frames_y must be greater than 0"));
        return Result;
    }

    FString OutputDir;
    FString OutputError;
    if (!TryResolveBakeOutputDirectory(AssetPath, RequestedOutputDir, OutputDir, OutputError))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), OutputError);
        return Result;
    }

    if (!IFileManager::Get().DirectoryExists(*OutputDir) && !IFileManager::Get().MakeDirectory(*OutputDir, true))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to create output directory: %s"), *OutputDir));
        return Result;
    }

    const FIntPoint FrameSize = ResolveBakeFrameSize(BakerSettings, FrameWidth, FrameHeight);
    const int32 TotalFrames = FramesX * FramesY;
    const float FrameDeltaSeconds = DurationSeconds / static_cast<float>(TotalFrames);
    const FString OutputPrefix = RequestedOutputPrefix.IsEmpty() ? NiagaraSystem->GetName() : RequestedOutputPrefix;

    CleanupExistingBakeSequence(OutputDir, OutputPrefix);

    FScopedBakerSettingsRestore ScopedRestore(BakerSettings);
    BakerSettings->StartSeconds = StartSeconds;
    BakerSettings->DurationSeconds = DurationSeconds;
    BakerSettings->FramesPerSecond = FramesPerSecond;
    BakerSettings->FramesPerDimension = FIntPoint(FramesX, FramesY);
    BakerSettings->bRenderComponentOnly = bRenderComponentOnly;
    BakerSettings->bLockToSimulationFrameRate = bLockToSimulationFrameRate;

    UTextureRenderTarget2D* BeautyRenderTarget = CreateBakeRenderTarget(FrameSize);
    UTextureRenderTarget2D* AlphaRenderTarget = CreateBakeRenderTarget(FrameSize);
    if (!BeautyRenderTarget || !AlphaRenderTarget)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to allocate bake render targets"));
        return Result;
    }

    FLocalNiagaraBakeRenderer BakeRenderer(NiagaraSystem);

    TArray<FFloat16Color> BeautyPixels;
    TArray<FFloat16Color> AlphaPixels;
    TArray<FFloat16Color> MergedPixels;
    TArray<TSharedPtr<FJsonValue>> OutputFiles;

    for (int32 FrameIndex = 0; FrameIndex < TotalFrames; ++FrameIndex)
    {
        const float FrameTime = StartSeconds + (static_cast<float>(FrameIndex) * FrameDeltaSeconds);
        BakeRenderer.SetAbsoluteTime(BakerSettings, FrameTime);
        BakeRenderer.RenderSceneCapture(BakerSettings, BeautyRenderTarget, ESceneCaptureSource::SCS_FinalToneCurveHDR);
        BakeRenderer.RenderSceneCapture(BakerSettings, AlphaRenderTarget, ESceneCaptureSource::SCS_SceneColorHDR);

        if (!ReadBakePixels(BeautyRenderTarget, BeautyPixels))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to read beauty pixels for frame %d"), FrameIndex));
            return Result;
        }
        if (!ReadBakePixels(AlphaRenderTarget, AlphaPixels))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to read alpha pixels for frame %d"), FrameIndex));
            return Result;
        }
        if (BeautyPixels.Num() != AlphaPixels.Num())
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Beauty/alpha pixel count mismatch on frame %d"), FrameIndex));
            return Result;
        }

        MergeBeautyAndAlphaPixels(BeautyPixels, AlphaPixels, MergedPixels);

        const FString FileName = FString::Printf(TEXT("%s_%04d%s"), *OutputPrefix, FrameIndex, *FileExtension);
        const FString FilePath = FPaths::Combine(OutputDir, FileName);
        if (!ExportBakeImage(FilePath, FrameSize, MergedPixels))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to export baked frame: %s"), *FilePath));
            return Result;
        }

        OutputFiles.Add(MakeShareable(new FJsonValueString(FilePath)));
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), NiagaraSystem->GetName());
    Result->SetStringField(TEXT("output_dir"), OutputDir);
    Result->SetStringField(TEXT("output_prefix"), OutputPrefix);
    Result->SetStringField(TEXT("file_extension"), FileExtension);
    Result->SetNumberField(TEXT("frame_width"), FrameSize.X);
    Result->SetNumberField(TEXT("frame_height"), FrameSize.Y);
    Result->SetNumberField(TEXT("frame_count"), TotalFrames);
    Result->SetNumberField(TEXT("frames_x"), FramesX);
    Result->SetNumberField(TEXT("frames_y"), FramesY);
    Result->SetNumberField(TEXT("frames_per_second"), FramesPerSecond);
    Result->SetNumberField(TEXT("start_seconds"), StartSeconds);
    Result->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
    Result->SetBoolField(TEXT("render_component_only"), bRenderComponentOnly);
    Result->SetArrayField(TEXT("files"), OutputFiles);
    return Result;
#else
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("bake_niagara_system is only available in editor builds"));
    return Result;
#endif
}

// ============================================================================
// Emitter Form Tools - Read Operations Implementation
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraEmitter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required parameter: asset_path"));
        return Result;
    }
    
    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
        return Result;
    }
    
    FString DetailLevel = TEXT("overview");
    Params->TryGetStringField(TEXT("detail_level"), DetailLevel);
    
    TArray<FString> IncludeSections = ParseIncludeSections(Params);

    int32 MaxDepth = 1;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
    MaxDepth = FMath::Clamp(MaxDepth, 0, 8);

    bool bIncludeAllProperties = false;
    Params->TryGetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);
    
    TArray<FString> RequestedEmitters;
    const TArray<TSharedPtr<FJsonValue>>* EmittersArray;
    if (Params->TryGetArrayField(TEXT("emitters"), EmittersArray))
    {
        for (const auto& Val : *EmittersArray)
        {
            RequestedEmitters.Add(Val->AsString());
        }
    }
    
    Result->SetStringField(TEXT("asset_name"), NiagaraSystem->GetName());
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetNumberField(TEXT("emitter_count"), NiagaraSystem->GetNumEmitters());
    Result->SetNumberField(TEXT("max_depth"), MaxDepth);
    Result->SetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);

    if (DetailLevel != TEXT("overview"))
    {
        Result->SetObjectField(TEXT("system_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(NiagaraSystem, MaxDepth, bIncludeAllProperties));
    }
    
    TArray<TSharedPtr<FJsonValue>> EmittersJson;
    
    for (FNiagaraEmitterHandle& Handle : NiagaraSystem->GetEmitterHandles())
    {
        if (!Handle.IsValid()) continue;
        
        FString EmitterName = Handle.GetName().ToString();
        
        if (RequestedEmitters.Num() > 0 && !RequestedEmitters.Contains(EmitterName))
        {
            continue;
        }
        
        if (DetailLevel == TEXT("overview"))
        {
            TSharedPtr<FJsonObject> EmitterOverview = MakeShareable(new FJsonObject);
            EmitterOverview->SetStringField(TEXT("name"), EmitterName);
            EmitterOverview->SetBoolField(TEXT("is_enabled"), Handle.GetIsEnabled());
            
            FString ModeStr = TEXT("Standard");
            if (Handle.GetEmitterMode() == ENiagaraEmitterMode::Stateless)
            {
                ModeStr = TEXT("Stateless");
            }
            EmitterOverview->SetStringField(TEXT("mode"), ModeStr);
            
            EmittersJson.Add(MakeShareable(new FJsonValueObject(EmitterOverview)));
        }
        else
        {
            TSharedPtr<FJsonObject> EmitterDetails = GetEmitterDetails(Handle, NiagaraSystem, IncludeSections, MaxDepth, bIncludeAllProperties);
            EmittersJson.Add(MakeShareable(new FJsonValueObject(EmitterDetails)));
        }
    }
    
    Result->SetArrayField(TEXT("emitters"), EmittersJson);
    Result->SetBoolField(TEXT("success"), true);
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetNiagaraSystemOverview(UNiagaraSystem* System)
{
    TSharedPtr<FJsonObject> Overview = MakeShareable(new FJsonObject);
    
    Overview->SetStringField(TEXT("name"), System->GetName());
    Overview->SetNumberField(TEXT("emitter_count"), System->GetNumEmitters());
    
    return Overview;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetEmitterDetails(FNiagaraEmitterHandle& Handle, 
    UNiagaraSystem* System, const TArray<FString>& IncludeSections, int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> EmitterJson = MakeShareable(new FJsonObject);
    
    FString EmitterName = Handle.GetName().ToString();
    EmitterJson->SetStringField(TEXT("name"), EmitterName);
    EmitterJson->SetBoolField(TEXT("is_enabled"), Handle.GetIsEnabled());
    
    FString ModeStr = TEXT("Standard");
    bool bIsStateless = false;
    if (Handle.GetEmitterMode() == ENiagaraEmitterMode::Stateless)
    {
        ModeStr = TEXT("Stateless");
        bIsStateless = true;
    }
    EmitterJson->SetStringField(TEXT("mode"), ModeStr);
    
    FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
    if (!EmitterData)
    {
        return EmitterJson;
    }
    
    // Stateless modules (UE 5.7+)
#if TAAGENT_WITH_NIAGARA_STATELESS
    if (bIsStateless && (ShouldInclude(IncludeSections, TEXT("modules")) || ShouldInclude(IncludeSections, TEXT("all"))))
    {
        TArray<TSharedPtr<FJsonValue>> ModulesJson;
        
        UNiagaraStatelessEmitter* StatelessEmitter = Handle.GetStatelessEmitter();
        if (StatelessEmitter)
        {
            for (UNiagaraStatelessModule* Module : StatelessEmitter->GetModules())
            {
                if (Module)
                {
                    TSharedPtr<FJsonObject> ModuleJson = MakeShareable(new FJsonObject);
                    ModuleJson = FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Module, MaxDepth, bIncludeAllProperties);
                    ModuleJson->SetStringField(TEXT("name"), Module->GetName());
                    ModuleJson->SetStringField(TEXT("type"), Module->GetClass()->GetName());
                    ModuleJson->SetBoolField(TEXT("is_enabled"), Module->IsModuleEnabled());
                    ModulesJson.Add(MakeShareable(new FJsonValueObject(ModuleJson)));
                }
            }
        }
        
        EmitterJson->SetArrayField(TEXT("modules"), ModulesJson);
        EmitterJson->SetNumberField(TEXT("module_count"), ModulesJson.Num());
    }
#endif
    
    // Standard modules (from Graph nodes)
    if (!bIsStateless && (ShouldInclude(IncludeSections, TEXT("modules")) || ShouldInclude(IncludeSections, TEXT("all"))))
    {
        TArray<TSharedPtr<FJsonValue>> ModulesJson;
        TSet<FString> UniqueModuleNames;  // Avoid duplicates
        
        // Get modules from Spawn script
        if (EmitterData->SpawnScriptProps.Script)
        {
            UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(EmitterData->SpawnScriptProps.Script->GetLatestSource());
            UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
            if (Graph)
            {
                for (UEdGraphNode* NodeBase : Graph->Nodes)
                {
                    UNiagaraNode* Node = Cast<UNiagaraNode>(NodeBase);
                    if (!Node) continue;
                    
                    FString NodeClass = Node->GetClass()->GetName();
                    // Check if this is a module node
                    if (NodeClass.Contains(TEXT("Module")) || NodeClass.Contains(TEXT("Output")))
                    {
                        FString ModuleName = Node->GetName();
                        
                        // Skip duplicates and system nodes
                        if (UniqueModuleNames.Contains(ModuleName))
                        {
                            continue;
                        }
                        
                        // Skip output and input nodes
                        if (NodeClass.Contains(TEXT("Output")) || NodeClass.Contains(TEXT("Input")))
                        {
                            continue;
                        }
                        
                        UniqueModuleNames.Add(ModuleName);
                        
                        TSharedPtr<FJsonObject> ModuleJson = GetNodeDetails(Node, MaxDepth, bIncludeAllProperties);
                        ModuleJson->SetStringField(TEXT("name"), ModuleName);
                        ModuleJson->SetStringField(TEXT("type"), NodeClass);
                        ModuleJson->SetStringField(TEXT("script"), TEXT("spawn"));
                        ModuleJson->SetBoolField(TEXT("is_enabled"), true);  // Standard modules don't have enabled state
                        ModulesJson.Add(MakeShareable(new FJsonValueObject(ModuleJson)));
                    }
                }
            }
        }
        
        // Get modules from Update script
        if (EmitterData->UpdateScriptProps.Script)
        {
            UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(EmitterData->UpdateScriptProps.Script->GetLatestSource());
            UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
            if (Graph)
            {
                for (UEdGraphNode* NodeBase : Graph->Nodes)
                {
                    UNiagaraNode* Node = Cast<UNiagaraNode>(NodeBase);
                    if (!Node) continue;
                    
                    FString NodeClass = Node->GetClass()->GetName();
                    if (NodeClass.Contains(TEXT("Module")))
                    {
                        FString ModuleName = Node->GetName();
                        
                        if (UniqueModuleNames.Contains(ModuleName))
                        {
                            continue;
                        }
                        
                        UniqueModuleNames.Add(ModuleName);
                        
                        TSharedPtr<FJsonObject> ModuleJson = GetNodeDetails(Node, MaxDepth, bIncludeAllProperties);
                        ModuleJson->SetStringField(TEXT("name"), ModuleName);
                        ModuleJson->SetStringField(TEXT("type"), NodeClass);
                        ModuleJson->SetStringField(TEXT("script"), TEXT("update"));
                        ModuleJson->SetBoolField(TEXT("is_enabled"), true);
                        ModulesJson.Add(MakeShareable(new FJsonValueObject(ModuleJson)));
                    }
                }
            }
        }
        
        EmitterJson->SetArrayField(TEXT("modules"), ModulesJson);
        EmitterJson->SetNumberField(TEXT("module_count"), ModulesJson.Num());
    }
    
    // Scripts (Standard mode only)
    if (!bIsStateless && (ShouldInclude(IncludeSections, TEXT("scripts")) || ShouldInclude(IncludeSections, TEXT("all"))))
    {
        TSharedPtr<FJsonObject> ScriptsJson = MakeShareable(new FJsonObject);
        
        if (EmitterData->SpawnScriptProps.Script)
        {
            ScriptsJson->SetObjectField(TEXT("spawn"), GetScriptDetails(EmitterData->SpawnScriptProps.Script, MaxDepth, bIncludeAllProperties));
        }
        if (EmitterData->UpdateScriptProps.Script)
        {
            ScriptsJson->SetObjectField(TEXT("update"), GetScriptDetails(EmitterData->UpdateScriptProps.Script, MaxDepth, bIncludeAllProperties));
        }
        
        TArray<TSharedPtr<FJsonValue>> EventHandlersJson;
        for (const FNiagaraEventScriptProperties& EventHandler : EmitterData->GetEventHandlers())
        {
            if (EventHandler.Script)
            {
                TSharedPtr<FJsonObject> EventJson = GetScriptDetails(EventHandler.Script, MaxDepth, bIncludeAllProperties);
                EventJson->SetStringField(TEXT("execution_mode"), 
                    EventHandler.ExecutionMode == EScriptExecutionMode::EveryParticle ? TEXT("EveryParticle") : 
                    (EventHandler.ExecutionMode == EScriptExecutionMode::SpawnedParticles ? TEXT("SpawnedParticles") : TEXT("SingleParticle")));
                EventHandlersJson.Add(MakeShareable(new FJsonValueObject(EventJson)));
            }
        }
        if (EventHandlersJson.Num() > 0)
        {
            ScriptsJson->SetArrayField(TEXT("event_handlers"), EventHandlersJson);
        }
        
        EmitterJson->SetObjectField(TEXT("scripts"), ScriptsJson);
    }
    
    // Renderers
    if (ShouldInclude(IncludeSections, TEXT("renderers")) || ShouldInclude(IncludeSections, TEXT("all")))
    {
        TArray<TSharedPtr<FJsonValue>> RenderersJson;
        const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
        for (UNiagaraRendererProperties* Renderer : Renderers)
        {
            if (Renderer)
            {
                RenderersJson.Add(MakeShareable(new FJsonValueObject(GetRendererDetails(Renderer, MaxDepth, bIncludeAllProperties))));
            }
        }
        EmitterJson->SetArrayField(TEXT("renderers"), RenderersJson);
        EmitterJson->SetNumberField(TEXT("renderer_count"), RenderersJson.Num());
    }
    
    // Simulation stages
    if (ShouldInclude(IncludeSections, TEXT("simulation_stages")) || ShouldInclude(IncludeSections, TEXT("all")))
    {
        TArray<TSharedPtr<FJsonValue>> StagesJson;
        for (UNiagaraSimulationStageBase* Stage : EmitterData->GetSimulationStages())
        {
            if (Stage)
            {
                StagesJson.Add(MakeShareable(new FJsonValueObject(GetSimulationStageDetails(Stage, MaxDepth, bIncludeAllProperties))));
            }
        }
        EmitterJson->SetArrayField(TEXT("simulation_stages"), StagesJson);
        EmitterJson->SetNumberField(TEXT("simulation_stage_count"), StagesJson.Num());
    }
    
    // Parameters
    if (ShouldInclude(IncludeSections, TEXT("parameters")) || ShouldInclude(IncludeSections, TEXT("all")))
    {
        TArray<TSharedPtr<FJsonValue>> ParametersJson;
        
        if (bIsStateless)
        {
            // Stateless parameters
#if TAAGENT_WITH_NIAGARA_STATELESS
            UNiagaraStatelessEmitter* StatelessEmitter = Handle.GetStatelessEmitter();
            if (StatelessEmitter)
            {
                TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
                ParamInfo->SetStringField(TEXT("source"), TEXT("stateless_modules"));
                ParamInfo->SetStringField(TEXT("note"), TEXT("Use 'modules' section to see available modules and their properties"));
                ParametersJson.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
            }
#endif
        }
        else
        {
            // Standard mode - parameters from scripts
            if (EmitterData->SpawnScriptProps.Script)
            {
                TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
                ParamInfo->SetStringField(TEXT("source"), TEXT("spawn_script"));
                ParamInfo->SetStringField(TEXT("script_name"), EmitterData->SpawnScriptProps.Script->GetName());
                ParametersJson.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
            }
        }
        
        EmitterJson->SetArrayField(TEXT("parameters"), ParametersJson);
    }
    
    // Stateless compatibility analysis (Standard mode only)
    if (!bIsStateless && ShouldInclude(IncludeSections, TEXT("stateless_analysis")))
    {
        TSharedPtr<FJsonObject> Analysis = AnalyzeStatelessCompatibility(Handle);
        EmitterJson->SetObjectField(TEXT("stateless_analysis"), Analysis);
    }
    
    return EmitterJson;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetScriptDetails(UNiagaraScript* Script,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> ScriptJson = MakeShareable(new FJsonObject);
    
    if (!Script)
    {
        return ScriptJson;
    }
    
    ScriptJson->SetStringField(TEXT("name"), Script->GetName());
    ScriptJson->SetStringField(TEXT("path"), Script->GetPathName());
    ScriptJson->SetObjectField(TEXT("script_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Script, MaxDepth, bIncludeAllProperties));
    
    FString UsageStr = TEXT("Unknown");
    switch (Script->GetUsage())
    {
        case ENiagaraScriptUsage::Function: UsageStr = TEXT("Function"); break;
        case ENiagaraScriptUsage::Module: UsageStr = TEXT("Module"); break;
        case ENiagaraScriptUsage::DynamicInput: UsageStr = TEXT("DynamicInput"); break;
        case ENiagaraScriptUsage::ParticleSpawnScript: UsageStr = TEXT("ParticleSpawn"); break;
        case ENiagaraScriptUsage::ParticleUpdateScript: UsageStr = TEXT("ParticleUpdate"); break;
        case ENiagaraScriptUsage::EmitterSpawnScript: UsageStr = TEXT("EmitterSpawn"); break;
        case ENiagaraScriptUsage::EmitterUpdateScript: UsageStr = TEXT("EmitterUpdate"); break;
        case ENiagaraScriptUsage::SystemSpawnScript: UsageStr = TEXT("SystemSpawn"); break;
        case ENiagaraScriptUsage::SystemUpdateScript: UsageStr = TEXT("SystemUpdate"); break;
        case ENiagaraScriptUsage::ParticleEventScript: UsageStr = TEXT("ParticleEvent"); break;
        default: break;
    }
    ScriptJson->SetStringField(TEXT("usage"), UsageStr);

    if (UObject* ScriptSourceObject = Script->GetLatestSource())
    {
        ScriptJson->SetObjectField(TEXT("source_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(ScriptSourceObject, FMath::Max(MaxDepth - 1, 0), bIncludeAllProperties));
        if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(ScriptSourceObject))
        {
            if (Source->NodeGraph)
            {
                ScriptJson->SetObjectField(TEXT("graph_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Source->NodeGraph, FMath::Max(MaxDepth - 1, 0), bIncludeAllProperties));
            }
        }
    }
    
    TArray<TSharedPtr<FJsonValue>> ParametersJson;
    const FNiagaraParameterStore& ParamStore = Script->RapidIterationParameters;
    
    TArrayView<const FNiagaraVariableWithOffset> ParamVariables = ParamStore.ReadParameterVariables();
    for (const FNiagaraVariableWithOffset& ParamWithOffset : ParamVariables)
    {
        TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
        
        FString ParamName = ParamWithOffset.GetName().ToString();
        ParamJson->SetStringField(TEXT("name"), ParamName);
        
        FNiagaraTypeDefinition ParamType = ParamWithOffset.GetType();
        FString TypeName = ParamType.GetName();
        ParamJson->SetStringField(TEXT("type"), TypeName);
        
        int32 Offset = ParamWithOffset.Offset;
        if (Offset >= 0)
        {
            const FNiagaraTypeDefinition& TypeDef = ParamWithOffset.GetType();
            
            if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
            {
                float Value = ParamStore.GetParameterValueFromOffset<float>(Offset);
                ParamJson->SetNumberField(TEXT("value"), Value);
            }
            else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
            {
                int32 Value = ParamStore.GetParameterValueFromOffset<int32>(Offset);
                ParamJson->SetNumberField(TEXT("value"), Value);
            }
            else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
            {
                bool Value = ParamStore.GetParameterValueFromOffset<bool>(Offset);
                ParamJson->SetBoolField(TEXT("value"), Value);
            }
            else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def() || 
                     TypeDef == FNiagaraTypeDefinition::GetPositionDef())
            {
                FVector3f Value = ParamStore.GetParameterValueFromOffset<FVector3f>(Offset);
                TArray<TSharedPtr<FJsonValue>> VecArray;
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.X)));
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.Y)));
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.Z)));
                ParamJson->SetArrayField(TEXT("value"), VecArray);
            }
            else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def() ||
                     TypeDef == FNiagaraTypeDefinition::GetColorDef())
            {
                FVector4f Value = ParamStore.GetParameterValueFromOffset<FVector4f>(Offset);
                TArray<TSharedPtr<FJsonValue>> VecArray;
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.X)));
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.Y)));
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.Z)));
                VecArray.Add(MakeShareable(new FJsonValueNumber(Value.W)));
                ParamJson->SetArrayField(TEXT("value"), VecArray);
            }
            else
            {
                int32 Size = TypeDef.GetSize();
                ParamJson->SetNumberField(TEXT("size_bytes"), Size);
            }
        }
        
        ParametersJson.Add(MakeShareable(new FJsonValueObject(ParamJson)));
    }
    
    ScriptJson->SetArrayField(TEXT("parameters"), ParametersJson);
    ScriptJson->SetNumberField(TEXT("parameter_count"), ParametersJson.Num());
    
    return ScriptJson;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetRendererDetails(UNiagaraRendererProperties* Renderer,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> RendererJson = MakeShareable(new FJsonObject);
    
    if (!Renderer)
    {
        return RendererJson;
    }
    
    FString RendererType = Renderer->GetClass()->GetName();
    RendererType.RemoveFromEnd(TEXT("Properties"));
    RendererJson->SetStringField(TEXT("name"), Renderer->GetName());
    RendererJson->SetStringField(TEXT("class"), Renderer->GetClass()->GetName());
    RendererJson->SetStringField(TEXT("type"), RendererType);
    RendererJson->SetBoolField(TEXT("is_enabled"), Renderer->GetIsEnabled());
    RendererJson->SetObjectField(TEXT("renderer_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Renderer, MaxDepth, bIncludeAllProperties));
    
    return RendererJson;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetSimulationStageDetails(UNiagaraSimulationStageBase* Stage,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> StageJson = MakeShareable(new FJsonObject);
    
    if (!Stage)
    {
        return StageJson;
    }
    
    StageJson->SetStringField(TEXT("name"), Stage->GetName());
    StageJson->SetStringField(TEXT("class"), Stage->GetClass()->GetName());
    StageJson->SetBoolField(TEXT("enabled"), Stage->bEnabled);
    StageJson->SetObjectField(TEXT("stage_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Stage, MaxDepth, bIncludeAllProperties));
    
    return StageJson;
}

// ============================================================================
// Emitter Form Tools - Update Operations Implementation
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleUpdateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required parameter: asset_path"));
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* OperationsArray;
    if (!Params->TryGetArrayField(TEXT("operations"), OperationsArray))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required parameter: operations"));
        return Result;
    }
    
    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
        return Result;
    }
    
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 SuccessCount = 0;
    int32 FailCount = 0;
    
    for (const TSharedPtr<FJsonValue>& OpValue : *OperationsArray)
    {
        TSharedPtr<FJsonObject> Op = OpValue->AsObject();
        if (!Op.IsValid()) continue;
        
        FString Target = Op->GetStringField(TEXT("target"));
        TSharedPtr<FJsonObject> OpResult;
        
        if (Target == TEXT("emitter"))
        {
            OpResult = ProcessEmitterOperation(NiagaraSystem, Op);
        }
        else if (Target == TEXT("renderer"))
        {
            OpResult = ProcessRendererOperation(NiagaraSystem, Op);
        }
        else if (Target == TEXT("parameter"))
        {
            OpResult = ProcessParameterOperation(NiagaraSystem, Op);
        }
        else if (Target == TEXT("sim_stage"))
        {
            OpResult = ProcessSimStageOperation(NiagaraSystem, Op);
        }
        else if (Target == TEXT("stateless_module"))
        {
            OpResult = ProcessStatelessModuleOperation(NiagaraSystem, Op);
        }
        else
        {
            OpResult = MakeShareable(new FJsonObject);
            OpResult->SetBoolField(TEXT("success"), false);
            OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown target: %s"), *Target));
        }
        
        if (OpResult->GetBoolField(TEXT("success")))
        {
            SuccessCount++;
        }
        else
        {
            FailCount++;
        }
        
        ResultsArray.Add(MakeShareable(new FJsonValueObject(OpResult)));
    }
    
    // Mark package dirty
    NiagaraSystem->MarkPackageDirty();
    
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetNumberField(TEXT("success_count"), SuccessCount);
    Result->SetNumberField(TEXT("fail_count"), FailCount);
    Result->SetArrayField(TEXT("results"), ResultsArray);
    
    return Result;
}

FNiagaraEmitterHandle* FEpicUnrealMCPNiagaraCommands::FindEmitterHandle(UNiagaraSystem* System, const FString& EmitterName)
{
    if (!System) return nullptr;
    
    TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
    for (FNiagaraEmitterHandle& Handle : Handles)
    {
        if (Handle.IsValid() && Handle.GetName().ToString() == EmitterName)
        {
            return &Handle;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ProcessEmitterOperation(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Op)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString Action = Op->GetStringField(TEXT("action"));
    FString EmitterName;
    Op->TryGetStringField(TEXT("name"), EmitterName);
    
    // Add emitter
    if (Action == TEXT("add"))
    {
        FString TemplatePath;
        if (!Op->TryGetStringField(TEXT("template"), TemplatePath))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Missing 'template' parameter for add action"));
            return Result;
        }
        
        UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
        if (!TemplateEmitter)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load emitter template: %s"), *TemplatePath));
            return Result;
        }
        
        FGuid NewId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *TemplateEmitter, FGuid::NewGuid(), true);
        if (NewId.IsValid())
        {
            // Rename if name provided
            if (!EmitterName.IsEmpty())
            {
                FNiagaraEmitterHandle* NewHandle = nullptr;
                for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
                {
                    if (Handle.GetId() == NewId)
                    {
                        NewHandle = &Handle;
                        break;
                    }
                }
                if (NewHandle)
                {
                    NewHandle->SetName(*EmitterName, *System);
                }
            }
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("action"), TEXT("add"));
            Result->SetStringField(TEXT("emitter_name"), EmitterName);
            Result->SetStringField(TEXT("emitter_id"), NewId.ToString());
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to add emitter to system"));
        }
        return Result;
    }
    
    // Remove emitter
    if (Action == TEXT("remove"))
    {
        FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
        if (!Handle)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
            return Result;
        }
        
        TSet<FGuid> IdsToDelete;
        IdsToDelete.Add(Handle->GetId());
        System->RemoveEmitterHandlesById(IdsToDelete);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("remove"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        return Result;
    }
    
    // Find emitter for other operations
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
        return Result;
    }
    
    // Set enabled
    if (Action == TEXT("set_enabled"))
    {
        bool bEnabled = Op->GetBoolField(TEXT("value"));
        Handle->SetIsEnabled(bEnabled, *System, true);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_enabled"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetBoolField(TEXT("value"), bEnabled);
    }
    // Rename
    else if (Action == TEXT("rename"))
    {
        FString NewName = Op->GetStringField(TEXT("value"));
        Handle->SetName(*NewName, *System);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("rename"));
        Result->SetStringField(TEXT("old_name"), EmitterName);
        Result->SetStringField(TEXT("new_name"), NewName);
    }
    // Set mode
    else if (Action == TEXT("set_mode"))
    {
        FString ModeStr = Op->GetStringField(TEXT("value"));
        ENiagaraEmitterMode Mode = ENiagaraEmitterMode::Standard;
        if (ModeStr.ToLower() == TEXT("stateless"))
        {
            Mode = ENiagaraEmitterMode::Stateless;
        }
        Handle->SetEmitterMode(*System, Mode);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_mode"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("mode"), ModeStr);
    }
    // Add module to emitter script (spawn or update)
    else if (Action == TEXT("add_module"))
    {
        FString ModulePath;
        if (!Op->TryGetStringField(TEXT("module_path"), ModulePath))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Missing 'module_path' parameter for add_module action"));
            return Result;
        }

        // Determine which script to add to (default: update)
        FString ScriptType = TEXT("update");
        Op->TryGetStringField(TEXT("script"), ScriptType);

        ENiagaraScriptUsage TargetUsage = (ScriptType.ToLower() == TEXT("spawn"))
            ? ENiagaraScriptUsage::ParticleSpawnScript
            : ENiagaraScriptUsage::ParticleUpdateScript;

        // Load module script
        UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
        if (!ModuleScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load module: %s"), *ModulePath));
            return Result;
        }

        // Get emitter data and target script
        FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
        if (!EmitterData)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
            return Result;
        }

        UNiagaraScript* TargetScript = EmitterData->GetScript(TargetUsage, FGuid());
        if (!TargetScript)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to get %s script"), *ScriptType));
            return Result;
        }

        // Get graph from script source
        UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(TargetScript->GetSource(FGuid()));
        if (!ScriptSource || !ScriptSource->NodeGraph)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get script graph"));
            return Result;
        }

        UNiagaraGraph* Graph = ScriptSource->NodeGraph;

        // Find output node
        UNiagaraNodeOutput* OutputNode = nullptr;
        for (UEdGraphNode* NodeBase : Graph->Nodes)
        {
            UNiagaraNodeOutput* Node = Cast<UNiagaraNodeOutput>(NodeBase);
            if (Node && Node->GetUsage() == TargetUsage)
            {
                OutputNode = Node;
                break;
            }
        }

        if (!OutputNode)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to find output node"));
            return Result;
        }

        // Add module to stack
        UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
            ModuleScript,
            *OutputNode,
            INDEX_NONE,      // TargetIndex - add to end
            FString(),       // SuggestedName - empty for default
            FGuid()          // VersionGuid - empty for default
        );

        if (NewModule)
        {
            TargetScript->Modify();

            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("action"), TEXT("add_module"));
            Result->SetStringField(TEXT("emitter_name"), EmitterName);
            Result->SetStringField(TEXT("script"), ScriptType);
            Result->SetStringField(TEXT("module_path"), ModulePath);
            Result->SetStringField(TEXT("module_name"), NewModule->GetName());
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to add module to stack"));
        }
    }
    // Convert to Stateless mode (UE 5.7+)
    else if (Action == TEXT("convert_to_stateless"))
    {
#if TAAGENT_WITH_NIAGARA_STATELESS
        // Check if already Stateless
        if (Handle->GetEmitterMode() == ENiagaraEmitterMode::Stateless)
        {
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("message"), TEXT("Emitter is already in Stateless mode"));
            return Result;
        }
        
        // Optional: force conversion even with warnings
        bool bForce = false;
        Op->TryGetBoolField(TEXT("force"), bForce);
        
        // Analyze compatibility
        TSharedPtr<FJsonObject> Analysis = AnalyzeStatelessCompatibility(*Handle);
        bool bCanConvert = Analysis->GetBoolField(TEXT("can_convert"));
        
        if (!bCanConvert && !bForce)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Emitter is not convertible. Use 'force=true' to attempt anyway."));
            Result->SetObjectField(TEXT("analysis"), Analysis);
            return Result;
        }
        
        // Get parameter values before conversion
        FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
        TMap<FString, float> FloatValues;
        TMap<FString, FVector3f> VectorValues;
        TMap<FString, FVector4f> ColorValues;
        
        auto CaptureParamValues = [&](UNiagaraScript* Script)
        {
            if (!Script) return;
            
            const FNiagaraParameterStore& ParamStore = Script->RapidIterationParameters;
            TArrayView<const FNiagaraVariableWithOffset> ParamVariables = ParamStore.ReadParameterVariables();
            
            for (const FNiagaraVariableWithOffset& ParamWithOffset : ParamVariables)
            {
                FString ParamName = ParamWithOffset.GetName().ToString();
                int32 Offset = ParamWithOffset.Offset;
                const FNiagaraTypeDefinition& TypeDef = ParamWithOffset.GetType();
                
                if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
                {
                    FloatValues.Add(ParamName, ParamStore.GetParameterValueFromOffset<float>(Offset));
                }
                else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def() || 
                         TypeDef == FNiagaraTypeDefinition::GetPositionDef())
                {
                    VectorValues.Add(ParamName, ParamStore.GetParameterValueFromOffset<FVector3f>(Offset));
                }
                else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def() ||
                         TypeDef == FNiagaraTypeDefinition::GetColorDef())
                {
                    ColorValues.Add(ParamName, ParamStore.GetParameterValueFromOffset<FVector4f>(Offset));
                }
            }
        };
        
        if (EmitterData)
        {
            if (EmitterData->SpawnScriptProps.Script)
            {
                CaptureParamValues(EmitterData->SpawnScriptProps.Script);
            }
            if (EmitterData->UpdateScriptProps.Script)
            {
                CaptureParamValues(EmitterData->UpdateScriptProps.Script);
            }
        }
        
        // Switch to Stateless mode
        Handle->SetEmitterMode(*System, ENiagaraEmitterMode::Stateless);
        
        // Get the Stateless emitter and migrate parameters
        UNiagaraStatelessEmitter* StatelessEmitter = Handle->GetStatelessEmitter();
        if (StatelessEmitter)
        {
            TArray<TSharedPtr<FJsonValue>> MigratedParams;
            
            for (UNiagaraStatelessModule* Module : StatelessEmitter->GetModules())
            {
                if (!Module) continue;
                
                FString ModuleName = Module->GetName();
                FString ParamPattern;
                
                if (ModuleName == TEXT("Lifetime")) ParamPattern = TEXT("Particles.Lifetime");
                else if (ModuleName == TEXT("Color")) ParamPattern = TEXT("Particles.Color");
                else if (ModuleName == TEXT("Size")) ParamPattern = TEXT("Particles.SpriteSize");
                else if (ModuleName == TEXT("Scale")) ParamPattern = TEXT("Particles.Scale");
                else if (ModuleName == TEXT("Rotation")) ParamPattern = TEXT("Particles.SpriteRotation");
                else if (ModuleName == TEXT("Velocity")) ParamPattern = TEXT("Particles.Velocity");
                else if (ModuleName == TEXT("SpawnRate")) ParamPattern = TEXT("SpawnRate");
                else continue;
                
                // Migrate float values
                for (auto& Pair : FloatValues)
                {
                    if (Pair.Key.StartsWith(ParamPattern))
                    {
                        FProperty* Property = Module->GetClass()->FindPropertyByName(TEXT("Value"));
                        if (!Property)
                        {
                            if (ModuleName == TEXT("Lifetime")) Property = Module->GetClass()->FindPropertyByName(TEXT("Lifetime"));
                            else if (ModuleName == TEXT("SpawnRate")) Property = Module->GetClass()->FindPropertyByName(TEXT("SpawnRate"));
                        }
                        
                        if (Property)
                        {
                            if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                            {
                                FloatProp->SetValue_InContainer(Module, Pair.Value);
                                
                                TSharedPtr<FJsonObject> Migrated = MakeShareable(new FJsonObject);
                                Migrated->SetStringField(TEXT("parameter"), Pair.Key);
                                Migrated->SetStringField(TEXT("module"), ModuleName);
                                Migrated->SetNumberField(TEXT("value"), Pair.Value);
                                MigratedParams.Add(MakeShareable(new FJsonValueObject(Migrated)));
                            }
                        }
                    }
                }
            }
            
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("action"), TEXT("convert_to_stateless"));
            Result->SetStringField(TEXT("emitter_name"), EmitterName);
            Result->SetStringField(TEXT("new_mode"), TEXT("Stateless"));
            Result->SetArrayField(TEXT("migrated_parameters"), MigratedParams);
            Result->SetNumberField(TEXT("migrated_count"), MigratedParams.Num());
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get StatelessEmitter after mode switch"));
        }
#else
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Stateless conversion requires UE 5.7 or later"));
#endif
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown emitter action: %s"), *Action));
    }

    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ProcessRendererOperation(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Op)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString EmitterName = Op->GetStringField(TEXT("emitter"));
    FString Action = Op->GetStringField(TEXT("action"));
    
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
        return Result;
    }
    
    FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
    if (!EmitterData)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
        return Result;
    }
    
    int32 Index = 0;
    if (Op->HasField(TEXT("index")))
    {
        Index = (int32)Op->GetNumberField(TEXT("index"));
    }
    
    const TArray<UNiagaraRendererProperties*>& Renderers = EmitterData->GetRenderers();
    if (Index < 0 || Index >= Renderers.Num())
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid renderer index: %d (valid: 0-%d)"), Index, Renderers.Num() - 1));
        return Result;
    }
    
    UNiagaraRendererProperties* Renderer = Renderers[Index];
    if (!Renderer)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Renderer is null"));
        return Result;
    }
    
    if (Action == TEXT("set_enabled"))
    {
        bool bEnabled = Op->GetBoolField(TEXT("value"));
        Renderer->SetIsEnabled(bEnabled);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_enabled"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetNumberField(TEXT("renderer_index"), Index);
        Result->SetBoolField(TEXT("value"), bEnabled);
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown renderer action: %s"), *Action));
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ProcessParameterOperation(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Op)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString EmitterName = Op->GetStringField(TEXT("emitter"));
    FString ScriptType = Op->GetStringField(TEXT("script")); // "spawn", "update", or "event"
    FString ParamName = Op->GetStringField(TEXT("name"));
    
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
        return Result;
    }
    
    FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
    if (!EmitterData)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
        return Result;
    }
    
    UNiagaraScript* Script = nullptr;
    if (ScriptType.ToLower() == TEXT("spawn"))
    {
        Script = EmitterData->SpawnScriptProps.Script;
    }
    else if (ScriptType.ToLower() == TEXT("update"))
    {
        Script = EmitterData->UpdateScriptProps.Script;
    }
    else if (ScriptType.ToLower() == TEXT("event"))
    {
        int32 EventIndex = 0;
        Op->TryGetNumberField(TEXT("event_index"), EventIndex);

        const TArray<FNiagaraEventScriptProperties>& EventHandlers = EmitterData->GetEventHandlers();
        if (EventHandlers.IsValidIndex(EventIndex))
        {
            Script = EventHandlers[EventIndex].Script;
        }
    }
    
    if (!Script)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Script not found: %s"), *ScriptType));
        return Result;
    }
    
    FNiagaraParameterStore& ParamStore = Script->RapidIterationParameters;
    
    // Find parameter by name
    TArrayView<const FNiagaraVariableWithOffset> ParamVariables = ParamStore.ReadParameterVariables();
    int32 FoundOffset = -1;
    FNiagaraTypeDefinition FoundType;
    
    for (const FNiagaraVariableWithOffset& ParamWithOffset : ParamVariables)
    {
        if (ParamWithOffset.GetName().ToString() == ParamName)
        {
            FoundOffset = ParamWithOffset.Offset;
            FoundType = ParamWithOffset.GetType();
            break;
        }
    }
    
    if (FoundOffset < 0)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Parameter not found: %s"), *ParamName));
        return Result;
    }
    
    // Get value from operation
    const TSharedPtr<FJsonValue>* ValuePtr = Op->Values.Find(TEXT("value"));
    if (!ValuePtr)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing 'value' parameter"));
        return Result;
    }
    
    // Set value based on type using SetParameterData (public method)
    bool bSetSuccess = false;
    
    if (FoundType == FNiagaraTypeDefinition::GetFloatDef())
    {
        float Value = (float)(*ValuePtr)->AsNumber();
        ParamStore.SetParameterData(reinterpret_cast<const uint8*>(&Value), FoundOffset, sizeof(float));
        bSetSuccess = true;
        Result->SetNumberField(TEXT("value_set"), Value);
    }
    else if (FoundType == FNiagaraTypeDefinition::GetIntDef())
    {
        int32 Value = (int32)(*ValuePtr)->AsNumber();
        ParamStore.SetParameterData(reinterpret_cast<const uint8*>(&Value), FoundOffset, sizeof(int32));
        bSetSuccess = true;
        Result->SetNumberField(TEXT("value_set"), Value);
    }
    else if (FoundType == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool Value = (*ValuePtr)->AsBool();
        ParamStore.SetParameterData(reinterpret_cast<const uint8*>(&Value), FoundOffset, sizeof(bool));
        bSetSuccess = true;
        Result->SetBoolField(TEXT("value_set"), Value);
    }
    else if (FoundType == FNiagaraTypeDefinition::GetVec3Def() || 
             FoundType == FNiagaraTypeDefinition::GetPositionDef())
    {
        const TArray<TSharedPtr<FJsonValue>>* VecArray;
        if ((*ValuePtr)->TryGetArray(VecArray) && VecArray->Num() >= 3)
        {
            FVector3f Value;
            Value.X = (float)(*VecArray)[0]->AsNumber();
            Value.Y = (float)(*VecArray)[1]->AsNumber();
            Value.Z = (float)(*VecArray)[2]->AsNumber();
            ParamStore.SetParameterData(reinterpret_cast<const uint8*>(&Value), FoundOffset, sizeof(FVector3f));
            bSetSuccess = true;
        }
    }
    else if (FoundType == FNiagaraTypeDefinition::GetVec4Def() ||
             FoundType == FNiagaraTypeDefinition::GetColorDef())
    {
        const TArray<TSharedPtr<FJsonValue>>* VecArray;
        if ((*ValuePtr)->TryGetArray(VecArray) && VecArray->Num() >= 4)
        {
            FVector4f Value;
            Value.X = (float)(*VecArray)[0]->AsNumber();
            Value.Y = (float)(*VecArray)[1]->AsNumber();
            Value.Z = (float)(*VecArray)[2]->AsNumber();
            Value.W = (float)(*VecArray)[3]->AsNumber();
            ParamStore.SetParameterData(reinterpret_cast<const uint8*>(&Value), FoundOffset, sizeof(FVector4f));
            bSetSuccess = true;
        }
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported parameter type: %s"), *FoundType.GetName()));
        return Result;
    }
    
    if (bSetSuccess)
    {
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_parameter"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("script"), ScriptType);
        Result->SetStringField(TEXT("parameter_name"), ParamName);
        Result->SetStringField(TEXT("parameter_type"), FoundType.GetName());
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to set parameter value"));
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ProcessSimStageOperation(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Op)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString EmitterName = Op->GetStringField(TEXT("emitter"));
    FString StageName = Op->GetStringField(TEXT("name"));
    FString Action = Op->GetStringField(TEXT("action"));
    
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
        return Result;
    }
    
    FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
    if (!EmitterData)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
        return Result;
    }
    
    // Find simulation stage by name
    UNiagaraSimulationStageBase* FoundStage = nullptr;
    for (UNiagaraSimulationStageBase* Stage : EmitterData->GetSimulationStages())
    {
        if (Stage && Stage->GetName() == StageName)
        {
            FoundStage = Stage;
            break;
        }
    }
    
    if (!FoundStage)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Simulation stage not found: %s"), *StageName));
        return Result;
    }
    
    if (Action == TEXT("set_enabled"))
    {
        bool bEnabled = Op->GetBoolField(TEXT("value"));
        FoundStage->bEnabled = bEnabled;
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_enabled"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("stage_name"), StageName);
        Result->SetBoolField(TEXT("value"), bEnabled);
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown sim_stage action: %s"), *Action));
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ProcessStatelessModuleOperation(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Op)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
#if TAAGENT_WITH_NIAGARA_STATELESS
    FString EmitterName = Op->GetStringField(TEXT("emitter"));
    FString ModuleName = Op->GetStringField(TEXT("name"));
    FString Action = Op->GetStringField(TEXT("action"));
    
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
        return Result;
    }
    
    // Verify emitter is in Stateless mode
    if (Handle->GetEmitterMode() != ENiagaraEmitterMode::Stateless)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Emitter is not in Stateless mode"));
        return Result;
    }
    
    UNiagaraStatelessEmitter* StatelessEmitter = Handle->GetStatelessEmitter();
    if (!StatelessEmitter)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get StatelessEmitter"));
        return Result;
    }
    
    // Find module by name
    UNiagaraStatelessModule* FoundModule = nullptr;
    for (UNiagaraStatelessModule* Module : StatelessEmitter->GetModules())
    {
        if (Module && Module->GetName() == ModuleName)
        {
            FoundModule = Module;
            break;
        }
    }
    
    if (!FoundModule)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Stateless module not found: %s"), *ModuleName));
        return Result;
    }
    
    if (Action == TEXT("set_enabled"))
    {
        bool bEnabled = Op->GetBoolField(TEXT("value"));
        FoundModule->SetIsModuleEnabled(bEnabled);
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_enabled"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("module_name"), ModuleName);
        Result->SetBoolField(TEXT("value"), bEnabled);
    }
    else if (Action == TEXT("set_property"))
    {
        // Set module property via reflection
        FString PropertyName = Op->GetStringField(TEXT("property"));
        const TSharedPtr<FJsonValue>* ValuePtr = Op->Values.Find(TEXT("value"));
        
        if (!ValuePtr)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Missing 'value' parameter"));
            return Result;
        }
        
        // Find property by name
        FProperty* Property = FoundModule->GetClass()->FindPropertyByName(*PropertyName);
        if (!Property)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Property not found: %s"), *PropertyName));
            return Result;
        }
        
        // Set value based on property type
        void* Container = FoundModule;
        
        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
        {
            FloatProp->SetValue_InContainer(Container, (float)(*ValuePtr)->AsNumber());
        }
        else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
        {
            IntProp->SetValue_InContainer(Container, (int32)(*ValuePtr)->AsNumber());
        }
        else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
        {
            bool BoolValue = (*ValuePtr)->AsBool();
            BoolProp->SetValue_InContainer(Container, &BoolValue);
        }
        else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            const TArray<TSharedPtr<FJsonValue>>* VecArray;
            if ((*ValuePtr)->TryGetArray(VecArray))
            {
                if (StructProp->Struct == TBaseStructure<FVector>::Get() && VecArray->Num() >= 3)
                {
                    FVector Value(
                        (float)(*VecArray)[0]->AsNumber(),
                        (float)(*VecArray)[1]->AsNumber(),
                        (float)(*VecArray)[2]->AsNumber()
                    );
                    StructProp->SetValue_InContainer(Container, &Value);
                }
                else if (StructProp->Struct == TBaseStructure<FVector4>::Get() && VecArray->Num() >= 4)
                {
                    FVector4 Value(
                        (float)(*VecArray)[0]->AsNumber(),
                        (float)(*VecArray)[1]->AsNumber(),
                        (float)(*VecArray)[2]->AsNumber(),
                        (float)(*VecArray)[3]->AsNumber()
                    );
                    StructProp->SetValue_InContainer(Container, &Value);
                }
                else if (StructProp->Struct == TBaseStructure<FLinearColor>::Get() && VecArray->Num() >= 4)
                {
                    FLinearColor Value(
                        (float)(*VecArray)[0]->AsNumber(),
                        (float)(*VecArray)[1]->AsNumber(),
                        (float)(*VecArray)[2]->AsNumber(),
                        (float)(*VecArray)[3]->AsNumber()
                    );
                    StructProp->SetValue_InContainer(Container, &Value);
                }
                else
                {
                    Result->SetBoolField(TEXT("success"), false);
                    Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported struct type for property: %s"), *PropertyName));
                    return Result;
                }
            }
            else
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), TEXT("Struct property requires array value"));
                return Result;
            }
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName()));
            return Result;
        }
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("set_property"));
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("module_name"), ModuleName);
        Result->SetStringField(TEXT("property"), PropertyName);
    }
    else
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown stateless_module action: %s"), *Action));
    }
#else
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Stateless Niagara requires UE 5.7 or later"));
#endif
    
    return Result;
}

// ============================================================================
// Utility Functions
// ============================================================================

TArray<FString> FEpicUnrealMCPNiagaraCommands::ParseIncludeSections(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> IncludeSections;
    
    const TArray<TSharedPtr<FJsonValue>>* IncludeArray;
    if (Params->TryGetArrayField(TEXT("include"), IncludeArray))
    {
        for (const auto& Val : *IncludeArray)
        {
            IncludeSections.Add(Val->AsString().ToLower());
        }
    }
    else
    {
        IncludeSections.Add(TEXT("all"));
    }
    
    return IncludeSections;
}

bool FEpicUnrealMCPNiagaraCommands::ShouldInclude(const TArray<FString>& IncludeSections, const FString& Section)
{
    if (IncludeSections.Contains(TEXT("all")))
    {
        return true;
    }
    return IncludeSections.Contains(Section.ToLower());
}

// ============================================================================
// Stateless Conversion Implementation
// ============================================================================

/**
 * Standard Parameter Name -> Stateless Module mapping
 * 
 * Standard Niagara parameters follow naming conventions:
 * - "Particles.Lifetime" -> Stateless Lifetime module
 * - "Engine.ExecutionState" -> not convertible
 * 
 * This mapping identifies convertible parameter patterns.
 */
struct FStatelessModuleMapping
{
    FString ParameterPrefix;        // e.g., "Particles.Lifetime"
    FString StatelessModuleClass;   // e.g., "NiagaraStatelessLifetimeModule"
    FString DisplayName;            // e.g., "Lifetime"
    int32 Priority;                 // Higher = more specific match
};

// Static mapping table for Standard -> Stateless conversion
static const TArray<FStatelessModuleMapping>& GetStatelessModuleMappings()
{
    static TArray<FStatelessModuleMapping> Mappings;
    if (Mappings.Num() == 0)
    {
        // Particle attribute modules
        Mappings.Add({"Particles.Lifetime", "NiagaraStatelessLifetimeModule", "Lifetime", 100});
        Mappings.Add({"Particles.SpriteSize", "NiagaraStatelessSizeModule", "Size", 100});
        Mappings.Add({"Particles.SpriteRotation", "NiagaraStatelessRotationModule", "Rotation", 100});
        Mappings.Add({"Particles.Color", "NiagaraStatelessColorModule", "Color", 100});
        Mappings.Add({"Particles.Position", "NiagaraStatelessPositionModule", "Position", 90});
        Mappings.Add({"Particles.Velocity", "NiagaraStatelessVelocityModule", "Velocity", 100});
        Mappings.Add({"Particles.Scale", "NiagaraStatelessScaleModule", "Scale", 100});
        Mappings.Add({"Particles.MeshRotation", "NiagaraStatelessMeshRotationModule", "MeshRotation", 100});
        Mappings.Add({"Particles.SubUV", "NiagaraStatelessSubUVModule", "SubUV", 100});
        
        // Spawn modules
        Mappings.Add({"SpawnRate", "NiagaraStatelessSpawnRateModule", "SpawnRate", 100});
        Mappings.Add({"SpawnPerUnit", "NiagaraStatelessSpawnPerUnitModule", "SpawnPerUnit", 100});
        
        // Force/Physics modules
        Mappings.Add({"CurlNoise", "NiagaraStatelessCurlNoiseModule", "CurlNoise", 100});
        Mappings.Add({"Drag", "NiagaraStatelessDragModule", "Drag", 100});
        Mappings.Add({"Collision", "NiagaraStatelessCollisionModule", "Collision", 100});
        Mappings.Add({"Attractor", "NiagaraStatelessAttractorModule", "Attractor", 100});
    }
    return Mappings;
}

/**
 * Check if a parameter is a system/engine parameter (not convertible)
 */
static bool IsSystemParameter(const FString& ParamName)
{
    return ParamName.StartsWith("Engine.") ||
           ParamName.StartsWith("System.") ||
           ParamName.StartsWith("Emitter.") ||
           ParamName.Contains("ExecutionState") ||
           ParamName.Contains("LoopCount") ||
           ParamName.Contains("Duration");
}

/**
 * Find matching Stateless module for a parameter name
 */
static const FStatelessModuleMapping* FindStatelessMapping(const FString& ParamName)
{
    const TArray<FStatelessModuleMapping>& Mappings = GetStatelessModuleMappings();
    const FStatelessModuleMapping* BestMatch = nullptr;
    
    for (const FStatelessModuleMapping& Mapping : Mappings)
    {
        if (ParamName.StartsWith(Mapping.ParameterPrefix) || ParamName == Mapping.ParameterPrefix)
        {
            if (!BestMatch || Mapping.Priority > BestMatch->Priority)
            {
                BestMatch = &Mapping;
            }
        }
    }
    
    return BestMatch;
}

/**
 * Standard Module Node Class -> Stateless Module mapping
 * 
 * Map Niagara Graph node class names to Stateless module equivalents.
 * E.g., "NiagaraNodeModule_Lifetime" -> "NiagaraStatelessLifetimeModule"
 */
struct FStandardModuleMapping
{
    FString NodeClassPattern;       // Pattern to match in node class name
    FString StatelessModuleClass;   // Corresponding Stateless module class
    FString DisplayName;            // Human-readable name
    bool bIsSupported;              // Whether conversion is supported
};

static const TArray<FStandardModuleMapping>& GetStandardModuleMappings()
{
    static TArray<FStandardModuleMapping> Mappings;
    if (Mappings.Num() == 0)
    {
        // Particle attribute modules - supported
        Mappings.Add({"Lifetime", "NiagaraStatelessLifetimeModule", "Lifetime", true});
        Mappings.Add({"Color", "NiagaraStatelessColorModule", "Color", true});
        Mappings.Add({"SpriteSize", "NiagaraStatelessSizeModule", "Size", true});
        Mappings.Add({"Size", "NiagaraStatelessSizeModule", "Size", true});
        Mappings.Add({"Scale", "NiagaraStatelessScaleModule", "Scale", true});
        Mappings.Add({"SpriteRotation", "NiagaraStatelessRotationModule", "Rotation", true});
        Mappings.Add({"Rotation", "NiagaraStatelessRotationModule", "Rotation", true});
        Mappings.Add({"Velocity", "NiagaraStatelessVelocityModule", "Velocity", true});
        Mappings.Add({"AddVelocity", "NiagaraStatelessVelocityModule", "Velocity", true});
        Mappings.Add({"Position", "NiagaraStatelessPositionModule", "Position", true});
        Mappings.Add({"AddPosition", "NiagaraStatelessPositionModule", "Position", true});
        Mappings.Add({"MeshRotation", "NiagaraStatelessMeshRotationModule", "MeshRotation", true});
        Mappings.Add({"SubUV", "NiagaraStatelessSubUVModule", "SubUV", true});
        
        // Spawn modules - supported
        Mappings.Add({"SpawnRate", "NiagaraStatelessSpawnRateModule", "SpawnRate", true});
        Mappings.Add({"SpawnPerUnit", "NiagaraStatelessSpawnPerUnitModule", "SpawnPerUnit", true});
        
        // Force/Physics modules - supported
        Mappings.Add({"CurlNoise", "NiagaraStatelessCurlNoiseModule", "CurlNoise", true});
        Mappings.Add({"Drag", "NiagaraStatelessDragModule", "Drag", true});
        Mappings.Add({"Collision", "NiagaraStatelessCollisionModule", "Collision", true});
        Mappings.Add({"Attractor", "NiagaraStatelessAttractorModule", "Attractor", true});
        
        // Common modules that need special handling
        Mappings.Add({"Initialize", "", "Initialize", false});  // Usually contains multiple attributes
        Mappings.Add({"Mesh", "NiagaraStatelessMeshModule", "Mesh", true});
        Mappings.Add({"Material", "NiagaraStatelessMaterialModule", "Material", true});
        
        // Unsupported modules
        Mappings.Add({"Event", "", "Event", false});  // Events not supported
        Mappings.Add({"Ribbon", "", "Ribbon", false});  // Ribbon renderer not supported
        Mappings.Add({"Light", "NiagaraStatelessLightModule", "Light", true});
    }
    return Mappings;
}

/**
 * Find matching Stateless module for a Graph node class
 */
static const FStandardModuleMapping* FindStandardModuleMapping(const FString& NodeClass)
{
    const TArray<FStandardModuleMapping>& Mappings = GetStandardModuleMappings();
    const FStandardModuleMapping* BestMatch = nullptr;
    
    for (const FStandardModuleMapping& Mapping : Mappings)
    {
        if (NodeClass.Contains(Mapping.NodeClassPattern))
        {
            // Prefer longer/more specific matches
            if (!BestMatch || Mapping.NodeClassPattern.Len() > BestMatch->NodeClassPattern.Len())
            {
                BestMatch = &Mapping;
            }
        }
    }
    
    return BestMatch;
}

// ============================================================================
// Stateless Helper Functions (Internal)
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::AnalyzeStatelessCompatibility(FNiagaraEmitterHandle& Handle)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    FString EmitterName = Handle.GetName().ToString();
    
    // Check if already Stateless
    if (Handle.GetEmitterMode() == ENiagaraEmitterMode::Stateless)
    {
        Result->SetBoolField(TEXT("is_stateless"), true);
        Result->SetStringField(TEXT("message"), TEXT("Emitter is already in Stateless mode"));
        return Result;
    }
    
    FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
    if (!EmitterData)
    {
        Result->SetBoolField(TEXT("error"), true);
        Result->SetStringField(TEXT("message"), TEXT("Failed to get emitter data"));
        return Result;
    }
    
    // Analyze compatibility
    TArray<TSharedPtr<FJsonValue>> ConvertibleParams;
    TArray<TSharedPtr<FJsonValue>> UnsupportedParams;
    TArray<TSharedPtr<FJsonValue>> Blockers;
    TMap<FString, bool> FoundModules;  // Track unique modules needed
    
    // Check for blockers (features that prevent conversion)
    
    // 1. Event handlers
    const TArray<FNiagaraEventScriptProperties>& EventHandlers = EmitterData->GetEventHandlers();
    if (EventHandlers.Num() > 0)
    {
        TSharedPtr<FJsonObject> Blocker = MakeShareable(new FJsonObject);
        Blocker->SetStringField(TEXT("type"), TEXT("event_handlers"));
        Blocker->SetStringField(TEXT("reason"), TEXT("Stateless mode does not support event handlers"));
        Blocker->SetNumberField(TEXT("count"), EventHandlers.Num());
        Blockers.Add(MakeShareable(new FJsonValueObject(Blocker)));
    }
    
    // 2. Simulation stages (beyond basic spawn/update)
    TArray<UNiagaraSimulationStageBase*> SimStages = EmitterData->GetSimulationStages();
    if (SimStages.Num() > 0)
    {
        TSharedPtr<FJsonObject> Blocker = MakeShareable(new FJsonObject);
        Blocker->SetStringField(TEXT("type"), TEXT("simulation_stages"));
        Blocker->SetStringField(TEXT("reason"), TEXT("Custom simulation stages may not be supported in Stateless mode"));
        Blocker->SetNumberField(TEXT("count"), SimStages.Num());
        Blockers.Add(MakeShareable(new FJsonValueObject(Blocker)));
    }
    
    // 3. Analyze Graph modules
    TArray<TSharedPtr<FJsonValue>> ConvertibleModules;
    TArray<TSharedPtr<FJsonValue>> UnsupportedModules;
    TSet<FString> FoundModuleNames;
    
    auto AnalyzeGraphModules = [&](UNiagaraScript* Script, const FString& ScriptType)
    {
        if (!Script) return;
        
        UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
        UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
        if (!Graph) return;
        
        for (UEdGraphNode* NodeBase : Graph->Nodes)
        {
            UNiagaraNode* Node = Cast<UNiagaraNode>(NodeBase);
            if (!Node) continue;
            
            FString NodeClass = Node->GetClass()->GetName();
            
            // Skip output/input/system nodes
            if (NodeClass.Contains(TEXT("Output")) || 
                NodeClass.Contains(TEXT("Input")) ||
                NodeClass.Contains(TEXT("System")) ||
                NodeClass.Contains(TEXT("Emitter")))
            {
                continue;
            }
            
            // Check if it's a module node
            if (!NodeClass.Contains(TEXT("Module"))) continue;
            
            FString ModuleName = Node->GetName();
            if (FoundModuleNames.Contains(ModuleName)) continue;
            FoundModuleNames.Add(ModuleName);
            
            // Find mapping
            const FStandardModuleMapping* Mapping = FindStandardModuleMapping(NodeClass);
            
            TSharedPtr<FJsonObject> ModuleInfo = MakeShareable(new FJsonObject);
            ModuleInfo->SetStringField(TEXT("name"), ModuleName);
            ModuleInfo->SetStringField(TEXT("node_class"), NodeClass);
            ModuleInfo->SetStringField(TEXT("script"), ScriptType);
            
            if (Mapping)
            {
                ModuleInfo->SetStringField(TEXT("stateless_module"), Mapping->DisplayName);
                ModuleInfo->SetBoolField(TEXT("supported"), Mapping->bIsSupported);
                if (Mapping->bIsSupported)
                {
                    ConvertibleModules.Add(MakeShareable(new FJsonValueObject(ModuleInfo)));
                    FoundModules.FindOrAdd(Mapping->DisplayName) = true;
                }
                else
                {
                    ModuleInfo->SetStringField(TEXT("reason"), TEXT("Module not supported in Stateless mode"));
                    UnsupportedModules.Add(MakeShareable(new FJsonValueObject(ModuleInfo)));
                }
            }
            else
            {
                ModuleInfo->SetBoolField(TEXT("supported"), false);
                ModuleInfo->SetStringField(TEXT("reason"), TEXT("No matching Stateless module found"));
                UnsupportedModules.Add(MakeShareable(new FJsonValueObject(ModuleInfo)));
            }
        }
    };
    
    // Analyze spawn and update script graphs
    if (EmitterData->SpawnScriptProps.Script)
    {
        AnalyzeGraphModules(EmitterData->SpawnScriptProps.Script, TEXT("spawn"));
    }
    if (EmitterData->UpdateScriptProps.Script)
    {
        AnalyzeGraphModules(EmitterData->UpdateScriptProps.Script, TEXT("update"));
    }
    
    // 4. Also analyze script parameters (for value migration)
    auto AnalyzeScriptParams = [&](UNiagaraScript* Script, const FString& ScriptType)
    {
        if (!Script) return;
        
        const FNiagaraParameterStore& ParamStore = Script->RapidIterationParameters;
        TArrayView<const FNiagaraVariableWithOffset> ParamVariables = ParamStore.ReadParameterVariables();
        
        for (const FNiagaraVariableWithOffset& ParamWithOffset : ParamVariables)
        {
            FString ParamName = ParamWithOffset.GetName().ToString();
            
            // Skip system parameters
            if (IsSystemParameter(ParamName))
            {
                continue;
            }
            
            // Find matching Stateless module
            const FStatelessModuleMapping* Mapping = FindStatelessMapping(ParamName);
            
            TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
            ParamInfo->SetStringField(TEXT("name"), ParamName);
            ParamInfo->SetStringField(TEXT("script"), ScriptType);
            ParamInfo->SetStringField(TEXT("type"), ParamWithOffset.GetType().GetName());
            
            if (Mapping)
            {
                ParamInfo->SetStringField(TEXT("stateless_module"), Mapping->DisplayName);
                ParamInfo->SetBoolField(TEXT("convertible"), true);
                ConvertibleParams.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
            }
            else
            {
                ParamInfo->SetBoolField(TEXT("convertible"), false);
                ParamInfo->SetStringField(TEXT("reason"), TEXT("No matching Stateless module"));
                UnsupportedParams.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
            }
        }
    };
    
    if (EmitterData->SpawnScriptProps.Script)
    {
        AnalyzeScriptParams(EmitterData->SpawnScriptProps.Script, TEXT("spawn"));
    }
    if (EmitterData->UpdateScriptProps.Script)
    {
        AnalyzeScriptParams(EmitterData->UpdateScriptProps.Script, TEXT("update"));
    }
    
    // Build result
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("emitter_name"), EmitterName);
    Result->SetStringField(TEXT("current_mode"), TEXT("Standard"));
    
    // Compatibility assessment
    bool bHasBlockers = Blockers.Num() > 0;
    bool bHasUnsupportedModules = UnsupportedModules.Num() > 0;
    bool bCanConvert = !bHasBlockers && ConvertibleModules.Num() > 0 && !bHasUnsupportedModules;
    
    Result->SetBoolField(TEXT("can_convert"), bCanConvert);
    Result->SetBoolField(TEXT("has_blockers"), bHasBlockers);
    
    // Convertible modules needed
    TArray<TSharedPtr<FJsonValue>> RequiredModules;
    for (auto& Pair : FoundModules)
    {
        RequiredModules.Add(MakeShareable(new FJsonValueString(Pair.Key)));
    }
    Result->SetArrayField(TEXT("required_stateless_modules"), RequiredModules);
    
    // Module analysis results
    Result->SetArrayField(TEXT("convertible_modules"), ConvertibleModules);
    Result->SetNumberField(TEXT("convertible_module_count"), ConvertibleModules.Num());
    Result->SetArrayField(TEXT("unsupported_modules"), UnsupportedModules);
    Result->SetNumberField(TEXT("unsupported_module_count"), UnsupportedModules.Num());
    
    // Parameter analysis results
    Result->SetArrayField(TEXT("convertible_parameters"), ConvertibleParams);
    Result->SetNumberField(TEXT("convertible_parameter_count"), ConvertibleParams.Num());
    Result->SetArrayField(TEXT("unsupported_parameters"), UnsupportedParams);
    Result->SetNumberField(TEXT("unsupported_parameter_count"), UnsupportedParams.Num());
    
    Result->SetArrayField(TEXT("blockers"), Blockers);
    
    // Summary message
    FString Message;
    if (bCanConvert)
    {
        Message = FString::Printf(TEXT("Convertible: %d modules and %d parameters can be migrated to Stateless"), 
            ConvertibleModules.Num(), ConvertibleParams.Num());
    }
    else if (bHasBlockers)
    {
        Message = FString::Printf(TEXT("Not convertible: %d blocker(s) found"), Blockers.Num());
    }
    else if (bHasUnsupportedModules)
    {
        Message = FString::Printf(TEXT("Not convertible: %d unsupported module(s)"), UnsupportedModules.Num());
    }
    else
    {
        Message = TEXT("Not convertible: No convertible modules found");
    }
    Result->SetStringField(TEXT("message"), Message);
    
    return Result;
}

// ============================================================================
// Graph Form Tools - Read/Update Niagara Script Graphs
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraGraph(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Method 1: Direct script path for standalone assets
    FString ScriptPath;
    bool bUseStandaloneScript = Params->TryGetStringField(TEXT("script_path"), ScriptPath);
    
    // Method 2: Locate script within System's Emitter
    FString AssetPath;
    FString EmitterName;
    FString ScriptType = TEXT("spawn");
    
    if (!bUseStandaloneScript)
    {
        if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Must provide either (asset_path + emitter) or script_path"));
            return Result;
        }
        
        if (!Params->TryGetStringField(TEXT("emitter"), EmitterName))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Must provide either (asset_path + emitter) or script_path"));
            return Result;
        }
        
        Params->TryGetStringField(TEXT("script"), ScriptType);
    }
    
    // Optional: filter by module name
    FString ModuleName;
    Params->TryGetStringField(TEXT("module"), ModuleName);

    int32 MaxDepth = 1;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
    MaxDepth = FMath::Clamp(MaxDepth, 0, 8);

    bool bIncludeAllProperties = false;
    Params->TryGetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);
    
    // Get the script
    UNiagaraScript* Script = nullptr;
    FString SourceLocation;  // For result metadata
    
    if (bUseStandaloneScript)
    {
        // Load standalone script asset
        Script = LoadObject<UNiagaraScript>(nullptr, *ScriptPath);
        if (!Script)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara script: %s"), *ScriptPath));
            return Result;
        }
        SourceLocation = ScriptPath;
    }
    else
    {
        // Load script from System's Emitter
        UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
        if (!NiagaraSystem)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
            return Result;
        }
        
        FNiagaraEmitterHandle* Handle = FindEmitterHandle(NiagaraSystem, EmitterName);
        if (!Handle)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
            return Result;
        }
        
        FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
        if (!EmitterData)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
            return Result;
        }
        
        if (ScriptType.ToLower() == TEXT("spawn"))
        {
            Script = EmitterData->SpawnScriptProps.Script;
        }
        else if (ScriptType.ToLower() == TEXT("update"))
        {
            Script = EmitterData->UpdateScriptProps.Script;
        }
        
        if (!Script)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Script not found: %s"), *ScriptType));
            return Result;
        }
        
        SourceLocation = FString::Printf(TEXT("%s:%s:%s"), *AssetPath, *EmitterName, *ScriptType);
    }
    
    // Extract graph using helper function
    TSharedPtr<FJsonObject> GraphResult = ExtractGraphFromScript(Script, ModuleName, MaxDepth, bIncludeAllProperties);
    GraphResult->SetStringField(TEXT("source_location"), SourceLocation);
    GraphResult->SetNumberField(TEXT("max_depth"), MaxDepth);
    GraphResult->SetBoolField(TEXT("include_all_properties"), bIncludeAllProperties);
    return GraphResult;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::ExtractGraphFromScript(UNiagaraScript* Script, const FString& ModuleFilter,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    if (!Script)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Script is null"));
        return Result;
    }
    
    // Get script metadata
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("script_name"), Script->GetName());
    Result->SetStringField(TEXT("script_path"), Script->GetPathName());
    Result->SetObjectField(TEXT("script_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Script, MaxDepth, bIncludeAllProperties));
    
    // Get script usage
    FString UsageStr = TEXT("Unknown");
    switch (Script->GetUsage())
    {
        case ENiagaraScriptUsage::Function: UsageStr = TEXT("Function"); break;
        case ENiagaraScriptUsage::Module: UsageStr = TEXT("Module"); break;
        case ENiagaraScriptUsage::DynamicInput: UsageStr = TEXT("DynamicInput"); break;
        case ENiagaraScriptUsage::ParticleSpawnScript: UsageStr = TEXT("ParticleSpawn"); break;
        case ENiagaraScriptUsage::ParticleUpdateScript: UsageStr = TEXT("ParticleUpdate"); break;
        case ENiagaraScriptUsage::EmitterSpawnScript: UsageStr = TEXT("EmitterSpawn"); break;
        case ENiagaraScriptUsage::EmitterUpdateScript: UsageStr = TEXT("EmitterUpdate"); break;
        case ENiagaraScriptUsage::SystemSpawnScript: UsageStr = TEXT("SystemSpawn"); break;
        case ENiagaraScriptUsage::SystemUpdateScript: UsageStr = TEXT("SystemUpdate"); break;
        case ENiagaraScriptUsage::ParticleEventScript: UsageStr = TEXT("ParticleEvent"); break;
        default: break;
    }
    Result->SetStringField(TEXT("usage"), UsageStr);
    
    // Get description
    Result->SetStringField(TEXT("description"), Script->GetDescription(FGuid()).ToString());
    
    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    if (!Graph)
    {
        Result->SetBoolField(TEXT("has_graph"), false);
        return Result;
    }
    
    Result->SetBoolField(TEXT("has_graph"), true);
    Result->SetObjectField(TEXT("graph_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Graph, FMath::Max(MaxDepth - 1, 0), bIncludeAllProperties));
    
    // Build node ID map
    TMap<UEdGraphNode*, FString> NodeIdMap;
    int32 NodeIndex = 0;
    for (UEdGraphNode* NodeBase : Graph->Nodes)
    {
        if (NodeBase)
        {
            FString NodeId = FString::Printf(TEXT("Node_%d"), NodeIndex++);
            NodeIdMap.Add(NodeBase, NodeId);
        }
    }
    
    // Build nodes array
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
    
    for (UEdGraphNode* NodeBase : Graph->Nodes)
    {
        UNiagaraNode* Node = Cast<UNiagaraNode>(NodeBase);
        if (!Node) continue;
        
        // Optional: filter by module name
        if (!ModuleFilter.IsEmpty())
        {
            FString NodeName = Node->GetName();
            if (!NodeName.Contains(ModuleFilter))
            {
                continue;
            }
        }
        
        // Get node details
        TSharedPtr<FJsonObject> NodeJson = GetNodeDetails(Node, MaxDepth, bIncludeAllProperties);
        FString NodeId = NodeIdMap[Node];
        NodeJson->SetStringField(TEXT("node_id"), NodeId);
        NodesArray.Add(MakeShareable(new FJsonValueObject(NodeJson)));
        
        // Get connections from this node
        TArray<TSharedPtr<FJsonValue>> NodeConnections = GetNodeConnections(Node, NodeIdMap);
        for (const auto& Conn : NodeConnections)
        {
            ConnectionsArray.Add(Conn);
        }
    }
    
    Result->SetArrayField(TEXT("nodes"), NodesArray);
    Result->SetNumberField(TEXT("node_count"), NodesArray.Num());
    Result->SetArrayField(TEXT("connections"), ConnectionsArray);
    Result->SetNumberField(TEXT("connection_count"), ConnectionsArray.Num());
    
    // Get rapid iteration parameters
    TArray<TSharedPtr<FJsonValue>> ParametersArray;
    const FNiagaraParameterStore& ParamStore = Script->RapidIterationParameters;
    TArrayView<const FNiagaraVariableWithOffset> ParamVariables = ParamStore.ReadParameterVariables();
    
    for (const FNiagaraVariableWithOffset& ParamWithOffset : ParamVariables)
    {
        TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
        ParamJson->SetStringField(TEXT("name"), ParamWithOffset.GetName().ToString());
        ParamJson->SetStringField(TEXT("type"), ParamWithOffset.GetType().GetName());
        ParametersArray.Add(MakeShareable(new FJsonValueObject(ParamJson)));
    }
    
    Result->SetArrayField(TEXT("parameters"), ParametersArray);
    Result->SetNumberField(TEXT("parameter_count"), ParametersArray.Num());
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::GetNodeDetails(UNiagaraNode* Node,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);
    
    if (!Node)
    {
        return NodeJson;
    }
    
    // Basic info
    FString NodeClass = Node->GetClass()->GetName();
    NodeClass.RemoveFromStart(TEXT("NiagaraNode"));
    NodeJson->SetStringField(TEXT("type"), NodeClass);
    NodeJson->SetStringField(TEXT("name"), Node->GetName());
    NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeJson->SetObjectField(TEXT("node_object"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(Node, MaxDepth, bIncludeAllProperties));
    
    // Position
    NodeJson->SetNumberField(TEXT("pos_x"), Node->NodePosX);
    NodeJson->SetNumberField(TEXT("pos_y"), Node->NodePosY);
    
    // Node-specific info
    FString NodeType = Node->GetClass()->GetName();
    
    // Input node - parameter input
    if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("input"));
        NodeJson->SetStringField(TEXT("input_name"), InputNode->Input.GetName().ToString());
        NodeJson->SetStringField(TEXT("input_type"), InputNode->Input.GetType().GetName());
        
        // Note: Default value extraction removed due to UE 5.7 API changes
        // FNiagaraVariable::GetVars() no longer available
    }
    // Output node
    else if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("output"));
    }
    // CustomHlsl node (must check before FunctionCall since CustomHlsl inherits from it)
    else if (UNiagaraNodeCustomHlsl* CustomHlslNode = Cast<UNiagaraNodeCustomHlsl>(Node))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("custom_hlsl"));
        
        // Get HLSL code using reflection (CustomHlsl is private in MinimalAPI class)
        if (FStrProperty* CustomHlslProperty = CastField<FStrProperty>(CustomHlslNode->GetClass()->FindPropertyByName(TEXT("CustomHlsl"))))
        {
            FString CustomHlslValue = CustomHlslProperty->GetPropertyValue_InContainer(CustomHlslNode);
            NodeJson->SetStringField(TEXT("hlsl_code"), CustomHlslValue);
        }
        
        // Get script usage name using enum reflection
        UEnum* ScriptUsageEnum = StaticEnum<ENiagaraScriptUsage>();
        if (ScriptUsageEnum)
        {
            FString ScriptUsageName = ScriptUsageEnum->GetNameStringByValue((int64)CustomHlslNode->ScriptUsage);
            NodeJson->SetStringField(TEXT("script_usage"), ScriptUsageName);
        }
    }
    // Function call node
    else if (UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("function_call"));
        
        // Get function name
        if (FuncNode->FunctionScript)
        {
            NodeJson->SetStringField(TEXT("function_name"), FuncNode->FunctionScript->GetName());
            NodeJson->SetStringField(TEXT("function_path"), FuncNode->FunctionScript->GetPathName());
            NodeJson->SetObjectField(TEXT("function_script"), FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(FuncNode->FunctionScript, FMath::Max(MaxDepth - 1, 0), bIncludeAllProperties));
        }
        else
        {
            NodeJson->SetStringField(TEXT("function_name"), FuncNode->GetName());
        }
    }
    // Operator node
    else if (UNiagaraNodeOp* OpNode = Cast<UNiagaraNodeOp>(Node))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("operator"));
        NodeJson->SetStringField(TEXT("operator"), OpNode->OpName.ToString());
    }
    // Generic module node
    else if (NodeType.Contains(TEXT("Module")))
    {
        NodeJson->SetStringField(TEXT("usage"), TEXT("module"));
    }
    
    // Get input pins info
    TArray<TSharedPtr<FJsonValue>> InputPinsArray;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && !Pin->bHidden)
        {
            TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);
            PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinJson->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinJson->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
            InputPinsArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
        }
    }
    if (InputPinsArray.Num() > 0)
    {
        NodeJson->SetArrayField(TEXT("input_pins"), InputPinsArray);
    }
    
    // Get output pins info
    TArray<TSharedPtr<FJsonValue>> OutputPinsArray;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && !Pin->bHidden)
        {
            TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);
            PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinJson->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinJson->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
            OutputPinsArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
        }
    }
    if (OutputPinsArray.Num() > 0)
    {
        NodeJson->SetArrayField(TEXT("output_pins"), OutputPinsArray);
    }
    
    return NodeJson;
}

TArray<TSharedPtr<FJsonValue>> FEpicUnrealMCPNiagaraCommands::GetNodeConnections(UNiagaraNode* Node, const TMap<UEdGraphNode*, FString>& NodeIdMap)
{
    TArray<TSharedPtr<FJsonValue>> Connections;
    
    if (!Node)
    {
        return Connections;
    }
    
    FString SourceNodeId = NodeIdMap.FindRef(Node);
    if (SourceNodeId.IsEmpty())
    {
        return Connections;
    }
    
    // Get output pins and their connections
    for (UEdGraphPin* OutputPin : Node->Pins)
    {
        if (!OutputPin || OutputPin->Direction != EGPD_Output || OutputPin->bHidden) continue;
        
        for (UEdGraphPin* ConnectedPin : OutputPin->LinkedTo)
        {
            if (!ConnectedPin) continue;
            
            UEdGraphNode* TargetNode = ConnectedPin->GetOwningNode();
            if (!TargetNode) continue;
            
            const FString* TargetNodeId = NodeIdMap.Find(TargetNode);
            if (!TargetNodeId) continue;
            
            TSharedPtr<FJsonObject> ConnJson = MakeShareable(new FJsonObject);
            ConnJson->SetStringField(TEXT("source_node"), SourceNodeId);
            ConnJson->SetStringField(TEXT("source_pin"), OutputPin->PinName.ToString());
            ConnJson->SetStringField(TEXT("target_node"), *TargetNodeId);
            ConnJson->SetStringField(TEXT("target_pin"), ConnectedPin->PinName.ToString());
            
            Connections.Add(MakeShareable(new FJsonValueObject(ConnJson)));
        }
    }
    
    return Connections;
}

// ============================================================================
// Graph Update Operations
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleUpdateNiagaraGraph(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

    // Method 1: Direct script path for standalone assets
    FString ScriptPath;
    bool bUseStandaloneScript = Params->TryGetStringField(TEXT("script_path"), ScriptPath);

    // Method 2: Locate script within System's Emitter
    FString AssetPath;
    FString EmitterName;
    FString ScriptType = TEXT("spawn");

    if (!bUseStandaloneScript)
    {
        if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Must provide either (asset_path + emitter) or script_path"));
            return Result;
        }

        if (!Params->TryGetStringField(TEXT("emitter"), EmitterName))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Must provide either (asset_path + emitter) or script_path"));
            return Result;
        }

        Params->TryGetStringField(TEXT("script"), ScriptType);
    }

    const TArray<TSharedPtr<FJsonValue>>* OperationsArray;
    if (!Params->TryGetArrayField(TEXT("operations"), OperationsArray))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing required parameter: operations"));
        return Result;
    }

    // Get the script
    UNiagaraScript* Script = nullptr;
    UNiagaraSystem* NiagaraSystem = nullptr;
    FVersionedNiagaraEmitterData* EmitterDataForOps = nullptr;
    FGuid EmitterHandleId;  // Store emitter ID for module operations
    
    if (bUseStandaloneScript)
    {
        Script = LoadObject<UNiagaraScript>(nullptr, *ScriptPath);
        if (!Script)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara script: %s"), *ScriptPath));
            return Result;
        }
        Result->SetStringField(TEXT("script_path"), ScriptPath);
    }
    else
    {
        NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
        if (!NiagaraSystem)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara system: %s"), *AssetPath));
            return Result;
        }

        FNiagaraEmitterHandle* Handle = FindEmitterHandle(NiagaraSystem, EmitterName);
        if (!Handle)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
            return Result;
        }
        
        EmitterHandleId = Handle->GetId();  // Save for later module operations

        EmitterDataForOps = Handle->GetEmitterData();
        if (!EmitterDataForOps)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data"));
            return Result;
        }

        if (ScriptType.ToLower() == TEXT("spawn"))
        {
            Script = EmitterDataForOps->SpawnScriptProps.Script;
        }
        else if (ScriptType.ToLower() == TEXT("update"))
        {
            Script = EmitterDataForOps->UpdateScriptProps.Script;
        }

        if (!Script)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Script not found: %s"), *ScriptType));
            return Result;
        }

        Result->SetStringField(TEXT("asset_path"), AssetPath);
        Result->SetStringField(TEXT("emitter_name"), EmitterName);
        Result->SetStringField(TEXT("script_type"), ScriptType);
    }

    // Get graph source
    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
    if (!Source)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get script source"));
        return Result;
    }

    UNiagaraGraph* Graph = Source->NodeGraph;
    if (!Graph)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Failed to get Niagara graph"));
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (const TSharedPtr<FJsonValue>& OpValue : *OperationsArray)
    {
        TSharedPtr<FJsonObject> Op = OpValue->AsObject();
        if (!Op.IsValid()) continue;

        FString Action = Op->GetStringField(TEXT("action"));
        TSharedPtr<FJsonObject> OpResult = MakeShareable(new FJsonObject);

        if (Action == TEXT("add_module"))
        {
            FString ModulePath;
            if (!Op->TryGetStringField(TEXT("module_path"), ModulePath))
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Missing module_path"));
                FailCount++;
            }
            else
            {
                UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
                if (!ModuleScript)
                {
                    OpResult->SetBoolField(TEXT("success"), false);
                    OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load module: %s"), *ModulePath));
                    FailCount++;
                }
                else
                {
                    ENiagaraScriptUsage ScriptUsage = Script->GetUsage();
                    UNiagaraNodeOutput* OutputNode = nullptr;

                    for (UEdGraphNode* NodeBase : Graph->Nodes)
                    {
                        UNiagaraNodeOutput* Node = Cast<UNiagaraNodeOutput>(NodeBase);
                        if (Node && Node->GetUsage() == ScriptUsage)
                        {
                            OutputNode = Node;
                            break;
                        }
                    }

                    if (!OutputNode)
                    {
                        OpResult->SetBoolField(TEXT("success"), false);
                        OpResult->SetStringField(TEXT("error"), TEXT("Failed to find output node"));
                        FailCount++;
                    }
                    else
                    {
                        UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
                            ModuleScript,
                            *OutputNode,
                            INDEX_NONE,
                            FString(),
                            FGuid()
                        );

                        if (NewModule)
                        {
                            OpResult->SetBoolField(TEXT("success"), true);
                            OpResult->SetStringField(TEXT("action"), TEXT("add_module"));
                            OpResult->SetStringField(TEXT("module_path"), ModulePath);
                            OpResult->SetStringField(TEXT("module_name"), NewModule->GetName());
                            SuccessCount++;
                            Script->Modify();
                        }
                        else
                        {
                            OpResult->SetBoolField(TEXT("success"), false);
                            OpResult->SetStringField(TEXT("error"), TEXT("Failed to add module to stack"));
                            FailCount++;
                        }
                    }
                }
            }
        }
        else if (Action == TEXT("add_module_to_usage"))
        {
            FString ModulePath;
            FString UsageString;
            if (!Op->TryGetStringField(TEXT("module_path"), ModulePath))
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Missing module_path"));
                FailCount++;
            }
            else if (!Op->TryGetStringField(TEXT("usage"), UsageString))
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Missing usage"));
                FailCount++;
            }
            else
            {
                UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
                if (!ModuleScript)
                {
                    OpResult->SetBoolField(TEXT("success"), false);
                    OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load module: %s"), *ModulePath));
                    FailCount++;
                }
                else
                {
                    const FString NormalizedUsage = UsageString.ToLower();
                    ENiagaraScriptUsage TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
                    if (NormalizedUsage == TEXT("emitter_update") || NormalizedUsage == TEXT("emitterupdatescript"))
                    {
                        TargetUsage = ENiagaraScriptUsage::EmitterUpdateScript;
                    }
                    else if (NormalizedUsage == TEXT("emitter_spawn") || NormalizedUsage == TEXT("emitterspawnscript"))
                    {
                        TargetUsage = ENiagaraScriptUsage::EmitterSpawnScript;
                    }
                    else if (NormalizedUsage == TEXT("particle_spawn") || NormalizedUsage == TEXT("spawn") || NormalizedUsage == TEXT("particlespawnscript"))
                    {
                        TargetUsage = ENiagaraScriptUsage::ParticleSpawnScript;
                    }
                    else if (NormalizedUsage == TEXT("particle_update") || NormalizedUsage == TEXT("update") || NormalizedUsage == TEXT("particleupdatescript"))
                    {
                        TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
                    }
                    else if (NormalizedUsage == TEXT("particle_event") || NormalizedUsage == TEXT("event") || NormalizedUsage == TEXT("particleeventscript"))
                    {
                        TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
                    }

                    UNiagaraNodeOutput* OutputNode = nullptr;
                    for (UEdGraphNode* NodeBase : Graph->Nodes)
                    {
                        UNiagaraNodeOutput* Node = Cast<UNiagaraNodeOutput>(NodeBase);
                        if (Node && Node->GetUsage() == TargetUsage)
                        {
                            OutputNode = Node;
                            break;
                        }
                    }

                    if (!OutputNode)
                    {
                        OpResult->SetBoolField(TEXT("success"), false);
                        OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find output node for usage: %s"), *UsageString));
                        FailCount++;
                    }
                    else
                    {
                        UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
                            ModuleScript,
                            *OutputNode,
                            INDEX_NONE,
                            FString(),
                            FGuid()
                        );

                        if (NewModule)
                        {
                            Graph->NotifyGraphChanged();
                            Script->Modify();
                            OpResult->SetBoolField(TEXT("success"), true);
                            OpResult->SetStringField(TEXT("action"), TEXT("add_module_to_usage"));
                            OpResult->SetStringField(TEXT("usage"), UsageString);
                            OpResult->SetStringField(TEXT("module_path"), ModulePath);
                            OpResult->SetStringField(TEXT("module_name"), NewModule->GetName());
                            SuccessCount++;
                        }
                        else
                        {
                            OpResult->SetBoolField(TEXT("success"), false);
                            OpResult->SetStringField(TEXT("error"), TEXT("Failed to add module to stack"));
                            FailCount++;
                        }
                    }
                }
            }
        }
        else if (Action == TEXT("set_event_handler_options"))
        {
            if (!EmitterDataForOps)
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("set_event_handler_options requires asset_path + emitter"));
                FailCount++;
            }
            else
            {
                int32 EventIndex = 0;
                Op->TryGetNumberField(TEXT("event_index"), EventIndex);

                if (!EmitterDataForOps->EventHandlerScriptProps.IsValidIndex(EventIndex))
                {
                    OpResult->SetBoolField(TEXT("success"), false);
                    OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Event handler index out of range: %d"), EventIndex));
                    FailCount++;
                }
                else
                {
                    FNiagaraEventScriptProperties& EventHandler = EmitterDataForOps->EventHandlerScriptProps[EventIndex];
                    Script->Modify();
                    if (NiagaraSystem)
                    {
                        NiagaraSystem->Modify();
                    }

                    FString ExecutionModeString;
                    if (Op->TryGetStringField(TEXT("execution_mode"), ExecutionModeString))
                    {
                        const FString NormalizedMode = ExecutionModeString.ToLower();
                        if (NormalizedMode == TEXT("spawnedparticles") || NormalizedMode == TEXT("spawned_particles"))
                        {
                            EventHandler.ExecutionMode = EScriptExecutionMode::SpawnedParticles;
                        }
                        else if (NormalizedMode == TEXT("singleparticle") || NormalizedMode == TEXT("single_particle"))
                        {
                            EventHandler.ExecutionMode = EScriptExecutionMode::SingleParticle;
                        }
                        else
                        {
                            EventHandler.ExecutionMode = EScriptExecutionMode::EveryParticle;
                        }
                    }

                    int32 IntValue = 0;
                    if (Op->TryGetNumberField(TEXT("spawn_number"), IntValue))
                    {
                        EventHandler.SpawnNumber = FMath::Max(0, IntValue);
                    }
                    if (Op->TryGetNumberField(TEXT("max_events_per_frame"), IntValue))
                    {
                        EventHandler.MaxEventsPerFrame = FMath::Max(0, IntValue);
                    }
                    if (Op->TryGetNumberField(TEXT("min_spawn_number"), IntValue))
                    {
                        EventHandler.MinSpawnNumber = FMath::Max(0, IntValue);
                    }

                    bool BoolValue = false;
                    if (Op->TryGetBoolField(TEXT("random_spawn_number"), BoolValue))
                    {
                        EventHandler.bRandomSpawnNumber = BoolValue;
                    }
                    if (Op->TryGetBoolField(TEXT("update_attribute_initial_values"), BoolValue))
                    {
                        EventHandler.UpdateAttributeInitialValues = BoolValue;
                    }

                    OpResult->SetBoolField(TEXT("success"), true);
                    OpResult->SetStringField(TEXT("action"), TEXT("set_event_handler_options"));
                    OpResult->SetNumberField(TEXT("event_index"), EventIndex);
                    OpResult->SetNumberField(TEXT("spawn_number"), EventHandler.SpawnNumber);
                    OpResult->SetNumberField(TEXT("max_events_per_frame"), EventHandler.MaxEventsPerFrame);
                    SuccessCount++;
                }
            }
        }
        else if (Action == TEXT("remove_module"))
        {
            FString ModuleName;
            if (Op->TryGetStringField(TEXT("module_name"), ModuleName))
            {
                bool bFound = false;
                for (UEdGraphNode* NodeBase : Graph->Nodes)
                {
                    UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(NodeBase);
                    if (FuncNode && FuncNode->GetName().Contains(ModuleName))
                    {
                        Script->Modify();
                        FuncNode->Modify();

                        // Find input and output pins for the ParameterMap chain
                        UEdGraphPin* InputMapPin = nullptr;
                        UEdGraphPin* OutputMapPin = nullptr;
                        
                        for (UEdGraphPin* Pin : FuncNode->Pins)
                        {
                            if (Pin->GetName() == TEXT("InputMap") && Pin->Direction == EGPD_Input)
                            {
                                InputMapPin = Pin;
                            }
                            else if (Pin->GetName() == TEXT("OutputMap") && Pin->Direction == EGPD_Output)
                            {
                                OutputMapPin = Pin;
                            }
                        }

                        // Reconnect the chain: previous output -> next input
                        if (InputMapPin && OutputMapPin)
                        {
                            // Get the connected source pin (from previous module)
                            TArray<UEdGraphPin*> ConnectedInputPins = InputMapPin->LinkedTo;
                            
                            // Get all pins connected to our output (next modules)
                            TArray<UEdGraphPin*> ConnectedOutputPins = OutputMapPin->LinkedTo;
                            
                            // Break all connections
                            InputMapPin->BreakAllPinLinks();
                            OutputMapPin->BreakAllPinLinks();
                            
                            // Reconnect: previous module's output -> next module's input
                            for (UEdGraphPin* SourcePin : ConnectedInputPins)
                            {
                                for (UEdGraphPin* TargetPin : ConnectedOutputPins)
                                {
                                    SourcePin->MakeLinkTo(TargetPin);
                                }
                            }
                        }
                        else
                        {
                            // Fallback: just break all links
                            for (UEdGraphPin* Pin : FuncNode->Pins)
                            {
                                if (Pin)
                                {
                                    Pin->BreakAllPinLinks();
                                }
                            }
                        }

                        Graph->RemoveNode(FuncNode);

                        OpResult->SetBoolField(TEXT("success"), true);
                        OpResult->SetStringField(TEXT("action"), TEXT("remove_module"));
                        OpResult->SetStringField(TEXT("module_name"), ModuleName);
                        SuccessCount++;
                        bFound = true;
                        break;
                    }
                }

                if (!bFound)
                {
                    OpResult->SetBoolField(TEXT("success"), false);
                    OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Module not found: %s"), *ModuleName));
                    FailCount++;
                }
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Missing module_name"));
                FailCount++;
            }
        }
        else if (Action == TEXT("set_parameter"))
        {
            FString ParamName;
            if (!Op->TryGetStringField(TEXT("name"), ParamName))
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Missing parameter name"));
                FailCount++;
            }
            else
            {
                TSharedPtr<FJsonValue> ValueJson = Op->TryGetField(TEXT("value"));
                if (ValueJson.IsValid())
                {
                    float FloatValue;
                    if (ValueJson->TryGetNumber(FloatValue))
                    {
                        FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(), *ParamName);
                        Var.SetValue(FloatValue);
                        bool bAddIfMissing = true;
                        Script->RapidIterationParameters.SetParameterData(Var.GetData(), Var, bAddIfMissing);

                        OpResult->SetBoolField(TEXT("success"), true);
                        OpResult->SetStringField(TEXT("action"), TEXT("set_parameter"));
                        OpResult->SetStringField(TEXT("name"), ParamName);
                        SuccessCount++;
                        Script->Modify();
                    }
                    else
                    {
                        bool BoolValue = false;
                        if (ValueJson->TryGetBool(BoolValue))
                        {
                            FNiagaraVariable Var(FNiagaraTypeDefinition::GetBoolDef(), *ParamName);
                            Var.SetValue(BoolValue);
                            bool bAddIfMissing = true;
                            Script->RapidIterationParameters.SetParameterData(Var.GetData(), Var, bAddIfMissing);

                            OpResult->SetBoolField(TEXT("success"), true);
                            OpResult->SetStringField(TEXT("action"), TEXT("set_parameter"));
                            OpResult->SetStringField(TEXT("name"), ParamName);
                            SuccessCount++;
                            Script->Modify();
                        }
                        else
                        {
                            const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
                            if (ValueJson->TryGetArray(ArrayValue) && ArrayValue && ArrayValue->Num() >= 3)
                            {
                                FVector3f VectorValue(
                                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                                    static_cast<float>((*ArrayValue)[1]->AsNumber()),
                                    static_cast<float>((*ArrayValue)[2]->AsNumber()));

                                FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetVec3Def();
                                FString TypeName;
                                Op->TryGetStringField(TEXT("type"), TypeName);
                                if (TypeName.Equals(TEXT("position"), ESearchCase::IgnoreCase))
                                {
                                    TypeDef = FNiagaraTypeDefinition::GetPositionDef();
                                }

                                FNiagaraVariable Var(TypeDef, *ParamName);
                                Var.SetValue(VectorValue);
                                bool bAddIfMissing = true;
                                Script->RapidIterationParameters.SetParameterData(Var.GetData(), Var, bAddIfMissing);

                                OpResult->SetBoolField(TEXT("success"), true);
                                OpResult->SetStringField(TEXT("action"), TEXT("set_parameter"));
                                OpResult->SetStringField(TEXT("name"), ParamName);
                                SuccessCount++;
                                Script->Modify();
                            }
                            else
                            {
                                OpResult->SetBoolField(TEXT("success"), false);
                                OpResult->SetStringField(TEXT("error"), TEXT("Unsupported parameter value type"));
                                FailCount++;
                            }
                        }
                    }
                }
                else
                {
                    OpResult->SetBoolField(TEXT("success"), false);
                    OpResult->SetStringField(TEXT("error"), TEXT("Missing parameter value"));
                    FailCount++;
                }
            }
        }
        else if (Action == TEXT("restore_orbit_delta_counter"))
        {
            FString FromTimeSource = TEXT("System.Age");
            Op->TryGetStringField(TEXT("from_time_source"), FromTimeSource);

            FString TimeSource = TEXT("Engine.DeltaTime");
            Op->TryGetStringField(TEXT("time_source"), TimeSource);

            FString CounterPinName = TEXT("Particles.Module.IncrementingCounter");
            Op->TryGetStringField(TEXT("counter_pin"), CounterPinName);

            Graph->Modify();
            Script->Modify();

            int32 TimePinsRenamed = 0;
            int32 CounterLinksAdded = 0;
            int32 CounterLinksRemoved = 0;

            UEdGraphPin* CounterOutputPin = nullptr;
            UEdGraphPin* CounterAddInputPin = nullptr;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                if (!NodeBase)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == FromTimeSource)
                    {
                        NodeBase->Modify();
                        Pin->PinName = FName(*TimeSource);
                        Pin->PinFriendlyName = FText::FromString(TimeSource);
                        TimePinsRenamed++;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == CounterPinName)
                    {
                        bool bPreferredCounterSource = false;
                        for (UEdGraphPin* SiblingPin : NodeBase->Pins)
                        {
                            if (SiblingPin && SiblingPin->Direction == EGPD_Output)
                            {
                                const FString SiblingName = SiblingPin->PinName.ToString();
                                if (SiblingName == TEXT("Module.Rotation Rate") || SiblingName == TEXT("Module.Delta Time"))
                                {
                                    bPreferredCounterSource = true;
                                    break;
                                }
                            }
                        }

                        // The counter must come from the upstream ParameterMapGet. Using a downstream get
                        // creates a graph cycle: Set Counter -> Get Counter -> Add -> Set Counter.
                        if (!CounterOutputPin || bPreferredCounterSource)
                        {
                            CounterOutputPin = Pin;
                        }
                    }
                }
            }

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                UNiagaraNodeOp* OpNode = Cast<UNiagaraNodeOp>(NodeBase);
                if (!OpNode)
                {
                    continue;
                }

                UEdGraphPin* ResultPin = nullptr;
                UEdGraphPin* CandidateBPin = nullptr;
                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == TEXT("Result"))
                    {
                        ResultPin = Pin;
                    }
                    else if (Pin->Direction == EGPD_Input && Pin->PinName.ToString() == TEXT("B"))
                    {
                        CandidateBPin = Pin;
                    }
                }

                if (!ResultPin || !CandidateBPin)
                {
                    continue;
                }

                bool bWritesCounter = false;
                for (UEdGraphPin* LinkedPin : ResultPin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->Direction == EGPD_Input && LinkedPin->PinName.ToString() == CounterPinName)
                    {
                        bWritesCounter = true;
                        break;
                    }
                }

                if (bWritesCounter)
                {
                    CounterAddInputPin = CandidateBPin;
                    break;
                }
            }

            if (CounterOutputPin && CounterAddInputPin && !CounterAddInputPin->LinkedTo.Contains(CounterOutputPin))
            {
                if (UEdGraphNode* CounterNode = CounterOutputPin->GetOwningNode())
                {
                    CounterNode->Modify();
                }
                if (UEdGraphNode* AddNode = CounterAddInputPin->GetOwningNode())
                {
                    AddNode->Modify();
                }

                TArray<UEdGraphPin*> ExistingCounterLinks = CounterAddInputPin->LinkedTo;
                for (UEdGraphPin* ExistingPin : ExistingCounterLinks)
                {
                    if (ExistingPin && ExistingPin != CounterOutputPin && ExistingPin->PinName.ToString() == CounterPinName)
                    {
                        if (UEdGraphNode* ExistingNode = ExistingPin->GetOwningNode())
                        {
                            ExistingNode->Modify();
                        }
                        ExistingPin->BreakLinkTo(CounterAddInputPin);
                        CounterLinksRemoved++;
                    }
                }

                if (!CounterAddInputPin->LinkedTo.Contains(CounterOutputPin))
                {
                    CounterOutputPin->MakeLinkTo(CounterAddInputPin);
                    CounterLinksAdded++;
                }
            }
            else if (CounterOutputPin && CounterAddInputPin)
            {
                TArray<UEdGraphPin*> ExistingCounterLinks = CounterAddInputPin->LinkedTo;
                for (UEdGraphPin* ExistingPin : ExistingCounterLinks)
                {
                    if (ExistingPin && ExistingPin != CounterOutputPin && ExistingPin->PinName.ToString() == CounterPinName)
                    {
                        if (UEdGraphNode* ExistingNode = ExistingPin->GetOwningNode())
                        {
                            ExistingNode->Modify();
                        }
                        if (UEdGraphNode* AddNode = CounterAddInputPin->GetOwningNode())
                        {
                            AddNode->Modify();
                        }
                        ExistingPin->BreakLinkTo(CounterAddInputPin);
                        CounterLinksRemoved++;
                    }
                }
            }

            if (CounterLinksRemoved > 0 && CounterOutputPin && CounterAddInputPin && !CounterAddInputPin->LinkedTo.Contains(CounterOutputPin))
            {
                if (UEdGraphNode* CounterNode = CounterOutputPin->GetOwningNode())
                {
                    CounterNode->Modify();
                }
                if (UEdGraphNode* AddNode = CounterAddInputPin->GetOwningNode())
                {
                    AddNode->Modify();
                }
                CounterOutputPin->MakeLinkTo(CounterAddInputPin);
                CounterLinksAdded++;
            }

            if (TimePinsRenamed > 0 || CounterLinksAdded > 0 || CounterLinksRemoved > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent restored orbit delta counter"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }
                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("restore_orbit_delta_counter"));
                OpResult->SetStringField(TEXT("time_source"), TimeSource);
                OpResult->SetNumberField(TEXT("time_pins_renamed"), TimePinsRenamed);
                OpResult->SetNumberField(TEXT("counter_links_added"), CounterLinksAdded);
                OpResult->SetNumberField(TEXT("counter_links_removed"), CounterLinksRemoved);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("No orbit delta counter changes were applied"));
                FailCount++;
            }
        }
        else if (Action == TEXT("patch_orbit_absolute_time"))
        {
            FString FromTimeSource = TEXT("Engine.DeltaTime");
            Op->TryGetStringField(TEXT("from_time_source"), FromTimeSource);

            FString TimeSource = TEXT("Emitter.Age");
            Op->TryGetStringField(TEXT("time_source"), TimeSource);

            FString CounterPinName = TEXT("Particles.Module.IncrementingCounter");
            Op->TryGetStringField(TEXT("counter_pin"), CounterPinName);

            FString CounterTargetPinName = TEXT("B");
            Op->TryGetStringField(TEXT("counter_target_pin"), CounterTargetPinName);

            Graph->Modify();
            Script->Modify();

            int32 TimePinsRenamed = 0;
            int32 CounterLinksRemoved = 0;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                if (!NodeBase)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == FromTimeSource)
                    {
                        NodeBase->Modify();
                        Pin->PinName = FName(*TimeSource);
                        Pin->PinFriendlyName = FText::FromString(TimeSource);
                        TimePinsRenamed++;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == CounterPinName)
                    {
                        TArray<UEdGraphPin*> LinkedPins = Pin->LinkedTo;
                        for (UEdGraphPin* LinkedPin : LinkedPins)
                        {
                            if (!LinkedPin)
                            {
                                continue;
                            }

                            if (LinkedPin->Direction == EGPD_Input && LinkedPin->PinName.ToString() == CounterTargetPinName)
                            {
                                NodeBase->Modify();
                                if (UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode())
                                {
                                    LinkedNode->Modify();
                                }
                                Pin->BreakLinkTo(LinkedPin);
                                LinkedPin->DefaultValue = TEXT("0.0");
                                LinkedPin->AutogeneratedDefaultValue = TEXT("0.0");
                                LinkedPin->DefaultTextValue = FText::FromString(TEXT("0.0"));
                                CounterLinksRemoved++;
                            }
                        }
                    }
                }
            }

            if (TimePinsRenamed > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent patched orbit absolute time"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }
                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("patch_orbit_absolute_time"));
                OpResult->SetStringField(TEXT("time_source"), TimeSource);
                OpResult->SetNumberField(TEXT("time_pins_renamed"), TimePinsRenamed);
                OpResult->SetNumberField(TEXT("counter_links_removed"), CounterLinksRemoved);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Could not find output pin: %s"), *FromTimeSource));
                FailCount++;
            }
        }
        else if (Action == TEXT("patch_orbit_direct_time"))
        {
            FString FromTimeSource = TEXT("Engine.DeltaTime");
            Op->TryGetStringField(TEXT("from_time_source"), FromTimeSource);

            FString TimeSource = TEXT("Particles.Age");
            Op->TryGetStringField(TEXT("time_source"), TimeSource);

            FString CounterPinName = TEXT("Particles.Module.IncrementingCounter");
            Op->TryGetStringField(TEXT("counter_pin"), CounterPinName);

            Graph->Modify();
            Script->Modify();

            int32 TimePinsRenamed = 0;
            int32 AngleLinksRewired = 0;
            int32 CounterSetNodesRemoved = 0;
            int32 CounterAddNodesRemoved = 0;

            UNiagaraNodeOp* CounterAddNode = nullptr;
            UEdGraphPin* CounterAddInputAPin = nullptr;
            UEdGraphPin* CounterAddResultPin = nullptr;
            UEdGraphPin* TimeMultiplyResultPin = nullptr;
            UNiagaraNodeOp* AngleAddNode = nullptr;
            UEdGraphPin* AngleAddInputAPin = nullptr;
            UNiagaraNodeParameterMapSet* CounterSetNode = nullptr;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                if (!NodeBase)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString() == FromTimeSource)
                    {
                        NodeBase->Modify();
                        Pin->PinName = FName(*TimeSource);
                        Pin->PinFriendlyName = FText::FromString(TimeSource);
                        TimePinsRenamed++;
                    }
                }

                if (UNiagaraNodeOp* OpNode = Cast<UNiagaraNodeOp>(NodeBase))
                {
                    UEdGraphPin* ResultPin = nullptr;
                    UEdGraphPin* InputAPin = nullptr;
                    for (UEdGraphPin* Pin : NodeBase->Pins)
                    {
                        if (!Pin)
                        {
                            continue;
                        }
                        if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == TEXT("Result"))
                        {
                            ResultPin = Pin;
                        }
                        else if (Pin->Direction == EGPD_Input && Pin->PinName.ToString() == TEXT("A"))
                        {
                            InputAPin = Pin;
                        }
                    }

                    if (ResultPin)
                    {
                        int32 AngleTargets = 0;
                        bool bWritesCounter = false;
                        for (UEdGraphPin* LinkedPin : ResultPin->LinkedTo)
                        {
                            if (!LinkedPin)
                            {
                                continue;
                            }
                            const FString LinkedName = LinkedPin->PinName.ToString();
                            if (LinkedName == TEXT("Angle"))
                            {
                                AngleTargets++;
                            }
                            else if (LinkedName == CounterPinName)
                            {
                                bWritesCounter = true;
                            }
                        }

                        if (AngleTargets >= 1 && InputAPin)
                        {
                            AngleAddNode = OpNode;
                            AngleAddInputAPin = InputAPin;
                        }

                        if (bWritesCounter)
                        {
                            CounterAddNode = OpNode;
                            CounterAddInputAPin = InputAPin;
                            CounterAddResultPin = ResultPin;
                        }
                    }
                }
                else if (UNiagaraNodeParameterMapSet* SetNode = Cast<UNiagaraNodeParameterMapSet>(NodeBase))
                {
                    for (UEdGraphPin* Pin : NodeBase->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString() == CounterPinName)
                        {
                            CounterSetNode = SetNode;
                            break;
                        }
                    }
                }
            }

            if (CounterAddInputAPin && CounterAddInputAPin->LinkedTo.Num() > 0)
            {
                TimeMultiplyResultPin = CounterAddInputAPin->LinkedTo[0];
            }

            if (TimeMultiplyResultPin && AngleAddInputAPin)
            {
                if (UEdGraphNode* SourceNode = TimeMultiplyResultPin->GetOwningNode())
                {
                    SourceNode->Modify();
                }
                if (UEdGraphNode* AngleNode = AngleAddInputAPin->GetOwningNode())
                {
                    AngleNode->Modify();
                }

                TArray<UEdGraphPin*> ExistingLinks = AngleAddInputAPin->LinkedTo;
                for (UEdGraphPin* ExistingPin : ExistingLinks)
                {
                    if (ExistingPin)
                    {
                        ExistingPin->BreakLinkTo(AngleAddInputAPin);
                    }
                }
                TimeMultiplyResultPin->MakeLinkTo(AngleAddInputAPin);
                AngleLinksRewired++;
            }

            if (CounterSetNode)
            {
                UEdGraphPin* SourcePin = nullptr;
                UEdGraphPin* DestPin = nullptr;
                UEdGraphPin* CounterInputPin = nullptr;
                for (UEdGraphPin* Pin : CounterSetNode->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }
                    if (Pin->Direction == EGPD_Input && Pin->PinName.ToString() == TEXT("Source"))
                    {
                        SourcePin = Pin;
                    }
                    else if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == TEXT("Dest"))
                    {
                        DestPin = Pin;
                    }
                    else if (Pin->Direction == EGPD_Input && Pin->PinName.ToString() == CounterPinName)
                    {
                        CounterInputPin = Pin;
                    }
                }

                const TArray<UEdGraphPin*> UpstreamPins = SourcePin ? SourcePin->LinkedTo : TArray<UEdGraphPin*>();
                const TArray<UEdGraphPin*> DownstreamPins = DestPin ? DestPin->LinkedTo : TArray<UEdGraphPin*>();

                CounterSetNode->Modify();
                if (SourcePin)
                {
                    SourcePin->BreakAllPinLinks();
                }
                if (DestPin)
                {
                    DestPin->BreakAllPinLinks();
                }
                if (CounterInputPin)
                {
                    CounterInputPin->BreakAllPinLinks();
                }

                for (UEdGraphPin* UpstreamPin : UpstreamPins)
                {
                    if (!UpstreamPin)
                    {
                        continue;
                    }
                    for (UEdGraphPin* DownstreamPin : DownstreamPins)
                    {
                        if (DownstreamPin)
                        {
                            UpstreamPin->MakeLinkTo(DownstreamPin);
                        }
                    }
                }

                Graph->RemoveNode(CounterSetNode);
                CounterSetNodesRemoved++;
            }

            if (CounterAddNode)
            {
                CounterAddNode->Modify();
                for (UEdGraphPin* Pin : CounterAddNode->Pins)
                {
                    if (Pin)
                    {
                        Pin->BreakAllPinLinks();
                    }
                }
                Graph->RemoveNode(CounterAddNode);
                CounterAddNodesRemoved++;
            }

            if (TimePinsRenamed > 0 || AngleLinksRewired > 0 || CounterSetNodesRemoved > 0 || CounterAddNodesRemoved > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent patched orbit direct time"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }
                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("patch_orbit_direct_time"));
                OpResult->SetStringField(TEXT("time_source"), TimeSource);
                OpResult->SetNumberField(TEXT("time_pins_renamed"), TimePinsRenamed);
                OpResult->SetNumberField(TEXT("angle_links_rewired"), AngleLinksRewired);
                OpResult->SetNumberField(TEXT("counter_set_nodes_removed"), CounterSetNodesRemoved);
                OpResult->SetNumberField(TEXT("counter_add_nodes_removed"), CounterAddNodesRemoved);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("No direct-time orbit graph changes were applied"));
                FailCount++;
            }
        }
        else if (Action == TEXT("force_orbit_output_position"))
        {
            FString OrbitPositionPinName = TEXT("Output.Module.OutputPosition");
            Op->TryGetStringField(TEXT("orbit_position_pin"), OrbitPositionPinName);

            FString ParticlePositionPinName = TEXT("Particles.Position");
            Op->TryGetStringField(TEXT("particle_position_pin"), ParticlePositionPinName);

            Graph->Modify();
            Script->Modify();

            UEdGraphPin* OrbitPositionOutputPin = nullptr;
            UEdGraphPin* ParticlePositionInputPin = nullptr;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                if (!NodeBase)
                {
                    continue;
                }

                const bool bIsParameterMapSet = NodeBase->IsA(UNiagaraNodeParameterMapSet::StaticClass());

                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == OrbitPositionPinName)
                    {
                        bool bPreferredOutput = false;
                        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                        {
                            if (!LinkedPin)
                            {
                                continue;
                            }

                            const FString LinkedPinName = LinkedPin->PinName.ToString();
                            const FString LinkedNodeClass = LinkedPin->GetOwningNode()
                                ? LinkedPin->GetOwningNode()->GetClass()->GetName()
                                : FString();
                            if (LinkedPinName.Contains(TEXT("if True")) || LinkedNodeClass.Contains(TEXT("Select")))
                            {
                                bPreferredOutput = true;
                                break;
                            }
                        }

                        if (!OrbitPositionOutputPin || bPreferredOutput)
                        {
                            OrbitPositionOutputPin = Pin;
                        }
                    }
                    else if (Pin->Direction == EGPD_Input && Pin->PinName.ToString() == ParticlePositionPinName)
                    {
                        if (!ParticlePositionInputPin || bIsParameterMapSet)
                        {
                            ParticlePositionInputPin = Pin;
                        }
                    }
                }
            }

            if (OrbitPositionOutputPin && ParticlePositionInputPin)
            {
                if (UEdGraphNode* SourceNode = OrbitPositionOutputPin->GetOwningNode())
                {
                    SourceNode->Modify();
                }
                if (UEdGraphNode* TargetNode = ParticlePositionInputPin->GetOwningNode())
                {
                    TargetNode->Modify();
                }

                int32 LinksRemoved = 0;
                TArray<UEdGraphPin*> ExistingLinks = ParticlePositionInputPin->LinkedTo;
                for (UEdGraphPin* ExistingPin : ExistingLinks)
                {
                    if (ExistingPin)
                    {
                        ExistingPin->BreakLinkTo(ParticlePositionInputPin);
                        LinksRemoved++;
                    }
                }

                OrbitPositionOutputPin->MakeLinkTo(ParticlePositionInputPin);

                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent forced orbit output position"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("force_orbit_output_position"));
                OpResult->SetStringField(TEXT("orbit_position_pin"), OrbitPositionPinName);
                OpResult->SetStringField(TEXT("particle_position_pin"), ParticlePositionPinName);
                OpResult->SetNumberField(TEXT("links_removed"), LinksRemoved);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(
                    TEXT("error"),
                    FString::Printf(
                        TEXT("Could not find pins: orbit=%s particle=%s"),
                        *OrbitPositionPinName,
                        *ParticlePositionPinName));
                FailCount++;
            }
        }
        else if (Action == TEXT("orbit_center_from_source_particle_position"))
        {
            FString RotationCenterPinName = TEXT("Module.Rotation Center");
            Op->TryGetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);

            FString SourcePositionPinName = TEXT("Particles.Position");
            Op->TryGetStringField(TEXT("source_position_pin"), SourcePositionPinName);

            Graph->Modify();
            Script->Modify();

            int32 PinsRenamed = 0;
            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(NodeBase);
                if (!GetNode)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : GetNode->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Output || Pin->PinName.ToString() != RotationCenterPinName)
                    {
                        continue;
                    }

                    GetNode->Modify();
                    Pin->PinName = FName(*SourcePositionPinName);
                    Pin->PinFriendlyName = FText::FromString(SourcePositionPinName);
                    PinsRenamed++;
                }
            }

            if (PinsRenamed > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent orbit center from source particle position"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("orbit_center_from_source_particle_position"));
                OpResult->SetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);
                OpResult->SetStringField(TEXT("source_position_pin"), SourcePositionPinName);
                OpResult->SetNumberField(TEXT("pins_renamed"), PinsRenamed);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(
                    TEXT("error"),
                    FString::Printf(TEXT("Could not find rotation center output pin: %s"), *RotationCenterPinName));
                FailCount++;
            }
        }
        else if (Action == TEXT("orbit_center_default_bind_source_particle_position"))
        {
            FString RotationCenterPinName = TEXT("Module.Rotation Center");
            Op->TryGetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);

            FString SourcePositionPinName = TEXT("Particles.Position");
            Op->TryGetStringField(TEXT("source_position_pin"), SourcePositionPinName);

            Graph->Modify();
            Script->Modify();

            int32 PinsRestored = 0;
            int32 BindingsSet = 0;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(NodeBase);
                if (!GetNode)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : GetNode->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Output || Pin->PinName.ToString() != SourcePositionPinName)
                    {
                        continue;
                    }

                    bool bFeedsOrbitCenter = false;
                    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        UNiagaraNodeOp* LinkedOp = LinkedPin ? Cast<UNiagaraNodeOp>(LinkedPin->GetOwningNode()) : nullptr;
                        if (LinkedPin && LinkedPin->Direction == EGPD_Input && LinkedPin->PinName == FName(TEXT("B")) &&
                            LinkedOp && LinkedOp->OpName == FName(TEXT("Numeric::Add")))
                        {
                            bFeedsOrbitCenter = true;
                            break;
                        }
                    }

                    if (!bFeedsOrbitCenter)
                    {
                        continue;
                    }

                    GetNode->Modify();
                    Pin->PinName = FName(*RotationCenterPinName);
                    Pin->PinFriendlyName = FText::FromString(RotationCenterPinName);
                    PinsRestored++;
                }
            }

            UNiagaraScriptVariable* ScriptVariable = Graph->GetScriptVariable(FName(*RotationCenterPinName));
            if (ScriptVariable)
            {
                ScriptVariable->Modify();
                ScriptVariable->DefaultMode = ENiagaraDefaultMode::Binding;
                ScriptVariable->DefaultBinding.SetName(FName(*SourcePositionPinName));
                ScriptVariable->UpdateChangeId();
                BindingsSet++;
            }

            if (PinsRestored > 0 || BindingsSet > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent orbit center default bound to source particle position"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("orbit_center_default_bind_source_particle_position"));
                OpResult->SetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);
                OpResult->SetStringField(TEXT("source_position_pin"), SourcePositionPinName);
                OpResult->SetNumberField(TEXT("pins_restored"), PinsRestored);
                OpResult->SetNumberField(TEXT("bindings_set"), BindingsSet);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Could not restore orbit center pin or set default binding"));
                FailCount++;
            }
        }
        else if (Action == TEXT("orbit_center_default_value"))
        {
            FString RotationCenterPinName = TEXT("Module.Rotation Center");
            Op->TryGetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);

            FString SourcePositionPinName = TEXT("Particles.Position");
            Op->TryGetStringField(TEXT("source_position_pin"), SourcePositionPinName);

            FVector3f DefaultValue(0.0f, 0.0f, 0.0f);
            TSharedPtr<FJsonValue> ValueJson = Op->TryGetField(TEXT("value"));
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
            if (ValueJson.IsValid() && ValueJson->TryGetArray(ArrayValue) && ArrayValue && ArrayValue->Num() >= 3)
            {
                DefaultValue = FVector3f(
                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                    static_cast<float>((*ArrayValue)[1]->AsNumber()),
                    static_cast<float>((*ArrayValue)[2]->AsNumber()));
            }

            Graph->Modify();
            Script->Modify();

            int32 PinsRestored = 0;
            int32 DefaultsSet = 0;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(NodeBase);
                if (!GetNode)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : GetNode->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Output || Pin->PinName.ToString() != SourcePositionPinName)
                    {
                        continue;
                    }

                    bool bFeedsOrbitCenter = false;
                    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        UNiagaraNodeOp* LinkedOp = LinkedPin ? Cast<UNiagaraNodeOp>(LinkedPin->GetOwningNode()) : nullptr;
                        if (LinkedPin && LinkedPin->Direction == EGPD_Input && LinkedPin->PinName == FName(TEXT("B")) &&
                            LinkedOp && LinkedOp->OpName == FName(TEXT("Numeric::Add")))
                        {
                            bFeedsOrbitCenter = true;
                            break;
                        }
                    }

                    if (!bFeedsOrbitCenter)
                    {
                        continue;
                    }

                    GetNode->Modify();
                    Pin->PinName = FName(*RotationCenterPinName);
                    Pin->PinFriendlyName = FText::FromString(RotationCenterPinName);
                    PinsRestored++;
                }
            }

            UNiagaraScriptVariable* ScriptVariable = Graph->GetScriptVariable(FName(*RotationCenterPinName));
            if (ScriptVariable)
            {
                ScriptVariable->Modify();
                ScriptVariable->DefaultMode = ENiagaraDefaultMode::Value;

                FNiagaraVariable DefaultVariable = ScriptVariable->Variable;
                if (!DefaultVariable.GetType().IsValid())
                {
                    DefaultVariable = FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), FName(*RotationCenterPinName));
                }
                DefaultVariable.SetValue(DefaultValue);
                ScriptVariable->SetDefaultValueData(DefaultVariable.GetData());
                ScriptVariable->UpdateChangeId();
                DefaultsSet++;
            }

            if (PinsRestored > 0 || DefaultsSet > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent orbit center restored to value default"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("orbit_center_default_value"));
                OpResult->SetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);
                OpResult->SetNumberField(TEXT("pins_restored"), PinsRestored);
                OpResult->SetNumberField(TEXT("defaults_set"), DefaultsSet);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(TEXT("error"), TEXT("Could not restore orbit center value default"));
                FailCount++;
            }
        }
        else if (Action == TEXT("orbit_use_particle_position_center"))
        {
            FString ParticlePositionPinName = TEXT("Particles.Position");
            Op->TryGetStringField(TEXT("particle_position_pin"), ParticlePositionPinName);

            FString RotationCenterPinName = TEXT("Module.Rotation Center");
            Op->TryGetStringField(TEXT("rotation_center_pin"), RotationCenterPinName);

            Graph->Modify();
            Script->Modify();

            UEdGraphPin* ParticlePositionOutputPin = nullptr;
            TArray<UEdGraphPin*> RotationCenterTargetPins;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                if (!NodeBase)
                {
                    continue;
                }

                for (UEdGraphPin* Pin : NodeBase->Pins)
                {
                    if (!Pin)
                    {
                        continue;
                    }

                    if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == ParticlePositionPinName)
                    {
                        bool bPreferredOutput = false;
                        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                        {
                            if (LinkedPin && LinkedPin->PinName.ToString().Contains(TEXT("if False")))
                            {
                                bPreferredOutput = true;
                                break;
                            }
                        }

                        if (!ParticlePositionOutputPin || bPreferredOutput)
                        {
                            ParticlePositionOutputPin = Pin;
                        }
                    }
                    else if (Pin->Direction == EGPD_Output && Pin->PinName.ToString() == RotationCenterPinName)
                    {
                        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                        {
                            if (LinkedPin && LinkedPin->Direction == EGPD_Input)
                            {
                                RotationCenterTargetPins.AddUnique(LinkedPin);
                            }
                        }
                    }
                }
            }

            int32 RewiredLinks = 0;
            if (ParticlePositionOutputPin && RotationCenterTargetPins.Num() > 0)
            {
                if (UEdGraphNode* SourceNode = ParticlePositionOutputPin->GetOwningNode())
                {
                    SourceNode->Modify();
                }

                for (UEdGraphPin* TargetPin : RotationCenterTargetPins)
                {
                    if (!TargetPin)
                    {
                        continue;
                    }

                    if (UEdGraphNode* TargetNode = TargetPin->GetOwningNode())
                    {
                        TargetNode->Modify();
                    }

                    TArray<UEdGraphPin*> ExistingLinks = TargetPin->LinkedTo;
                    for (UEdGraphPin* ExistingPin : ExistingLinks)
                    {
                        if (ExistingPin)
                        {
                            ExistingPin->BreakLinkTo(TargetPin);
                        }
                    }

                    ParticlePositionOutputPin->MakeLinkTo(TargetPin);
                    RewiredLinks++;
                }
            }

            if (RewiredLinks > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent orbit uses particle position center"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("orbit_use_particle_position_center"));
                OpResult->SetNumberField(TEXT("rewired_links"), RewiredLinks);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(
                    TEXT("error"),
                    FString::Printf(
                        TEXT("Could not rewire orbit center: particle=%s targets=%d"),
                        ParticlePositionOutputPin ? TEXT("found") : TEXT("missing"),
                        RotationCenterTargetPins.Num()));
                FailCount++;
            }
        }
        else if (Action == TEXT("set_function_input_int_default"))
        {
            FString FunctionName = TEXT("Get Position By Index");
            Op->TryGetStringField(TEXT("function_name"), FunctionName);

            FString InputPinName = TEXT("Particle Index");
            Op->TryGetStringField(TEXT("input_pin"), InputPinName);

            int32 IntValue = 0;
            Op->TryGetNumberField(TEXT("value"), IntValue);

            const FString PinDefaultValue = FString::FromInt(IntValue);

            Graph->Modify();
            Script->Modify();

            int32 MatchedFunctions = 0;
            int32 InputsUpdated = 0;
            int32 LinksRemoved = 0;

            for (UEdGraphNode* NodeBase : Graph->Nodes)
            {
                UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(NodeBase);
                if (!FuncNode || !FuncNode->GetFunctionName().Equals(FunctionName, ESearchCase::IgnoreCase))
                {
                    continue;
                }

                MatchedFunctions++;
                for (UEdGraphPin* Pin : FuncNode->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Input || Pin->PinName.ToString() != InputPinName)
                    {
                        continue;
                    }

                    FuncNode->Modify();
                    Pin->Modify();

                    const TArray<UEdGraphPin*> ExistingLinks = Pin->LinkedTo;
                    for (UEdGraphPin* LinkedPin : ExistingLinks)
                    {
                        if (LinkedPin)
                        {
                            if (UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode())
                            {
                                LinkedNode->Modify();
                            }
                            LinkedPin->BreakLinkTo(Pin);
                            LinksRemoved++;
                        }
                    }

                    Pin->DefaultValue = PinDefaultValue;
                    Pin->AutogeneratedDefaultValue = PinDefaultValue;
                    Pin->DefaultTextValue = FText::FromString(PinDefaultValue);
                    FuncNode->MarkNodeRequiresSynchronization(TEXT("TAAgent set function input int default"), true);
                    InputsUpdated++;
                }
            }

            if (InputsUpdated > 0)
            {
                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent set function input int default"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("set_function_input_int_default"));
                OpResult->SetStringField(TEXT("function_name"), FunctionName);
                OpResult->SetStringField(TEXT("input_pin"), InputPinName);
                OpResult->SetNumberField(TEXT("value"), IntValue);
                OpResult->SetStringField(TEXT("pin_default_value"), PinDefaultValue);
                OpResult->SetNumberField(TEXT("matched_functions"), MatchedFunctions);
                OpResult->SetNumberField(TEXT("inputs_updated"), InputsUpdated);
                OpResult->SetNumberField(TEXT("links_removed"), LinksRemoved);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(
                    TEXT("error"),
                    FString::Printf(
                        TEXT("Could not find input pin: function=%s input=%s matched_functions=%d"),
                        *FunctionName,
                        *InputPinName,
                        MatchedFunctions));
                FailCount++;
            }
        }
        else if (Action == TEXT("set_particle_read_emitter_binding"))
        {
            FString EmitterNameToBind = TEXT("Yuan");
            Op->TryGetStringField(TEXT("emitter_name"), EmitterNameToBind);

            FString ParameterName = TEXT("Module.粒子属性阅读器");
            Op->TryGetStringField(TEXT("parameter_name"), ParameterName);

            FString ReaderObjectName;
            Op->TryGetStringField(TEXT("reader_object_name"), ReaderObjectName);

            Script->Modify();

            TArray<UObject*> InnerObjects;
            GetObjectsWithOuter(Script, InnerObjects, false);

            int32 ReadersFound = 0;
            int32 ReadersUpdated = 0;
            UNiagaraDataInterfaceParticleRead* FirstUpdatedReader = nullptr;
            TArray<TSharedPtr<FJsonValue>> UpdatedReadersArray;

            for (UObject* InnerObject : InnerObjects)
            {
                UNiagaraDataInterfaceParticleRead* ParticleReadDI = Cast<UNiagaraDataInterfaceParticleRead>(InnerObject);
                if (!ParticleReadDI)
                {
                    continue;
                }

                if (!ReaderObjectName.IsEmpty() && ParticleReadDI->GetName() != ReaderObjectName)
                {
                    continue;
                }

                ReadersFound++;
                const FString PreviousEmitterName = ParticleReadDI->EmitterBinding.EmitterName.ToString();
                const ENiagaraDataInterfaceEmitterBindingMode PreviousBindingMode = ParticleReadDI->EmitterBinding.BindingMode;

                ParticleReadDI->Modify();
                ParticleReadDI->EmitterBinding.BindingMode = ENiagaraDataInterfaceEmitterBindingMode::Other;
                ParticleReadDI->EmitterBinding.EmitterName = FName(*EmitterNameToBind);

                TSharedPtr<FJsonObject> ReaderResult = MakeShareable(new FJsonObject);
                ReaderResult->SetStringField(TEXT("object_name"), ParticleReadDI->GetName());
                ReaderResult->SetStringField(TEXT("previous_emitter_name"), PreviousEmitterName);
                ReaderResult->SetStringField(TEXT("new_emitter_name"), EmitterNameToBind);
                ReaderResult->SetNumberField(TEXT("previous_binding_mode"), static_cast<int32>(PreviousBindingMode));
                ReaderResult->SetNumberField(TEXT("new_binding_mode"), static_cast<int32>(ParticleReadDI->EmitterBinding.BindingMode));
                UpdatedReadersArray.Add(MakeShareable(new FJsonValueObject(ReaderResult)));
                const bool bCurrentReaderInvalidated = ParticleReadDI->GetName().StartsWith(TEXT("Invalidated_"));
                const bool bFirstReaderInvalidated = FirstUpdatedReader && FirstUpdatedReader->GetName().StartsWith(TEXT("Invalidated_"));
                if (!FirstUpdatedReader || (bFirstReaderInvalidated && !bCurrentReaderInvalidated))
                {
                    FirstUpdatedReader = ParticleReadDI;
                }
                ReadersUpdated++;
            }

            if (ReadersUpdated > 0)
            {
                bool bDefaultVariantUpdated = false;
                FString PreviousDefaultVariantMode = TEXT("None");
                TArray<UNiagaraScriptVariable*> ReaderScriptVariables;
                int32 ReaderInputNodesMatched = 0;
                int32 ReaderScriptVariablesCreated = 0;
                int32 ReaderInputNodeDefaultsUpdated = 0;
                int32 ReaderParameterMapDefaultsUpdated = 0;
                TArray<TSharedPtr<FJsonValue>> UpdatedInputNodesArray;
                TArray<TSharedPtr<FJsonValue>> UpdatedParameterMapDefaultsArray;

                if (UNiagaraScriptVariable* ReaderScriptVariable = Graph->GetScriptVariable(FName(*ParameterName)))
                {
                    ReaderScriptVariables.Add(ReaderScriptVariable);
                }

                for (const TPair<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& ScriptVariablePair : Graph->GetAllMetaData())
                {
                    UNiagaraScriptVariable* ScriptVariable = ScriptVariablePair.Value.Get();
                    if (!ScriptVariable)
                    {
                        continue;
                    }

                    const FNiagaraTypeDefinition& ScriptVariableType = ScriptVariable->Variable.GetType();
                    UClass* ScriptVariableClass = ScriptVariableType.GetClass();
                    if (ScriptVariableType.IsDataInterface() && ScriptVariableClass && ScriptVariableClass->IsChildOf(UNiagaraDataInterfaceParticleRead::StaticClass()))
                    {
                        ReaderScriptVariables.AddUnique(ScriptVariable);
                    }
                }

                for (UEdGraphNode* NodeBase : Graph->Nodes)
                {
                    UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(NodeBase);
                    if (!InputNode)
                    {
                        continue;
                    }

                    const FNiagaraTypeDefinition& InputType = InputNode->Input.GetType();
                    UClass* InputClass = InputType.GetClass();
                    if (!InputType.IsDataInterface() || !InputClass || !InputClass->IsChildOf(UNiagaraDataInterfaceParticleRead::StaticClass()))
                    {
                        continue;
                    }

                    ReaderInputNodesMatched++;
                    InputNode->Modify();

                    if (FObjectPropertyBase* DataInterfaceProperty = FindFProperty<FObjectPropertyBase>(UNiagaraNodeInput::StaticClass(), TEXT("DataInterface")))
                    {
                        UObject* PreviousDataInterface = DataInterfaceProperty->GetObjectPropertyValue_InContainer(InputNode);
                        DataInterfaceProperty->SetObjectPropertyValue_InContainer(InputNode, FirstUpdatedReader);

                        TSharedPtr<FJsonObject> InputNodeResult = MakeShareable(new FJsonObject);
                        InputNodeResult->SetStringField(TEXT("input_name"), InputNode->Input.GetName().ToString());
                        InputNodeResult->SetStringField(TEXT("node_name"), InputNode->GetName());
                        InputNodeResult->SetStringField(TEXT("previous_data_interface"), PreviousDataInterface ? PreviousDataInterface->GetName() : TEXT("None"));
                        InputNodeResult->SetStringField(TEXT("new_data_interface"), FirstUpdatedReader ? FirstUpdatedReader->GetName() : TEXT("None"));
                        UpdatedInputNodesArray.Add(MakeShareable(new FJsonValueObject(InputNodeResult)));
                        ReaderInputNodeDefaultsUpdated++;
                    }

                    UNiagaraScriptVariable* ReaderScriptVariable = Graph->GetScriptVariable(InputNode->Input.GetName());
                    if (!ReaderScriptVariable)
                    {
                        Graph->Modify();
                        ReaderScriptVariable = NewObject<UNiagaraScriptVariable>(Graph, FName(), RF_Transactional);
                        ReaderScriptVariable->Init(InputNode->Input, FNiagaraVariableMetaData());
                        ReaderScriptVariable->SetIsStaticSwitch(false);
                        Graph->GetAllMetaData().Add(InputNode->Input, ReaderScriptVariable);
                        ReaderScriptVariablesCreated++;
                    }
                    if (ReaderScriptVariable)
                    {
                        ReaderScriptVariables.AddUnique(ReaderScriptVariable);
                    }
                }

                for (UEdGraphNode* NodeBase : Graph->Nodes)
                {
                    UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(NodeBase);
                    if (!GetNode)
                    {
                        continue;
                    }

                    TArray<UEdGraphPin*> ValueOutputPins;
                    TArray<UEdGraphPin*> DefaultInputPins;
                    for (UEdGraphPin* Pin : GetNode->Pins)
                    {
                        if (!Pin)
                        {
                            continue;
                        }

                        if (Pin->Direction == EGPD_Output && Pin->PinName != FName(TEXT("Add")))
                        {
                            ValueOutputPins.Add(Pin);
                        }
                        else if (Pin->Direction == EGPD_Input && Pin->PinName != FName(TEXT("Source")))
                        {
                            DefaultInputPins.Add(Pin);
                        }
                    }

                    const int32 PairCount = FMath::Min(ValueOutputPins.Num(), DefaultInputPins.Num());
                    for (int32 PinPairIndex = 0; PinPairIndex < PairCount; ++PinPairIndex)
                    {
                        UEdGraphPin* OutputPin = ValueOutputPins[PinPairIndex];
                        UEdGraphPin* DefaultPin = DefaultInputPins[PinPairIndex];
                        if (!OutputPin || !DefaultPin)
                        {
                            continue;
                        }

                        UClass* OutputPinClass = Cast<UClass>(OutputPin->PinType.PinSubCategoryObject.Get());
                        if (!OutputPinClass || !OutputPinClass->IsChildOf(UNiagaraDataInterfaceParticleRead::StaticClass()))
                        {
                            continue;
                        }

                        GetNode->Modify();
                        OutputPin->Modify();
                        DefaultPin->Modify();

                        UObject* PreviousDefaultObject = DefaultPin->DefaultObject;
                        DefaultPin->DefaultObject = FirstUpdatedReader;
                        DefaultPin->DefaultValue.Reset();
                        DefaultPin->AutogeneratedDefaultValue.Reset();
                        DefaultPin->DefaultTextValue = FText::GetEmpty();

                        TSharedPtr<FJsonObject> DefaultResult = MakeShareable(new FJsonObject);
                        DefaultResult->SetStringField(TEXT("node_name"), GetNode->GetName());
                        DefaultResult->SetStringField(TEXT("output_pin"), OutputPin->PinName.ToString());
                        DefaultResult->SetStringField(TEXT("default_pin"), DefaultPin->PinName.ToString());
                        DefaultResult->SetStringField(TEXT("previous_default_object"), PreviousDefaultObject ? PreviousDefaultObject->GetName() : TEXT("None"));
                        DefaultResult->SetStringField(TEXT("new_default_object"), FirstUpdatedReader ? FirstUpdatedReader->GetName() : TEXT("None"));
                        UpdatedParameterMapDefaultsArray.Add(MakeShareable(new FJsonValueObject(DefaultResult)));
                        ReaderParameterMapDefaultsUpdated++;
                    }
                }

                TArray<TSharedPtr<FJsonValue>> UpdatedScriptVariablesArray;
                for (UNiagaraScriptVariable* ReaderScriptVariable : ReaderScriptVariables)
                {
                    ReaderScriptVariable->Modify();
                    const FString ThisPreviousDefaultVariantMode = StaticEnum<ENiagaraVariantMode>()
                        ? StaticEnum<ENiagaraVariantMode>()->GetNameStringByValue(static_cast<int64>(ReaderScriptVariable->GetDefaultValueVariant().GetMode()))
                        : FString::FromInt(static_cast<int32>(ReaderScriptVariable->GetDefaultValueVariant().GetMode()));
                    if (!bDefaultVariantUpdated)
                    {
                        PreviousDefaultVariantMode = ThisPreviousDefaultVariantMode;
                    }
                    ReaderScriptVariable->DefaultMode = ENiagaraDefaultMode::Value;
                    FNiagaraVariant& MutableDefaultVariant = const_cast<FNiagaraVariant&>(ReaderScriptVariable->GetDefaultValueVariant());
                    MutableDefaultVariant.SetDataInterface(FirstUpdatedReader);
                    ReaderScriptVariable->UpdateChangeId();
                    bDefaultVariantUpdated = true;

                    TSharedPtr<FJsonObject> ScriptVariableResult = MakeShareable(new FJsonObject);
                    ScriptVariableResult->SetStringField(TEXT("variable_name"), ReaderScriptVariable->Variable.GetName().ToString());
                    ScriptVariableResult->SetStringField(TEXT("previous_default_variant_mode"), ThisPreviousDefaultVariantMode);
                    ScriptVariableResult->SetStringField(
                        TEXT("new_default_variant_mode"),
                        StaticEnum<ENiagaraVariantMode>()
                            ? StaticEnum<ENiagaraVariantMode>()->GetNameStringByValue(static_cast<int64>(ReaderScriptVariable->GetDefaultValueVariant().GetMode()))
                            : FString::FromInt(static_cast<int32>(ReaderScriptVariable->GetDefaultValueVariant().GetMode())));
                    UpdatedScriptVariablesArray.Add(MakeShareable(new FJsonValueObject(ScriptVariableResult)));
                }

                Graph->NotifyGraphChanged();
                Script->InvalidateCompileResults(TEXT("TAAgent set particle read emitter binding"));
                if (EmitterDataForOps)
                {
                    EmitterDataForOps->InvalidateCompileResults();
                }

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("set_particle_read_emitter_binding"));
                OpResult->SetStringField(TEXT("emitter_name"), EmitterNameToBind);
                OpResult->SetStringField(TEXT("parameter_name"), ParameterName);
                OpResult->SetNumberField(TEXT("readers_found"), ReadersFound);
                OpResult->SetNumberField(TEXT("readers_updated"), ReadersUpdated);
                OpResult->SetBoolField(TEXT("default_variant_updated"), bDefaultVariantUpdated);
                OpResult->SetStringField(TEXT("previous_default_variant_mode"), PreviousDefaultVariantMode);
                OpResult->SetStringField(TEXT("default_reader_object_name"), FirstUpdatedReader ? FirstUpdatedReader->GetName() : TEXT(""));
                OpResult->SetNumberField(TEXT("reader_input_nodes_matched"), ReaderInputNodesMatched);
                OpResult->SetNumberField(TEXT("reader_input_node_defaults_updated"), ReaderInputNodeDefaultsUpdated);
                OpResult->SetArrayField(TEXT("updated_input_nodes"), UpdatedInputNodesArray);
                OpResult->SetNumberField(TEXT("reader_parameter_map_defaults_updated"), ReaderParameterMapDefaultsUpdated);
                OpResult->SetArrayField(TEXT("updated_parameter_map_defaults"), UpdatedParameterMapDefaultsArray);
                OpResult->SetNumberField(TEXT("script_variables_created"), ReaderScriptVariablesCreated);
                OpResult->SetNumberField(TEXT("script_variables_updated"), UpdatedScriptVariablesArray.Num());
                OpResult->SetArrayField(TEXT("updated_script_variables"), UpdatedScriptVariablesArray);
                OpResult->SetArrayField(TEXT("updated_readers"), UpdatedReadersArray);
                SuccessCount++;
            }
            else
            {
                OpResult->SetBoolField(TEXT("success"), false);
                OpResult->SetStringField(
                    TEXT("error"),
                    FString::Printf(
                        TEXT("No UNiagaraDataInterfaceParticleRead objects found for script %s reader filter '%s'"),
                        *Script->GetName(),
                        *ReaderObjectName));
                FailCount++;
            }
        }
        else if (Action == TEXT("fix_cached_particle_read_bindings"))
        {
            FString EmitterNameToBind = TEXT("Yuan");
            Op->TryGetStringField(TEXT("emitter_name"), EmitterNameToBind);

            Script->Modify();

            int32 CachedReadersFound = 0;
            int32 CachedReadersUpdated = 0;
            TArray<TSharedPtr<FJsonValue>> CachedReadersArray;

            TArray<FNiagaraScriptDataInterfaceInfo>& CachedDefaultDataInterfaces = Script->GetCachedDefaultDataInterfaces();
            for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : CachedDefaultDataInterfaces)
            {
                UNiagaraDataInterfaceParticleRead* ParticleReadDI = Cast<UNiagaraDataInterfaceParticleRead>(DataInterfaceInfo.DataInterface.Get());
                if (!ParticleReadDI)
                {
                    continue;
                }

                CachedReadersFound++;
                const FString PreviousEmitterName = ParticleReadDI->EmitterBinding.EmitterName.ToString();
                const ENiagaraDataInterfaceEmitterBindingMode PreviousBindingMode = ParticleReadDI->EmitterBinding.BindingMode;
                const FString PreviousSourceEmitterName = DataInterfaceInfo.SourceEmitterName;

                ParticleReadDI->Modify();
                ParticleReadDI->EmitterBinding.BindingMode = ENiagaraDataInterfaceEmitterBindingMode::Other;
                ParticleReadDI->EmitterBinding.EmitterName = FName(*EmitterNameToBind);
                DataInterfaceInfo.SourceEmitterName = EmitterNameToBind;

                TSharedPtr<FJsonObject> ReaderResult = MakeShareable(new FJsonObject);
                ReaderResult->SetStringField(TEXT("name"), DataInterfaceInfo.Name.ToString());
                ReaderResult->SetStringField(TEXT("compile_name"), DataInterfaceInfo.CompileName.ToString());
                ReaderResult->SetStringField(TEXT("object_name"), ParticleReadDI->GetName());
                ReaderResult->SetStringField(TEXT("previous_emitter_name"), PreviousEmitterName);
                ReaderResult->SetStringField(TEXT("new_emitter_name"), EmitterNameToBind);
                ReaderResult->SetNumberField(TEXT("previous_binding_mode"), static_cast<int32>(PreviousBindingMode));
                ReaderResult->SetNumberField(TEXT("new_binding_mode"), static_cast<int32>(ParticleReadDI->EmitterBinding.BindingMode));
                ReaderResult->SetStringField(TEXT("previous_source_emitter_name"), PreviousSourceEmitterName);
                ReaderResult->SetStringField(TEXT("new_source_emitter_name"), DataInterfaceInfo.SourceEmitterName);
                CachedReadersArray.Add(MakeShareable(new FJsonValueObject(ReaderResult)));
                CachedReadersUpdated++;
            }

            int32 ResolvedReadersFound = 0;
            int32 ResolvedReadersUpdated = 0;
            TArray<TSharedPtr<FJsonValue>> ResolvedReadersArray;
            TArray<FNiagaraScriptResolvedDataInterfaceInfo> ResolvedDataInterfaces;
            TConstArrayView<FNiagaraScriptResolvedDataInterfaceInfo> ExistingResolvedDataInterfaces = Script->GetResolvedDataInterfaces();
            ResolvedDataInterfaces.Reserve(ExistingResolvedDataInterfaces.Num());
            for (const FNiagaraScriptResolvedDataInterfaceInfo& ExistingResolvedDataInterface : ExistingResolvedDataInterfaces)
            {
                ResolvedDataInterfaces.Add(ExistingResolvedDataInterface);
            }
            for (FNiagaraScriptResolvedDataInterfaceInfo& ResolvedDataInterfaceInfo : ResolvedDataInterfaces)
            {
                UNiagaraDataInterfaceParticleRead* ParticleReadDI = Cast<UNiagaraDataInterfaceParticleRead>(ResolvedDataInterfaceInfo.ResolvedDataInterface.Get());
                if (!ParticleReadDI)
                {
                    continue;
                }

                ResolvedReadersFound++;
                const FString PreviousEmitterName = ParticleReadDI->EmitterBinding.EmitterName.ToString();
                const ENiagaraDataInterfaceEmitterBindingMode PreviousBindingMode = ParticleReadDI->EmitterBinding.BindingMode;
                const FString PreviousSourceEmitterName = ResolvedDataInterfaceInfo.ResolvedSourceEmitterName;

                ParticleReadDI->Modify();
                ParticleReadDI->EmitterBinding.BindingMode = ENiagaraDataInterfaceEmitterBindingMode::Other;
                ParticleReadDI->EmitterBinding.EmitterName = FName(*EmitterNameToBind);
                ResolvedDataInterfaceInfo.ResolvedSourceEmitterName = EmitterNameToBind;

                TSharedPtr<FJsonObject> ReaderResult = MakeShareable(new FJsonObject);
                ReaderResult->SetStringField(TEXT("name"), ResolvedDataInterfaceInfo.Name.ToString());
                ReaderResult->SetStringField(TEXT("compile_name"), ResolvedDataInterfaceInfo.CompileName.ToString());
                ReaderResult->SetStringField(TEXT("object_name"), ParticleReadDI->GetName());
                ReaderResult->SetStringField(TEXT("previous_emitter_name"), PreviousEmitterName);
                ReaderResult->SetStringField(TEXT("new_emitter_name"), EmitterNameToBind);
                ReaderResult->SetNumberField(TEXT("previous_binding_mode"), static_cast<int32>(PreviousBindingMode));
                ReaderResult->SetNumberField(TEXT("new_binding_mode"), static_cast<int32>(ParticleReadDI->EmitterBinding.BindingMode));
                ReaderResult->SetStringField(TEXT("previous_source_emitter_name"), PreviousSourceEmitterName);
                ReaderResult->SetStringField(TEXT("new_source_emitter_name"), ResolvedDataInterfaceInfo.ResolvedSourceEmitterName);
                ResolvedReadersArray.Add(MakeShareable(new FJsonValueObject(ReaderResult)));
                ResolvedReadersUpdated++;
            }

            if (ResolvedReadersUpdated > 0)
            {
                Script->SetResolvedDataInterfaces(ResolvedDataInterfaces);
            }

            OpResult->SetBoolField(TEXT("success"), CachedReadersUpdated > 0 || ResolvedReadersUpdated > 0);
            OpResult->SetStringField(TEXT("action"), TEXT("fix_cached_particle_read_bindings"));
            OpResult->SetStringField(TEXT("emitter_name"), EmitterNameToBind);
            OpResult->SetNumberField(TEXT("cached_readers_found"), CachedReadersFound);
            OpResult->SetNumberField(TEXT("cached_readers_updated"), CachedReadersUpdated);
            OpResult->SetArrayField(TEXT("cached_readers"), CachedReadersArray);
            OpResult->SetNumberField(TEXT("resolved_readers_found"), ResolvedReadersFound);
            OpResult->SetNumberField(TEXT("resolved_readers_updated"), ResolvedReadersUpdated);
            OpResult->SetArrayField(TEXT("resolved_readers"), ResolvedReadersArray);

            if (CachedReadersUpdated > 0 || ResolvedReadersUpdated > 0)
            {
                SuccessCount++;
            }
            else
            {
                OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("No cached or resolved UNiagaraDataInterfaceParticleRead objects found for script %s"), *Script->GetName()));
                FailCount++;
            }
        }
        else if (Action == TEXT("compile_system"))
        {
            if (!NiagaraSystem)
            {
                Script->InvalidateCompileResults(TEXT("TAAgent standalone script compile"));
                Script->RequestCompile(FGuid(), true);

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("compile_system"));
                OpResult->SetBoolField(TEXT("standalone_script"), true);
                SuccessCount++;
            }
            else
            {
                NiagaraSystem->Modify();
                Script->InvalidateCompileResults(TEXT("TAAgent force system compile"));

                if (UNiagaraScript* SystemSpawnScript = NiagaraSystem->GetSystemSpawnScript())
                {
                    SystemSpawnScript->InvalidateCompileResults(TEXT("TAAgent force system compile"));
                }
                if (UNiagaraScript* SystemUpdateScript = NiagaraSystem->GetSystemUpdateScript())
                {
                    SystemUpdateScript->InvalidateCompileResults(TEXT("TAAgent force system compile"));
                }

                int32 InvalidatedEmitters = 0;
                for (FNiagaraEmitterHandle& Handle : NiagaraSystem->GetEmitterHandles())
                {
                    if (FVersionedNiagaraEmitterData* HandleData = Handle.GetEmitterData())
                    {
                        HandleData->InvalidateCompileResults();
                        InvalidatedEmitters++;
                    }
                }

                const bool bLaunchedCompile = NiagaraSystem->RequestCompile(true);
                NiagaraSystem->WaitForCompilationComplete(false, false);
                const bool bHasActiveCompilations = NiagaraSystem->HasActiveCompilations();
                NiagaraSystem->PollForCompilationComplete(false);
                NiagaraSystem->CacheFromCompiledData();

                OpResult->SetBoolField(TEXT("success"), true);
                OpResult->SetStringField(TEXT("action"), TEXT("compile_system"));
                OpResult->SetBoolField(TEXT("launched_compile"), bLaunchedCompile);
                OpResult->SetBoolField(TEXT("has_active_compilations"), bHasActiveCompilations);
                OpResult->SetNumberField(TEXT("invalidated_emitters"), InvalidatedEmitters);
                SuccessCount++;
            }
        }
        else
        {
            OpResult->SetBoolField(TEXT("success"), false);
            OpResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
            FailCount++;
        }

        ResultsArray.Add(MakeShareable(new FJsonValueObject(OpResult)));
    }

    // Mark dirty
    if (NiagaraSystem)
    {
        NiagaraSystem->MarkPackageDirty();
    }
    else
    {
        Script->MarkPackageDirty();
    }

    Result->SetBoolField(TEXT("success"), FailCount == 0);
    Result->SetArrayField(TEXT("results"), ResultsArray);
    Result->SetNumberField(TEXT("success_count"), SuccessCount);
    Result->SetNumberField(TEXT("fail_count"), FailCount);

    return Result;
}

// ============================================================================
// Debug Tools - Compiled Code Inspection
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraCompiledCode(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Get asset path
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing asset_path parameter"));
        return Result;
    }
    
    // Load Niagara System
    UNiagaraSystem* NiagaraSystem = LoadNiagaraSystemAsset(AssetPath);
    if (!NiagaraSystem)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara System: %s"), *AssetPath));
        return Result;
    }
    
    // Get emitter name (required for embedded scripts)
    FString EmitterName;
    Params->TryGetStringField(TEXT("emitter"), EmitterName);
    
    // Get script type (spawn, update)
    FString ScriptType = TEXT("spawn");
    Params->TryGetStringField(TEXT("script"), ScriptType);
    
    // Find the script
    UNiagaraScript* Script = nullptr;
    
    if (EmitterName.IsEmpty())
    {
        // Try to get system-level script
        if (ScriptType == TEXT("system_spawn"))
        {
            Script = NiagaraSystem->GetSystemSpawnScript();
        }
        else if (ScriptType == TEXT("system_update"))
        {
            Script = NiagaraSystem->GetSystemUpdateScript();
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("For system scripts, use script='system_spawn' or 'system_update'"));
            return Result;
        }
    }
    else
    {
        // Find emitter handle
        FNiagaraEmitterHandle* Handle = FindEmitterHandle(NiagaraSystem, EmitterName);
        if (!Handle)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Emitter not found: %s"), *EmitterName));
            return Result;
        }
        
        // Get emitter data (contains script references)
        FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
        if (!EmitterData)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to get emitter data from handle"));
            return Result;
        }
        
        // Get script based on type
        if (ScriptType == TEXT("spawn"))
        {
            Script = EmitterData->SpawnScriptProps.Script;
        }
        else if (ScriptType == TEXT("update"))
        {
            Script = EmitterData->UpdateScriptProps.Script;
        }
        else if (ScriptType == TEXT("gpu_compute"))
        {
            // GPU compute script uses GetScript with specific usage
            Script = EmitterData->GetScript(ENiagaraScriptUsage::ParticleGPUComputeScript, FGuid());
        }
        else
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown script type: %s. Use 'spawn', 'update', or 'gpu_compute'"), *ScriptType));
            return Result;
        }
    }
    
    if (!Script)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Script not found"));
        return Result;
    }
    
    // Extract compiled HLSL code
#if WITH_EDITORONLY_DATA
    // HLSL translations are stored in FNiagaraVMExecutableData
    const FNiagaraVMExecutableData& VMData = Script->GetVMExecutableData();
    FString HlslCpu = VMData.LastHlslTranslation;
    FString HlslGpu = VMData.LastHlslTranslationGPU;
    
    // Get compile errors if any
    TArray<FString> CompileErrors;
#if WITH_EDITOR
    if (Script->GetRenderThreadScript())
    {
        const TArray<FString>& Errors = Script->GetRenderThreadScript()->GetCompileErrors();
        for (const FString& Error : Errors)
        {
            CompileErrors.Add(Error);
        }
    }
#endif
    
    // Build result
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("emitter_name"), EmitterName);
    Result->SetStringField(TEXT("script_type"), ScriptType);
    Result->SetStringField(TEXT("script_name"), Script->GetName());
    
    // HLSL outputs
    if (!HlslCpu.IsEmpty())
    {
        Result->SetStringField(TEXT("hlsl_cpu"), HlslCpu);
        Result->SetNumberField(TEXT("hlsl_cpu_length"), HlslCpu.Len());
    }
    else
    {
        Result->SetStringField(TEXT("hlsl_cpu"), TEXT(""));
        Result->SetNumberField(TEXT("hlsl_cpu_length"), 0);
    }
    
    if (!HlslGpu.IsEmpty())
    {
        Result->SetStringField(TEXT("hlsl_gpu"), HlslGpu);
        Result->SetNumberField(TEXT("hlsl_gpu_length"), HlslGpu.Len());
    }
    else
    {
        Result->SetStringField(TEXT("hlsl_gpu"), TEXT(""));
        Result->SetNumberField(TEXT("hlsl_gpu_length"), 0);
    }
    
    // Compile errors
    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    for (const FString& Error : CompileErrors)
    {
        ErrorsArray.Add(MakeShareable(new FJsonValueString(Error)));
    }
    Result->SetArrayField(TEXT("compile_errors"), ErrorsArray);
    Result->SetNumberField(TEXT("error_count"), CompileErrors.Num());
    
#else
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Compiled code inspection is only available in editor builds (WITH_EDITORONLY_DATA)"));
#endif
    
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraParticleAttributes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
#if WITH_EDITOR
    // Get component name from params
    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing component_name parameter"));
        return Result;
    }
    
    // Optional parameters
    FString EmitterName;
    Params->TryGetStringField(TEXT("emitter"), EmitterName);
    
    TArray<FString> AttributeNames;
    const TSharedPtr<FJsonObject>* AttrsObj;
    if (Params->TryGetObjectField(TEXT("attributes"), AttrsObj))
    {
        for (const auto& Pair : AttrsObj->Get()->Values)
        {
            AttributeNames.Add(Pair.Key);
        }
    }
    
    int32 FrameIndex = 0;
    Params->TryGetNumberField(TEXT("frame"), FrameIndex);
    
    // Find the Niagara component in the world
    UNiagaraComponent* NiagaraComponent = nullptr;
    
    // Search through all actors in the world
    for (TActorIterator<AActor> It(GWorld); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        
        // Get all Niagara components from this actor
        TArray<UNiagaraComponent*> Components;
        Actor->GetComponents<UNiagaraComponent>(Components);
        
        for (UNiagaraComponent* Comp : Components)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                NiagaraComponent = Comp;
                break;
            }
        }
        
        if (NiagaraComponent)
            break;
    }
    
    if (!NiagaraComponent)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Niagara component not found: %s"), *ComponentName));
        return Result;
    }
    
    // Get or create SimCache
    UNiagaraSimCache* SimCache = NiagaraComponent->GetSimCache();
    bool bCreatedCache = false;
    
    if (!SimCache)
    {
        // Create a new SimCache for capturing
        SimCache = NewObject<UNiagaraSimCache>(GetTransientPackage());
        if (!SimCache)
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to create SimCache"));
            return Result;
        }
        
        // Begin capturing
        FNiagaraSimCacheCreateParameters CreateParams = FNiagaraSimCacheCreateParameters::CreateForDebugging();
        if (!SimCache->BeginWrite(CreateParams, NiagaraComponent))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to begin SimCache write"));
            return Result;
        }
        
        // Write current frame
        if (!SimCache->WriteFrame(NiagaraComponent))
        {
            Result->SetBoolField(TEXT("success"), false);
            Result->SetStringField(TEXT("error"), TEXT("Failed to write frame to SimCache"));
            return Result;
        }
        
        SimCache->EndWrite();
        bCreatedCache = true;
    }
    
    // Get num frames and validate frame index
    int32 NumFrames = SimCache->GetNumFrames();
    if (FrameIndex < 0 || FrameIndex >= NumFrames)
    {
        FrameIndex = FMath::Clamp(FrameIndex, 0, NumFrames - 1);
    }
    
    // Get emitter info
    TArray<FName> EmitterNames = SimCache->GetEmitterNames();
    
    // Build result
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("component_name"), ComponentName);
    Result->SetNumberField(TEXT("num_frames"), NumFrames);
    Result->SetNumberField(TEXT("current_frame"), FrameIndex);
    
    // Emitter data
    TArray<TSharedPtr<FJsonValue>> EmittersArray;
    
    for (int32 EmitterIdx = 0; EmitterIdx < EmitterNames.Num(); ++EmitterIdx)
    {
        FName EmitterFName = EmitterNames[EmitterIdx];
        FString EmitterFNameStr = EmitterFName.ToString();
        
        // Skip if specific emitter requested and this isn't it
        if (!EmitterName.IsEmpty() && EmitterFNameStr != EmitterName)
        {
            continue;
        }
        
        TSharedPtr<FJsonObject> EmitterJson = MakeShareable(new FJsonObject);
        EmitterJson->SetStringField(TEXT("name"), EmitterFNameStr);
        
        int32 NumInstances = SimCache->GetEmitterNumInstances(EmitterIdx, FrameIndex);
        EmitterJson->SetNumberField(TEXT("particle_count"), NumInstances);
        
        // Get attribute list
        TArray<TSharedPtr<FJsonValue>> AttributesList;
        SimCache->ForEachEmitterAttribute(EmitterIdx, [&](const FNiagaraSimCacheVariable& Var) -> bool
        {
            TSharedPtr<FJsonObject> AttrJson = MakeShareable(new FJsonObject);
            FString VarName = Var.Variable.GetName().ToString();
            AttrJson->SetStringField(TEXT("name"), VarName);
            AttrJson->SetNumberField(TEXT("float_count"), Var.FloatCount);
            AttrJson->SetNumberField(TEXT("int_count"), Var.Int32Count);
            AttrJson->SetNumberField(TEXT("half_count"), Var.HalfCount);
            AttributesList.Add(MakeShareable(new FJsonValueObject(AttrJson)));
            return true;
        });
        EmitterJson->SetArrayField(TEXT("attributes"), AttributesList);
        
        // Read particle data
        if (NumInstances > 0)
        {
            TArray<TSharedPtr<FJsonValue>> ParticlesArray;
            
            // Read common attributes
            TArray<FVector> Positions;
            TArray<FLinearColor> Colors;
            TArray<float> Lifetimes;
            TArray<float> Ages;
            TArray<FVector> Velocities;
            
            SimCache->ReadPositionAttribute(Positions, FName("Position"), EmitterFName, true, FrameIndex);
            SimCache->ReadColorAttribute(Colors, FName("Color"), EmitterFName, FrameIndex);
            SimCache->ReadFloatAttribute(Lifetimes, FName("Lifetime"), EmitterFName, FrameIndex);
            SimCache->ReadFloatAttribute(Ages, FName("Age"), EmitterFName, FrameIndex);
            SimCache->ReadVectorAttribute(Velocities, FName("Velocity"), EmitterFName, FrameIndex);
            
            for (int32 i = 0; i < NumInstances; ++i)
            {
                TSharedPtr<FJsonObject> ParticleJson = MakeShareable(new FJsonObject);
                ParticleJson->SetNumberField(TEXT("index"), i);
                
                // Position
                if (Positions.IsValidIndex(i))
                {
                    TArray<TSharedPtr<FJsonValue>> PosArray;
                    PosArray.Add(MakeShareable(new FJsonValueNumber(Positions[i].X)));
                    PosArray.Add(MakeShareable(new FJsonValueNumber(Positions[i].Y)));
                    PosArray.Add(MakeShareable(new FJsonValueNumber(Positions[i].Z)));
                    ParticleJson->SetArrayField(TEXT("position"), PosArray);
                }
                
                // Color
                if (Colors.IsValidIndex(i))
                {
                    TArray<TSharedPtr<FJsonValue>> ColorArray;
                    ColorArray.Add(MakeShareable(new FJsonValueNumber(Colors[i].R)));
                    ColorArray.Add(MakeShareable(new FJsonValueNumber(Colors[i].G)));
                    ColorArray.Add(MakeShareable(new FJsonValueNumber(Colors[i].B)));
                    ColorArray.Add(MakeShareable(new FJsonValueNumber(Colors[i].A)));
                    ParticleJson->SetArrayField(TEXT("color"), ColorArray);
                }
                
                // Lifetime
                if (Lifetimes.IsValidIndex(i))
                {
                    ParticleJson->SetNumberField(TEXT("lifetime"), Lifetimes[i]);
                }
                
                // Age
                if (Ages.IsValidIndex(i))
                {
                    ParticleJson->SetNumberField(TEXT("age"), Ages[i]);
                }
                
                // Velocity
                if (Velocities.IsValidIndex(i))
                {
                    TArray<TSharedPtr<FJsonValue>> VelArray;
                    VelArray.Add(MakeShareable(new FJsonValueNumber(Velocities[i].X)));
                    VelArray.Add(MakeShareable(new FJsonValueNumber(Velocities[i].Y)));
                    VelArray.Add(MakeShareable(new FJsonValueNumber(Velocities[i].Z)));
                    ParticleJson->SetArrayField(TEXT("velocity"), VelArray);
                }
                
                ParticlesArray.Add(MakeShareable(new FJsonValueObject(ParticleJson)));
            }
            
            EmitterJson->SetArrayField(TEXT("particles"), ParticlesArray);
        }
        
        EmittersArray.Add(MakeShareable(new FJsonValueObject(EmitterJson)));
    }
    
    Result->SetArrayField(TEXT("emitters"), EmittersArray);
    
    // Clean up if we created the cache
    if (bCreatedCache && SimCache)
    {
        // Don't set the cache on the component since we just wanted to read
    }
    
#else
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Particle attribute inspection requires editor build"));
#endif
    
    return Result;
}
