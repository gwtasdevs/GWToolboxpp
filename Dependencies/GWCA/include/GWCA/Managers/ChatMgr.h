#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct Module;
    extern Module ChatModule;

    struct HookStatus;
    struct HookEntry;

    namespace Chat {
        typedef uint32_t Color;
#pragma warning(push)
#pragma warning(disable: 4200)
        struct ChatMessage {
            uint32_t channel;
            uint32_t unk1;
            FILETIME timestamp;
            wchar_t message[0];
        };
#pragma warning(pop)

        const size_t CHAT_LOG_LENGTH = 0x200;
        struct ChatBuffer {
            uint32_t next;
            uint32_t unk1;
            uint32_t unk2;
            ChatMessage* messages[CHAT_LOG_LENGTH];
        };

        enum Channel : int {
            CHANNEL_ALLIANCE = 0,
            CHANNEL_ALLIES = 1,
            CHANNEL_GWCA1 = 2,
            CHANNEL_ALL = 3,
            CHANNEL_GWCA2 = 4,
            CHANNEL_MODERATOR = 5,
            CHANNEL_EMOTE = 6,
            CHANNEL_WARNING = 7,
            CHANNEL_GWCA3 = 8,
            CHANNEL_GUILD = 9,
            CHANNEL_GLOBAL = 10,
            CHANNEL_GROUP = 11,
            CHANNEL_TRADE = 12,
            CHANNEL_ADVISORY = 13,
            CHANNEL_WHISPER = 14,
            CHANNEL_COUNT,
            CHANNEL_COMMAND,
            CHANNEL_UNKNOW = -1
        };

        GWCA_API Chat::ChatBuffer* GetChatLog();
        GWCA_API bool AddToChatLog(wchar_t* message, uint32_t channel);
        GWCA_API bool GetIsTyping();

        GWCA_API bool SendChat(Channel channel, const wchar_t* msg);
        GWCA_API bool SendWhisper(const wchar_t* to, const wchar_t* msg);

        GWCA_API void WriteChatF(Channel channel, const wchar_t* format, ...);
        GWCA_API void WriteChat(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);
        GWCA_API void WriteChatEnc(Channel channel, const wchar_t* message, const wchar_t* sender = nullptr, bool transient = false);

        typedef void(__cdecl* ChatCommandCallback)(GW::HookStatus*, const wchar_t* cmd, int argc, const LPWSTR* argv);
        GWCA_API void CreateCommand(GW::HookEntry* entry, const wchar_t* cmd, ChatCommandCallback callback);
        GWCA_API void DeleteCommand(GW::HookEntry* entry, const wchar_t* cmd = 0);

        GWCA_API void ToggleTimestamps(bool enable);
        GWCA_API void SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds = false);
        GWCA_API void SetTimestampsColor(Color color);

        GWCA_API Color SetSenderColor(Channel chan, Color col);
        GWCA_API Color SetMessageColor(Channel chan, Color col);
        GWCA_API void  GetChannelColors(Channel chan, Color* sender, Color* message);
        GWCA_API void  GetDefaultColors(Channel chan, Color* sender, Color* message);

        GWCA_API Channel GetChannel(wchar_t opcode);
        GWCA_API Channel GetChannel(char opcode);
    };
}

// ============================================================
// C Interop API
// ============================================================
extern "C" {
    GWCA_API void* GetChatLog();
    GWCA_API bool     AddToChatLog(wchar_t* message, uint32_t channel);
    GWCA_API bool     GetIsTyping();
    GWCA_API bool     SendChat(int channel, const wchar_t* message);
    GWCA_API bool     SendWhisper(const wchar_t* to, const wchar_t* message);
    GWCA_API void     WriteChat(int channel, const wchar_t* message, const wchar_t* sender, bool transient);
    GWCA_API void     WriteChatEnc(int channel, const wchar_t* message, const wchar_t* sender, bool transient);
    GWCA_API void     ToggleTimestamps(bool enable);
    GWCA_API void     SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds);
    GWCA_API void     SetTimestampsColor(uint32_t color);
    GWCA_API uint32_t SetSenderColor(int channel, uint32_t color);
    GWCA_API uint32_t SetMessageColor(int channel, uint32_t color);
    GWCA_API void     GetChannelColors(int channel, uint32_t* sender, uint32_t* message);
    GWCA_API void     GetDefaultColors(int channel, uint32_t* sender, uint32_t* message);
    GWCA_API int      GetChannelFromWChar(wchar_t opcode);
    GWCA_API int      GetChannelFromChar(char opcode);
}