#include "NiagaraAtlasBakerToolBaker.h"

#include "NiagaraAtlasBakerToolSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageWrapperHelper.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "NiagaraBakerOutputTexture2D.h"
#include "NiagaraBakerSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"
#include "AdvancedPreviewScene.h"
#include "RenderCore.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"

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

    void SetAbsoluteTime(UNiagaraBakerSettings* BakerSettings, float AbsoluteTime)
    {
        if (!BakerSettings || !PreviewComponent)
        {
            return;
        }

        if (!PreviewComponent->IsActive())
        {
            PreviewComponent->Activate(true);
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

        if (!SceneCaptureComponent)
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
            FPlane(0, 0, 0, 1));
        const FMatrix ViewMatrix =
            SceneCaptureMatrix *
            BakerSettings->GetViewportMatrix().Inverse() *
            FRotationTranslationMatrix(BakerSettings->GetCameraRotation(), BakerSettings->GetCameraLocation());
        SceneCaptureComponent->SetWorldLocationAndRotation(ViewMatrix.GetOrigin(), ViewMatrix.Rotator());
        SceneCaptureComponent->bUseCustomProjectionMatrix = true;
        SceneCaptureComponent->CustomProjectionMatrix = BakerSettings->GetProjectionMatrix();

        if (BakerSettings->bRenderComponentOnly)
        {
            SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            SceneCaptureComponent->ShowOnlyComponents.Empty();
            SceneCaptureComponent->ShowOnlyComponents.Add(PreviewComponent);

            const TArray<TObjectPtr<USceneComponent>>& AttachChildren = PreviewComponent->GetAttachChildren();
            for (USceneComponent* ChildComponent : AttachChildren)
            {
                if (UPrimitiveComponent* PrimitiveChild = Cast<UPrimitiveComponent>(ChildComponent))
                {
                    SceneCaptureComponent->ShowOnlyComponents.Add(PrimitiveChild);
                }
            }
        }
        else
        {
            SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
        }

        SceneCaptureComponent->CaptureScene();
    }

private:
    UNiagaraSystem* NiagaraSystem = nullptr;
    UNiagaraComponent* PreviewComponent = nullptr;
    USceneCaptureComponent2D* SceneCaptureComponent = nullptr;
    TSharedPtr<FAdvancedPreviewScene> PreviewScene;
};

static FIntPoint ResolveFrameSize(UNiagaraBakerSettings* BakerSettings, int32 RequestedWidth, int32 RequestedHeight)
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

    return FIntPoint(FMath::Max(1, Width), FMath::Max(1, Height));
}

static UTextureRenderTarget2D* CreateRenderTarget(const FIntPoint& FrameSize)
{
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
    RenderTarget->ClearColor = FLinearColor::Transparent;
    RenderTarget->InitCustomFormat(FrameSize.X, FrameSize.Y, PF_FloatRGBA, false);
    RenderTarget->UpdateResourceImmediate(true);
    return RenderTarget;
}

static bool ReadPixels(UTextureRenderTarget2D* RenderTarget, TArray<FFloat16Color>& OutPixels)
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

static float ResolveMergedOpacity(const FLinearColor& BeautyColor, const FLinearColor& AlphaColor)
{
    const float CapturedOpacity = FMath::Clamp(1.0f - AlphaColor.A, 0.0f, 1.0f);
    // Some Niagara materials render visible RGB but write a very weak scene alpha.
    // Use visible color intensity as a lower bound so the imported atlas remains visible in UE.
    const float VisibleOpacity = FMath::Clamp(FMath::Max3(BeautyColor.R, BeautyColor.G, BeautyColor.B), 0.0f, 1.0f);
    return FMath::Max(CapturedOpacity, VisibleOpacity);
}

static void MergeBeautyAndAlphaPixels(const TArray<FFloat16Color>& BeautyPixels, const TArray<FFloat16Color>& AlphaPixels, TArray<FFloat16Color>& OutMergedPixels)
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
            ResolveMergedOpacity(BeautyColor, AlphaColor)));
    }
}

