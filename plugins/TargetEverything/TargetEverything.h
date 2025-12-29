#pragma once

#include <ToolboxUIPlugin.h>
#include <IconsFontAwesome5.h>

class TargetEverything : public ToolboxPlugin {
public:
    TargetEverything()
    {
    }
    ~TargetEverything() override = default;

    const char* Name() const override { return "TargetEverything"; }
    const char* Icon() const override { return ICON_FA_BULLSEYE; }

    bool HasSettings() const override { return false; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    bool CanTerminate() override;
    void SignalTerminate() override;
    void Terminate() override;
};
