#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NiagaraAtlasBakerToolSettings.generated.h"

class UNiagaraSystem;

UENUM()
enum class ENiagaraAtlasTextureCompressionMode : uint8
{
    PlatformDefault UMETA(DisplayName = "Platform Default Compressed"),
    HighQualityBC7 UMETA(DisplayName = "High Quality BC7 (Desktop)"),
    Lossless UMETA(DisplayName = "Lossless BGRA8")
};

UCLASS(Transient)
class NIAGARAATLASBAKERTOOL_API UNiagaraAtlasBakerToolSettings : public UObject
{
    GENERATED_BODY()

public:
    UNiagaraAtlasBakerToolSettings();

    UPROPERTY(EditAnywhere, Category = "Niagara", meta = (DisplayName = "Niagara System"))
    TObjectPtr<UNiagaraSystem> NiagaraSystem;

    UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Use Niagara Folder"))
    bool bUseNiagaraFolderForOutput;

    UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Output Directory", RelativeToGameDir))
    FDirectoryPath OutputDirectory;

    UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Atlas File Name"))
    FString AtlasFileName;

    UPROPERTY(EditAnywhere, Category = "Import", meta = (DisplayName = "Import Atlas Into Project"))
    bool bImportAtlasIntoProject;

    UPROPERTY(EditAnywhere, Category = "Import", meta = (DisplayName = "Use Niagara Asset Folder", EditCondition = "bImportAtlasIntoProject", ToolTip = "When enabled, the atlas is always imported into the selected Niagara asset's folder and any custom import path is ignored."))
    bool bUseNiagaraFolderForImport;

    UPROPERTY(EditAnywhere, Category = "Import", meta = (DisplayName = "Import Destination Path", EditCondition = "bImportAtlasIntoProject && bUseNiagaraFolderForImport == false", ToolTip = "Unreal asset path, e.g. /Game/VFX/Baked. This is only used when Use Niagara Asset Folder is disabled."))
    FString ImportDestinationPath;

    UPROPERTY(EditAnywhere, Category = "Import", meta = (DisplayName = "Atlas Asset Name"))
    FString AtlasAssetName;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (ClampMin = "1", UIMin = "1"))
    int32 FrameWidth;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (ClampMin = "1", UIMin = "1"))
    int32 FrameHeight;

    UPROPERTY(EditAnywhere, Category = "Bake")
    bool bUseBakerTiming;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (EditCondition = "bUseBakerTiming == false"))
    float StartSeconds;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (EditCondition = "bUseBakerTiming == false", ClampMin = "0.001"))
    float DurationSeconds;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (EditCondition = "bUseBakerTiming == false", ClampMin = "1", UIMin = "1"))
    int32 FramesPerSecond;

    UPROPERTY(EditAnywhere, Category = "Bake")
    bool bUseBakerGrid;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (EditCondition = "bUseBakerGrid == false", ClampMin = "1", UIMin = "1"))
    int32 FramesX;

    UPROPERTY(EditAnywhere, Category = "Bake", meta = (EditCondition = "bUseBakerGrid == false", ClampMin = "1", UIMin = "1"))
    int32 FramesY;

    UPROPERTY(EditAnywhere, Category = "Bake")
    bool bRenderComponentOnly;

    UPROPERTY(EditAnywhere, Category = "Import")
    bool bReplaceExistingAtlasAsset;

    UPROPERTY(EditAnywhere, Category = "Import")
    bool bOpenAtlasAfterBake;

    UPROPERTY(EditAnywhere, Category = "Import Optimization", meta = (DisplayName = "Compression Mode", ToolTip = "Platform Default matches the usual DXT-style compressed texture workflow. BC7 keeps the same memory footprint with higher desktop quality. Lossless keeps the uncompressed fallback for debugging or artifact-sensitive effects."))
    ENiagaraAtlasTextureCompressionMode CompressionMode;

    UPROPERTY(EditAnywhere, Category = "Import Optimization", meta = (DisplayName = "Generate Mipmaps", ToolTip = "Enable mip generation so the atlas can stream and reduce runtime memory pressure. Disable this if you see frame bleeding in the distance."))
    bool bGenerateMipmaps;

    UPROPERTY(EditAnywhere, Category = "Import Optimization", meta = (DisplayName = "Allow Texture Streaming", EditCondition = "bGenerateMipmaps", ToolTip = "Allow the imported atlas to participate in texture streaming. Requires mipmaps."))
    bool bAllowTextureStreaming;

    FString ResolveDefaultAtlasAssetName() const;
    FString ResolveDefaultAtlasFileName() const;
    FString ResolveDefaultImportPath() const;
};
