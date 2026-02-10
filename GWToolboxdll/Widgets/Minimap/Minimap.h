#pragma once

#include <GWCA/GameEntities/Party.h>

#include <ToolboxWidget.h>

#include "Defines.h"

#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/EffectRenderer.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Widgets/Minimap/PingsLinesRenderer.h>
#include <Widgets/Minimap/PmapRenderer.h>
#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/SymbolsRenderer.h>

// Context structure that encapsulates all rendering parameters
struct MinimapRenderContext {
    // Position and size
    DirectX::XMFLOAT2 screen_position; // Top-left corner in screen coordinates
    DirectX::XMFLOAT2 size; // Width and height in pixels (typically square)

    // Camera/view parameters
    GW::Vec2f translation; // World-space translation (for panning)
    float zoom_scale; // Zoom level (scale factor)
    float rotation; // Map rotation in radians

    // Visual options
    bool circular_map; // Whether to render as circle or square
    bool draw_center_marker; // Whether to draw center marker when panned
    D3DCOLOR background_color; // Background color (or 0 to use renderer's default)

    // Clipping rectangle (calculated from screen_position and size)
    RECT clipping_rect;

    // Helper to create context from current widget state
    static MinimapRenderContext FromWidget(const class Minimap& minimap);

    // Helper to calculate clipping rect from position and size
    void UpdateClippingRect()
    {
        clipping_rect.left = static_cast<LONG>(screen_position.x);
        clipping_rect.top = static_cast<LONG>(screen_position.y);
        clipping_rect.right = static_cast<LONG>(screen_position.x + size.x);
        clipping_rect.bottom = static_cast<LONG>(screen_position.y + size.y);
    }
};

class Minimap final : public ToolboxWidget {
    Minimap()
    {
        is_resizable = false;
    }

    ~Minimap() override = default;

public:
    Minimap(const Minimap&) = delete;

    static Minimap& Instance()
    {
        static Minimap instance;
        return instance;
    }

    const int ms_before_back = 1000; // time before we snap back to player
    const float acceleration = 0.5f;
    const float max_speed = 15.0f; // game units per frame

    [[nodiscard]] const char* Name() const override { return "Minimap"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    [[nodiscard]] float Scale() const;

    void DrawHelp() override;
    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;

    // Widget-based rendering (uses internal state)
    void Draw(IDirect3DDevice9* device) override;

    // Static rendering with explicit context (for multiple minimaps)
    static void Render(IDirect3DDevice9* device, const MinimapRenderContext& context);

    // Setup projection matrix for a given context
    static void RenderSetupProjection(IDirect3DDevice9* device, const MinimapRenderContext& context);

    bool FlagHeros(LPARAM lParam);
    bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) const;
    bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
    static void CHAT_CMD_FUNC(OnFlagHeroCmd);
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    [[nodiscard]] float GetMapRotation() const;
    [[nodiscard]] GW::Vec2f ShadowstepLocation() const;

    // 0 is 'all' flag, 1 to 7 is each hero
    static bool FlagHero(uint32_t idx);

    RangeRenderer range_renderer;
    PmapRenderer pmap_renderer;
    AgentRenderer agent_renderer;
    PingsLinesRenderer pingslines_renderer;
    SymbolsRenderer symbols_renderer;
    CustomRenderer custom_renderer;
    EffectRenderer effect_renderer;

    static bool ShouldMarkersDrawOnMap();
    static bool ShouldDrawAllQuests();

    [[nodiscard]] static bool IsActive();

private:
    [[nodiscard]] bool IsInside(int x, int y) const;
    // returns true if the map is visible, valid, not loading, etc

    static void SelectTarget(GW::Vec2f pos);
    static size_t GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags = nullptr);

    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage /*msgid*/, void* /*wParam*/, void*);

    // Get context from current widget state
    [[nodiscard]] MinimapRenderContext GetCurrentContext() const;
};