static void CopyFrameIntoAtlas(const TArray<FFloat16Color>& FramePixels, const FIntPoint& FrameSize, const FIntPoint& AtlasGrid, int32 FrameIndex, TArray<FFloat16Color>& AtlasPixels)
{
    const int32 AtlasWidth = FrameSize.X * AtlasGrid.X;
    const int32 TileX = FrameIndex % AtlasGrid.X;
    const int32 TileY = FrameIndex / AtlasGrid.X;

    for (int32 Row = 0; Row < FrameSize.Y; ++Row)
    {
        const int32 SourceIndex = Row * FrameSize.X;
        const int32 DestIndex = ((TileY * FrameSize.Y) + Row) * AtlasWidth + (TileX * FrameSize.X);
        FMemory::Memcpy(&AtlasPixels[DestIndex], &FramePixels[SourceIndex], sizeof(FFloat16Color) * FrameSize.X);
    }
}

static void ConvertAtlasPixelsToSrgb8(TArrayView<const FFloat16Color> ImageData, TArray<FColor>& OutPixels)
{
    OutPixels.Reset();
    OutPixels.Reserve(ImageData.Num());
    for (const FFloat16Color& HalfColor : ImageData)
    {
        OutPixels.Add(HalfColor.GetFloats().ToFColor(true));
    }
}

static bool ExportAtlasImage(const FString& FilePath, const FIntPoint& ImageSize, TArrayView<FFloat16Color> ImageData)
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

    TArray<FColor> TempImageData;
    ConvertAtlasPixelsToSrgb8(ImageData, TempImageData);

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

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed();
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

static void ApplyImportedTextureSettings(UTexture2D* Texture, const UNiagaraAtlasBakerToolSettings* Settings)
{
    check(Texture);

    const ENiagaraAtlasTextureCompressionMode CompressionMode = Settings
        ? Settings->CompressionMode
        : ENiagaraAtlasTextureCompressionMode::PlatformDefault;

    Texture->SRGB = true;
    Texture->Filter = TF_Default;
    Texture->AddressX = TA_Wrap;
    Texture->AddressY = TA_Wrap;
    Texture->CompressionSettings = TextureCompressionSettings::TC_Default;
    Texture->CompressionNone = false;
    Texture->LODGroup = TEXTUREGROUP_World;

    switch (CompressionMode)
    {
    case ENiagaraAtlasTextureCompressionMode::HighQualityBC7:
        Texture->CompressionSettings = TextureCompressionSettings::TC_BC7;
        break;

    case ENiagaraAtlasTextureCompressionMode::Lossless:
        Texture->CompressionNone = true;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->NeverStream = true;
        return;

    case ENiagaraAtlasTextureCompressionMode::PlatformDefault:
    default:
        break;
    }

    const bool bGenerateMipmaps = Settings ? Settings->bGenerateMipmaps : true;
    const bool bAllowTextureStreaming = Settings ? Settings->bAllowTextureStreaming : true;

    Texture->MipGenSettings = bGenerateMipmaps ? TMGS_FromTextureGroup : TMGS_NoMipmaps;
    Texture->NeverStream = !bGenerateMipmaps || !bAllowTextureStreaming;
}

static bool FinalizeImportedTextureAsset(UTexture2D* Texture, const FString& AssetPath, FText& OutError)
{
    check(Texture);

    Texture->SetDeterministicLightingGuid();
    Texture->MarkPackageDirty();
    Texture->PostEditChange();
    Texture->UpdateResource();

    if (!UEditorAssetLibrary::SaveLoadedAsset(Texture, false))
    {
        OutError = FText::FromString(FString::Printf(TEXT("Atlas imported but failed to save: %s"), *AssetPath));
        return false;
    }

    return true;
}

