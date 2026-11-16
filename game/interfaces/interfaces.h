#ifndef CS2_INTERFACES_INTERFACES_H
#define CS2_INTERFACES_INTERFACES_H

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "engine_client.h"
#include "game_event_manager.h"
#include "sound_system.h"
#include "network_server_service.h"
#include "network_client_service.h"
#include "source2_server.h"
#include "source2_client.h"
#include "cvar.h"
#include "schema_interface.h"
#include "memalloc.h"
#include "input_system.h"
#include "file_system.h"
#include "trace_manager.h"

namespace cs2::interfaces {

using InstantiateInterfaceFn = void* (*)();

/**
 * @brief Source 2 Internal Interface Registry Node (InterfaceReg)
 * Singly-linked list node populated by DLL exports.
 * Size: 24 bytes (0x18) on x64.
 */
struct InterfaceReg {
    InstantiateInterfaceFn m_CreateFn; // +0x00: Factory function returning interface instance
    const char*            m_pName;    // +0x08: Null-terminated versioned token string
    InterfaceReg*          m_pNext;    // +0x10: Next node in linked list
};
static_assert(sizeof(InterfaceReg) == 0x18, "InterfaceReg must be 24 bytes (0x18)");

using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);

/**
 * @brief Traverses an InterfaceReg linked list to locate an interface by version token.
 */
inline void* CaptureInterface(InterfaceReg* pHead, const char* pInterfaceName) noexcept {
    if (!pHead || !pInterfaceName) return nullptr;
    for (InterfaceReg* pCur = pHead; pCur; pCur = pCur->m_pNext) {
        if (pCur->m_pName && std::strcmp(pCur->m_pName, pInterfaceName) == 0) {
            return pCur->m_CreateFn ? pCur->m_CreateFn() : nullptr;
        }
    }
    return nullptr;
}

/**
 * @brief Type-safe template wrapper for capturing interfaces from an InterfaceReg head.
 */
template <typename T>
inline T* GetInterface(InterfaceReg* pHead, const char* pInterfaceName) noexcept {
    return static_cast<T*>(CaptureInterface(pHead, pInterfaceName));
}

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_INTERFACES_H
