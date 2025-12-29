#include "TargetEverything.h"

#include <DbgHelp.h>
#include <GWCA/GWCA.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/AgentContext.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "PluginUtils.h"

using GetIsAgentTargettableFn = decltype(&GW::Agents::GetIsAgentTargettable);
GetIsAgentTargettableFn GetIsAgentTargettable_Func = nullptr;
GetIsAgentTargettableFn RetGetIsAgentTargettable = nullptr;

static bool GetIsAgentTargettableOverride(const GW::Agent* agent)
{
    if (!agent) return false;

    if (agent->GetIsLivingType() || GW::Agents::GetAgentEncName(agent)) {
        return true;
    }

    return agent->agent_id < GW::GetAgentContext()->agent_movement.size();
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static TargetEverything instance;
    return &instance;
}

void TargetEverything::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::Initialize();

    GW::Scanner::Initialize(toolbox_dll);
    // find in ChatCommands::TargetNearest
    const auto instr = GW::Scanner::Find("\xFF\x15\xCC\xCC\xCC\xCC\x83\xC4\x04\x84\xC0\x0F\x84", "xx????xxxxxxx", 0x2);
    uintptr_t iat_entry = *reinterpret_cast<uintptr_t*>(instr);
    uintptr_t func_addr = *reinterpret_cast<uintptr_t*>(iat_entry);
    GetIsAgentTargettable_Func = reinterpret_cast<GetIsAgentTargettableFn>(func_addr);

    if (!GetIsAgentTargettable_Func) {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_WARNING, L"Failed to find GetIsAgentTargettable_Func in GWToolboxdll.dll", L"TargetEverything");
    }
    else {
        GW::Hook::Initialize();
        GW::Hook::CreateHook(reinterpret_cast<void**>(&GetIsAgentTargettable_Func), GetIsAgentTargettableOverride, reinterpret_cast<void**>(&RetGetIsAgentTargettable));
        GW::Hook::EnableHooks(GetIsAgentTargettable_Func);
    }

    GW::Scanner::Initialize();
}

bool TargetEverything::CanTerminate()
{
    return GW::Hook::GetInHookCount() == 0 && ToolboxPlugin::CanTerminate();
}

void TargetEverything::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();
    GW::DisableHooks();
}

void TargetEverything::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}