static bool ImportAtlasTexture(
    const UNiagaraAtlasBakerToolSettings* Settings,
    const FIntPoint& ImageSize,
    TArrayView<const FFloat16Color> ImageData,
    const FString& DestinationPath,
    const FString& AssetName,
    bool bReplaceExisting,
    UTexture2D*& OutTexture,
    FText& OutError)
{
    const FString NormalizedDestinationPath = DestinationPath.EndsWith(TEXT("/")) ? DestinationPath.LeftChop(1) : DestinationPath;
    const FString AssetPath = NormalizedDestinationPath + TEXT("/") + AssetName;
    TArray<FColor> TexturePixels;
    ConvertAtlasPixelsToSrgb8(ImageData, TexturePixels);

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        if (!bReplaceExisting)
        {
            OutError = FText::FromString(FString::Printf(TEXT("Atlas asset already exists: %s"), *AssetPath));
            return false;
        }

        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (ExistingAsset && GEditor)
        {
            if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                AssetEditorSubsystem->CloseAllEditorsForAsset(ExistingAsset);
            }
        }

        UTexture2D* ExistingTexture = Cast<UTexture2D>(ExistingAsset);
        if (!ExistingTexture)
        {
            OutError = FText::FromString(FString::Printf(TEXT("Existing asset is not a Texture2D and cannot be updated safely: %s"), *AssetPath));
            return false;
        }

        ExistingTexture->Modify();
        ExistingTexture->PreEditChange(nullptr);
        ExistingTexture->Source.Init(ImageSize.X, ImageSize.Y, 1, 1, ETextureSourceFormat::TSF_BGRA8, reinterpret_cast<const uint8*>(TexturePixels.GetData()));
        ApplyImportedTextureSettings(ExistingTexture, Settings);

        if (!FinalizeImportedTextureAsset(ExistingTexture, AssetPath, OutError))
        {
            return false;
        }

        OutTexture = ExistingTexture;
        return true;
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        OutError = FText::FromString(FString::Printf(TEXT("Failed to create package: %s"), *AssetPath));
        return false;
    }

    Package->FullyLoad();

    OutTexture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone);

    if (!OutTexture)
    {
        OutError = FText::FromString(FString::Printf(TEXT("Failed to create atlas texture asset: %s"), *AssetPath));
        return false;
    }

    OutTexture->Source.Init(ImageSize.X, ImageSize.Y, 1, 1, ETextureSourceFormat::TSF_BGRA8, reinterpret_cast<const uint8*>(TexturePixels.GetData()));
    ApplyImportedTextureSettings(OutTexture, Settings);

    FAssetRegistryModule::AssetCreated(OutTexture);
    if (!FinalizeImportedTextureAsset(OutTexture, AssetPath, OutError))
    {
        return false;
    }

    return true;
}
}

