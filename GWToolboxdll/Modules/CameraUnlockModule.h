#pragma once

#include <ToolboxModule.h>

class ToolboxIni;

class CameraUnlockModule : public ToolboxModule {
    CameraUnlockModule() = default;
    ~CameraUnlockModule() = default;

public:
    static CameraUnlockModule& Instance()
    {
        static CameraUnlockModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Camera Unlock"; }
    [[nodiscard]] const char* Description() const override { return "Allows free-roaming camera functionality via /cam unlock"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CAMERA; }
    void Initialize() override;

    void Terminate() override;

    void Update(float) override;

    void LoadSettings(ToolboxIni*) override;

    void SaveSettings(ToolboxIni*) override;

    void DrawSettingsInternal() override;

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

};
