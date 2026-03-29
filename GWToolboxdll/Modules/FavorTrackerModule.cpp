#include "stdafx.h"

#include <Defines.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <Modules/FavorTrackerModule.h>
#include <Modules/AudioSettings.h>
#include <ImGuiAddons.h>
#include <Timer.h>
#include <Utils/ToolboxUtils.h>

namespace {
    bool enabled = true;
    bool play_sound_on_favor = true;
    char favor_sound_id[64] = "b3e00101";
    int poll_interval_seconds = 60;

    uint32_t favor_minutes = 0;
    bool favor_active = false;
    clock_t last_poll_time = 0;
    clock_t suppress_until = 0;

    GW::HookEntry OnMessageServer_Entry;
    GW::HookEntry OnUIMessage_Entry;
    
    std::wstring HexToWString(const char* hex)
    {
        std::wstring result;
        const size_t len = strlen(hex);
        for (size_t i = 0; i + 3 < len; i += 4) {
            char word[5] = { hex[i], hex[i+1], hex[i+2], hex[i+3], '\0' };
            result.push_back(static_cast<wchar_t>(strtoul(word, nullptr, 16)));
        }
        return result;
    }

    uint32_t ExtractEncodedValue(const wchar_t* msg)
    {
        if (!msg) return 0;
        for (size_t i = 0; msg[i]; i++) {
            if (msg[i] == 0x101 && msg[i + 1] >= 0x100) {
                return msg[i + 1] - 0x100;
            }
        }
        return 0;
    }

    void SetFavorActive(bool active, uint32_t minutes = 0)
    {
        const bool was_active = favor_active;
        favor_active = active;
        favor_minutes = minutes;

        if (active && !was_active && play_sound_on_favor) {
            const auto sound = HexToWString(favor_sound_id);
            if (!sound.empty()) {
                AudioSettings::PlaySound(sound.c_str());
            }
        }
    }

    bool ParseFavorMessage(const wchar_t* msg)
    {
        if (!msg || !*msg) return false;

        // "x minutes of favor of the gods remaining" as a result of /favor command
        if (msg[0] == 0x8102 && msg[1] == 0x223F) {
            const auto mins = ExtractEncodedValue(msg);
            SetFavorActive(mins > 0, mins);
            return true;
        }

        if (msg[0] == 0x8101) {
            switch (msg[1]) {
                // "x minutes of favor of the gods remaining" (broadcast)
                case 0x7B91: {
                    const auto mins = ExtractEncodedValue(msg);
                    SetFavorActive(mins > 0, mins);
                    return true;
                }
                // "x more achievements must be performed to earn the favor of the gods" (broadcast)
                case 0x7B92:
                    SetFavorActive(false);
                    return true;
            }
        }

        if (msg[0] == 0x8102) {
            switch (msg[1]) {
                // "The gods have blessed the world with their favor"
                case 0x23E3:
                    SetFavorActive(true, favor_minutes);
                    return true;

                // "The world no longer has the favor of the gods"
                case 0x23E4:
                    SetFavorActive(false);
                    return true;
            }
        }

        return false;
    }
    
    void OnMessageServer(GW::HookStatus*, GW::Packet::StoC::MessageServer*)
    {
        const wchar_t* msg = ToolboxUtils::GetMessageCore();
        ParseFavorMessage(msg);
    }
    
    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (!suppress_until || TIMER_INIT() > suppress_until) return;
        if (status->blocked) return;

        GW::Chat::Channel channel = GW::Chat::Channel::CHANNEL_UNKNOW;

        switch (message_id) {
            case GW::UI::UIMessage::kWriteToChatLog: {
                const auto packet = static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam);
                channel = packet->channel;
            } break;
            case GW::UI::UIMessage::kPrintChatMessage: {
                const auto packet = static_cast<GW::UI::UIPacket::kPrintChatMessage*>(wparam);
                channel = packet->channel;
            } break;
            case GW::UI::UIMessage::kLogChatMessage: {
                const auto packet = static_cast<GW::UI::UIPacket::kLogChatMessage*>(wparam);
                channel = packet->channel;
            } break;
            default:
                return;
        }

        if (channel == GW::Chat::Channel::CHANNEL_GLOBAL) {
            status->blocked = true;
        }
    }

}

void FavorTrackerModule::Initialize()
{
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&OnMessageServer_Entry, OnMessageServer);

    constexpr GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kPrintChatMessage,
        GW::UI::UIMessage::kLogChatMessage,
        GW::UI::UIMessage::kWriteToChatLog
    };
    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_Entry, message_id, OnUIMessage, -0x8000);
    }
}

void FavorTrackerModule::Update(float)
{
    if (!enabled) return;
    if (!GW::Map::GetIsMapLoaded()) return;

    if (TIMER_DIFF(last_poll_time) >= poll_interval_seconds * 1000) {
        last_poll_time = TIMER_INIT();
        suppress_until = TIMER_INIT() + 2000;
        GW::Chat::SendChat('/', L"favor");
    }
}

void FavorTrackerModule::SignalTerminate()
{
    ToolboxModule::SignalTerminate();

    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&OnMessageServer_Entry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}

uint32_t FavorTrackerModule::GetFavorMinutes()
{
    return favor_minutes;
}

bool FavorTrackerModule::HasFavor()
{
    return favor_active;
}

void FavorTrackerModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(enabled);
    LOAD_BOOL(play_sound_on_favor);
    const char* sound = ini->GetValue(Name(), "favor_sound_id", favor_sound_id);
    if (sound) {
        strncpy(favor_sound_id, sound, sizeof(favor_sound_id) - 1);
        favor_sound_id[sizeof(favor_sound_id) - 1] = '\0';
    }
    poll_interval_seconds = ini->GetLongValue(Name(), "poll_interval_seconds", poll_interval_seconds);
}

void FavorTrackerModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(enabled);
    SAVE_BOOL(play_sound_on_favor);
    ini->SetValue(Name(), "favor_sound_id", favor_sound_id);
    ini->SetLongValue(Name(), "poll_interval_seconds", poll_interval_seconds);
}

void FavorTrackerModule::DrawSettingsInternal()
{
    ImGui::Checkbox("Enable Favor Tracking", &enabled);
    ImGui::ShowHelp("Periodically runs /favor to check Favor of the Gods status. Automated queries are hidden from chat.");

    ImGui::Checkbox("Play sound on favor activation", &play_sound_on_favor);
    if (play_sound_on_favor) {
        ImGui::InputText("Sound ID (hex)", favor_sound_id, sizeof(favor_sound_id));
        ImGui::ShowHelp("Hex-encoded GW sound ID from the Audio Settings sound log. Default is b3e00101 (level-up sound). Set to empty to disable");
    }
    ImGui::InputInt("Poll Interval (seconds)", &poll_interval_seconds);
    if (poll_interval_seconds < 10) poll_interval_seconds = 10;

    ImGui::Separator();
    ImGui::Text("Favor Status: %s", favor_active ? "Active" : "Inactive");
    if (favor_active) {
        ImGui::Text("Minutes Remaining: %u", favor_minutes);
    }

    if (ImGui::Button("Check Now")) {
        GW::Chat::SendChat('/', L"favor");
    }
}
