#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

#ifndef EXCEPT_EXPRESSION_LOOP
    #define EXCEPT_EXPRESSION_LOOP EXCEPTION_CONTINUE_SEARCH
#endif
namespace GW {

    struct Module;
    extern Module GameThreadModule;

    namespace GameThread {
        GWCA_API void EnableHooks();

        GWCA_API void ClearCalls();

        // force_enqueue = false; will check if we're already in the game thread, and run immediately if we are.
        // force_enqueue = true; enqueue the function for the next loop regardless
        GWCA_API void Enqueue(std::function<void ()> f, bool force_enqueue = false);

        typedef HookCallback<> GameThreadCallback;
        GWCA_API void RegisterGameThreadCallback(
            HookEntry *entry,
            const GameThreadCallback& callback,
            int altitude = 0x4000);

        GWCA_API void RemoveGameThreadCallback(HookEntry *entry);

        GWCA_API bool IsInGameThread();
    };
}
// ============================================================
// C Interop API
// ============================================================
extern "C" {
    typedef void(__cdecl* GW_GameThreadCallback)();
    GWCA_API void Enqueue(GW_GameThreadCallback callback, bool force_enqueue);
    GWCA_API bool IsInGameThread();
}