#pragma once

#include "CoreMinimal.h"

class UNiagaraAtlasBakerToolSettings;
class UNiagaraSystem;
class UTexture2D;

struct FNiagaraAtlasBakeResult
{
    FString OutputFilePath;
    FString ImportedAssetPath;
    FIntPoint FrameSize = FIntPoint::ZeroValue;
    FIntPoint AtlasGrid = FIntPoint::ZeroValue;
    FIntPoint AtlasSize = FIntPoint::ZeroValue;
    int32 FrameCount = 0;
    UTexture2D* ImportedTexture = nullptr;
};

class FNiagaraAtlasBakerToolBaker
{
public:
    static bool BakeAtlas(const UNiagaraAtlasBakerToolSettings* Settings, FNiagaraAtlasBakeResult& OutResult, FText& OutError);

private:
    static bool ValidateSettings(const UNiagaraAtlasBakerToolSettings* Settings, FText& OutError);
    static bool ResolveOutputDirectory(const UNiagaraAtlasBakerToolSettings* Settings, FString& OutDirectory, FText& OutError);
    static FString ResolveImportDestinationPath(const UNiagaraAtlasBakerToolSettings* Settings);
    static FString ResolveAtlasFileName(const UNiagaraAtlasBakerToolSettings* Settings);
    static FString ResolveAtlasAssetName(const UNiagaraAtlasBakerToolSettings* Settings);
};
