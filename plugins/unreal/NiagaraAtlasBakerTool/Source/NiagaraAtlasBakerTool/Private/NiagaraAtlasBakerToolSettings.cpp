#include "NiagaraAtlasBakerToolSettings.h"

#include "NiagaraSystem.h"
#include "Misc/Paths.h"

UNiagaraAtlasBakerToolSettings::UNiagaraAtlasBakerToolSettings()
{
    bUseNiagaraFolderForOutput = true;
    bImportAtlasIntoProject = true;
    bUseNiagaraFolderForImport = true;
    FrameWidth = 512;
    FrameHeight = 512;
    bUseBakerTiming = true;
    StartSeconds = 0.0f;
    DurationSeconds = 1.0f;
    FramesPerSecond = 60;
    bUseBakerGrid = true;
    FramesX = 8;
    FramesY = 8;
    bRenderComponentOnly = true;
    bReplaceExistingAtlasAsset = true;
    bOpenAtlasAfterBake = true;
    CompressionMode = ENiagaraAtlasTextureCompressionMode::PlatformDefault;
    bGenerateMipmaps = true;
    bAllowTextureStreaming = true;
}

FString UNiagaraAtlasBakerToolSettings::ResolveDefaultAtlasAssetName() const
{
    return NiagaraSystem ? FString::Printf(TEXT("T_%s_Atlas"), *NiagaraSystem->GetName()) : TEXT("T_Niagara_Atlas");
}

FString UNiagaraAtlasBakerToolSettings::ResolveDefaultAtlasFileName() const
{
    return NiagaraSystem ? FString::Printf(TEXT("%s_Atlas"), *NiagaraSystem->GetName()) : TEXT("Niagara_Atlas");
}

FString UNiagaraAtlasBakerToolSettings::ResolveDefaultImportPath() const
{
    if (!NiagaraSystem)
    {
        return TEXT("/Game");
    }

    const FString PackageName = NiagaraSystem->GetOutermost()->GetName();
    return FPaths::GetPath(PackageName);
}
