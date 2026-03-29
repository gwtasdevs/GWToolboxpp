#pragma once

#include <ToolboxWidget.h>

class FavorWidget : public ToolboxWidget {
    FavorWidget() = default;
    ~FavorWidget() override = default;

public:
    static FavorWidget& Instance()
    {
        static FavorWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Favor"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PRAY; }

    void Draw(IDirect3DDevice9* pDevice) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