bool FNiagaraAtlasBakerToolBaker::BakeAtlas(const UNiagaraAtlasBakerToolSettings* Settings, FNiagaraAtlasBakeResult& OutResult, FText& OutError)
{
    if (!ValidateSettings(Settings, OutError))
    {
        return false;
    }

    UNiagaraSystem* NiagaraSystem = Settings->NiagaraSystem;
    UNiagaraBakerSettings* BakerSettings = NiagaraSystem->GetBakerSettings();
    if (!BakerSettings)
    {
        OutError = FText::FromString(TEXT("The selected Niagara System does not have baker settings."));
        return false;
    }

    FString OutputDirectory;
    if (!ResolveOutputDirectory(Settings, OutputDirectory, OutError))
    {
        return false;
    }

    if (!IFileManager::Get().DirectoryExists(*OutputDirectory) && !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
    {
        OutError = FText::FromString(FString::Printf(TEXT("Failed to create output directory: %s"), *OutputDirectory));
        return false;
    }

    const int32 FramesX = Settings->bUseBakerGrid ? BakerSettings->FramesPerDimension.X : Settings->FramesX;
    const int32 FramesY = Settings->bUseBakerGrid ? BakerSettings->FramesPerDimension.Y : Settings->FramesY;
    const float StartSeconds = Settings->bUseBakerTiming ? BakerSettings->StartSeconds : Settings->StartSeconds;
    const float DurationSeconds = Settings->bUseBakerTiming ? BakerSettings->DurationSeconds : Settings->DurationSeconds;
    const int32 FramesPerSecond = Settings->bUseBakerTiming ? BakerSettings->FramesPerSecond : Settings->FramesPerSecond;
    const FIntPoint FrameSize = ResolveFrameSize(BakerSettings, Settings->FrameWidth, Settings->FrameHeight);
    const FIntPoint AtlasGrid(FramesX, FramesY);
    const FIntPoint AtlasSize(FrameSize.X * AtlasGrid.X, FrameSize.Y * AtlasGrid.Y);
    const int32 TotalFrames = FramesX * FramesY;

    if (AtlasSize.X > 16384 || AtlasSize.Y > 16384)
    {
        OutError = FText::FromString(FString::Printf(TEXT("Atlas resolution %dx%d is too large. Reduce frame size or atlas grid."), AtlasSize.X, AtlasSize.Y));
        return false;
    }

    const FString AtlasFilePath = FPaths::Combine(OutputDirectory, ResolveAtlasFileName(Settings) + TEXT(".png"));

    FScopedBakerSettingsRestore ScopedRestore(BakerSettings);
    BakerSettings->StartSeconds = StartSeconds;
    BakerSettings->DurationSeconds = DurationSeconds;
    BakerSettings->FramesPerSecond = FramesPerSecond;
    BakerSettings->FramesPerDimension = AtlasGrid;
    BakerSettings->bRenderComponentOnly = Settings->bRenderComponentOnly;
    BakerSettings->bLockToSimulationFrameRate = false;

    UTextureRenderTarget2D* BeautyRenderTarget = CreateRenderTarget(FrameSize);
    UTextureRenderTarget2D* AlphaRenderTarget = CreateRenderTarget(FrameSize);
    if (!BeautyRenderTarget || !AlphaRenderTarget)
    {
        OutError = FText::FromString(TEXT("Failed to allocate bake render targets."));
        return false;
    }

    FLocalNiagaraBakeRenderer BakeRenderer(NiagaraSystem);
    TArray<FFloat16Color> BeautyPixels;
    TArray<FFloat16Color> AlphaPixels;
    TArray<FFloat16Color> MergedPixels;
    TArray<FFloat16Color> AtlasPixels;
    AtlasPixels.Init(FFloat16Color(FLinearColor::Transparent), AtlasSize.X * AtlasSize.Y);

    const float FrameDeltaSeconds = DurationSeconds / static_cast<float>(TotalFrames);
    FScopedSlowTask SlowTask(static_cast<float>(TotalFrames + 1), FText::FromString(TEXT("Baking Niagara atlas...")));
    SlowTask.MakeDialog(true);

    for (int32 FrameIndex = 0; FrameIndex < TotalFrames; ++FrameIndex)
    {
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Baking frame %d / %d"), FrameIndex + 1, TotalFrames)));
        if (SlowTask.ShouldCancel())
        {
            OutError = FText::FromString(TEXT("Bake cancelled."));
            return false;
        }

        const float FrameTime = StartSeconds + (static_cast<float>(FrameIndex) * FrameDeltaSeconds);
        BakeRenderer.SetAbsoluteTime(BakerSettings, FrameTime);
        BakeRenderer.RenderSceneCapture(BakerSettings, BeautyRenderTarget, ESceneCaptureSource::SCS_FinalToneCurveHDR);
        BakeRenderer.RenderSceneCapture(BakerSettings, AlphaRenderTarget, ESceneCaptureSource::SCS_SceneColorHDR);

        if (!ReadPixels(BeautyRenderTarget, BeautyPixels))
        {
            OutError = FText::FromString(FString::Printf(TEXT("Failed to read beauty pixels for frame %d."), FrameIndex));
            return false;
        }
        if (!ReadPixels(AlphaRenderTarget, AlphaPixels))
        {
            OutError = FText::FromString(FString::Printf(TEXT("Failed to read alpha pixels for frame %d."), FrameIndex));
            return false;
        }
        if (BeautyPixels.Num() != AlphaPixels.Num())
        {
            OutError = FText::FromString(FString::Printf(TEXT("Beauty/alpha pixel count mismatch on frame %d."), FrameIndex));
            return false;
        }

        MergeBeautyAndAlphaPixels(BeautyPixels, AlphaPixels, MergedPixels);
        CopyFrameIntoAtlas(MergedPixels, FrameSize, AtlasGrid, FrameIndex, AtlasPixels);
    }

    SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Writing atlas image...")));
    if (!ExportAtlasImage(AtlasFilePath, AtlasSize, AtlasPixels))
    {
        OutError = FText::FromString(FString::Printf(TEXT("Failed to export atlas image: %s"), *AtlasFilePath));
        return false;
    }

    OutResult.OutputFilePath = AtlasFilePath;
    OutResult.FrameSize = FrameSize;
    OutResult.AtlasGrid = AtlasGrid;
    OutResult.AtlasSize = AtlasSize;
    OutResult.FrameCount = TotalFrames;

    if (Settings->bImportAtlasIntoProject)
    {
        const FString DestinationPath = ResolveImportDestinationPath(Settings);
        UTexture2D* ImportedTexture = nullptr;
        if (!ImportAtlasTexture(
            Settings,
            AtlasSize,
            AtlasPixels,
                DestinationPath,
                ResolveAtlasAssetName(Settings),
                Settings->bReplaceExistingAtlasAsset,
                ImportedTexture,
                OutError))
        {
            return false;
        }

        OutResult.ImportedTexture = ImportedTexture;
        OutResult.ImportedAssetPath = DestinationPath + TEXT("/") + ResolveAtlasAssetName(Settings);

        if (Settings->bOpenAtlasAfterBake && GEditor)
        {
            if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                AssetEditorSubsystem->OpenEditorForAsset(ImportedTexture);
            }
        }
    }

    return true;
}

