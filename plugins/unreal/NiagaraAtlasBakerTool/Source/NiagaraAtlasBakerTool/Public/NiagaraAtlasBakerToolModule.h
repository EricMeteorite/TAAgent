#pragma once

#include "Modules/ModuleManager.h"

class FNiagaraAtlasBakerToolModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void UnregisterMenus();
    void RegisterTabSpawner();
    void UnregisterTabSpawner();
    void OnOpenPluginWindow();

    static const FName TabName;
};
