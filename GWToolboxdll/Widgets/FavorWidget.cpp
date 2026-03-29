#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <Defines.h>
#include <Widgets/FavorWidget.h>
#include <Modules/FavorTrackerModule.h>
#include <Utils/FontLoader.h>

namespace {
    float text_size = 24.f;
}

void FavorWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    if (!FavorTrackerModule::HasFavor()) return;

    const uint32_t minutes = FavorTrackerModule::GetFavorMinutes();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 60.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        char buf[32];
        if (minutes >= 60) {
            snprintf(buf, sizeof(buf), "%uh %um Favor", minutes / 60, minutes % 60);
        }
        else {
            snprintf(buf, sizeof(buf), "%u Minutes Favor", minutes);
        }

        ImGui::PushFont(FontLoader::GetFontByPx(text_size), text_size);
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "%s", buf);
        ImGui::SetCursorPos(cur);
        ImGui::TextColored(ImColor(255, 215, 0), "%s", buf);
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void FavorWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_FLOAT(text_size);
}

void FavorWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_FLOAT(text_size);
}

void FavorWidget::DrawSettingsInternal()
{
    ImGui::DragFloat("Text size", &text_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
}