bool FNiagaraAtlasBakerToolBaker::ValidateSettings(const UNiagaraAtlasBakerToolSettings* Settings, FText& OutError)
{
    if (!Settings)
    {
        OutError = FText::FromString(TEXT("Tool settings are not available."));
        return false;
    }

    if (!Settings->NiagaraSystem)
    {
        OutError = FText::FromString(TEXT("Please select a Niagara System."));
        return false;
    }

    if (Settings->FrameWidth <= 0 || Settings->FrameHeight <= 0)
    {
        OutError = FText::FromString(TEXT("Frame width and height must be greater than 0."));
        return false;
    }

    if (!Settings->bUseBakerTiming)
    {
        if (Settings->DurationSeconds <= 0.0f)
        {
            OutError = FText::FromString(TEXT("DurationSeconds must be greater than 0."));
            return false;
        }

        if (Settings->FramesPerSecond <= 0)
        {
            OutError = FText::FromString(TEXT("FramesPerSecond must be greater than 0."));
            return false;
        }
    }

    if (!Settings->bUseBakerGrid && (Settings->FramesX <= 0 || Settings->FramesY <= 0))
    {
        OutError = FText::FromString(TEXT("FramesX and FramesY must be greater than 0."));
        return false;
    }

    if (Settings->bImportAtlasIntoProject && !Settings->bUseNiagaraFolderForImport)
    {
        if (Settings->ImportDestinationPath.IsEmpty() || !Settings->ImportDestinationPath.StartsWith(TEXT("/")))
        {
            OutError = FText::FromString(TEXT("ImportDestinationPath must be a valid Unreal asset path, for example /Game/VFX/Baked."));
            return false;
        }
    }

    return true;
}

bool FNiagaraAtlasBakerToolBaker::ResolveOutputDirectory(const UNiagaraAtlasBakerToolSettings* Settings, FString& OutDirectory, FText& OutError)
{
    if (Settings->bUseNiagaraFolderForOutput)
    {
        const FString PackageFolder = FPaths::GetPath(Settings->NiagaraSystem->GetOutermost()->GetName());
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackageFolder, OutDirectory, TEXT("")))
        {
            OutError = FText::FromString(TEXT("Failed to resolve the Niagara asset folder on disk."));
            return false;
        }
        return true;
    }

    if (Settings->OutputDirectory.Path.IsEmpty())
    {
        OutError = FText::FromString(TEXT("Please choose an output directory."));
        return false;
    }

    OutDirectory = FPaths::ConvertRelativePathToFull(Settings->OutputDirectory.Path);
    return true;
}

FString FNiagaraAtlasBakerToolBaker::ResolveImportDestinationPath(const UNiagaraAtlasBakerToolSettings* Settings)
{
    FString DestinationPath = Settings->bUseNiagaraFolderForImport
        ? Settings->ResolveDefaultImportPath()
        : (Settings->ImportDestinationPath.IsEmpty() ? Settings->ResolveDefaultImportPath() : Settings->ImportDestinationPath);

    while (DestinationPath.EndsWith(TEXT("/")))
    {
        DestinationPath.LeftChopInline(1);
    }

    return DestinationPath;
}

FString FNiagaraAtlasBakerToolBaker::ResolveAtlasFileName(const UNiagaraAtlasBakerToolSettings* Settings)
{
    const FString RawName = Settings->AtlasFileName.IsEmpty() ? Settings->ResolveDefaultAtlasFileName() : Settings->AtlasFileName;
    return FPaths::GetBaseFilename(RawName);
}

FString FNiagaraAtlasBakerToolBaker::ResolveAtlasAssetName(const UNiagaraAtlasBakerToolSettings* Settings)
{
    return Settings->AtlasAssetName.IsEmpty() ? Settings->ResolveDefaultAtlasAssetName() : Settings->AtlasAssetName;
}
