#ifndef CS2_INTERFACES_GAME_EVENT_MANAGER_H
#define CS2_INTERFACES_GAME_EVENT_MANAGER_H

#include <cstdint>

namespace cs2::interfaces {

class IGameEvent {
public:
    virtual ~IGameEvent() = default;

    // Slot 1: GetName (RVA: +0x009d9990)
    virtual const char* GetName() const = 0;

    // Slot 2: GetID
    virtual int GetID() const = 0;

    // Slot 3: IsReliable
    virtual bool IsReliable() const = 0;

    // Slot 4: IsLocal
    virtual bool IsLocal() const = 0;

    // Slot 5: IsEmpty
    virtual bool IsEmpty(const char* keyName = nullptr) = 0;

    // Slot 6: GetBool
    virtual bool GetBool(const char* keyName = nullptr, bool defaultValue = false) = 0;

    // Slot 7: GetInt
    virtual int GetInt(const char* keyName = nullptr, int defaultValue = 0) = 0;

    // Slot 8: GetUint64
    virtual std::uint64_t GetUint64(const char* keyName = nullptr, std::uint64_t defaultValue = 0) = 0;

    // Slot 9: GetFloat
    virtual float GetFloat(const char* keyName = nullptr, float defaultValue = 0.0f) = 0;

    // Slot 10: GetString
    virtual const char* GetString(const char* keyName = nullptr, const char* defaultValue = "") = 0;
};

class IGameEventListener2 {
public:
    virtual ~IGameEventListener2() = default;
    virtual void FireGameEvent(IGameEvent* pEvent) = 0;
    virtual int  GetEventDebugID() { return 42; }
};

/**
 * @brief Source 2 Game Events Subsystem (IGameEventManager2)
 * Token: "GAMEEVENTSMANAGER002" in client.dll
 * Global Instance: client.dll + 0x2228E40 -> 0x22292C0
 * VTable: client.dll + 0x1C337A8 (16 slots)
 */
class IGameEventManager2 {
public:
    virtual ~IGameEventManager2() = default;

    // Slot 1: LoadEventsFromFile
    virtual int LoadEventsFromFile(const char* filename) = 0;

    // Slot 2: Reset
    virtual void Reset() = 0;

    // Slot 3: AddListener (RVA: +0x009cab60)
    virtual bool AddListener(IGameEventListener2* listener, const char* name, bool bServerSide = false) = 0;

    // Slot 4: FindListener (RVA: +0x009e2a20)
    virtual bool FindListener(IGameEventListener2* listener, const char* name) = 0;

    // Slot 5: RemoveListener (RVA: +0x00a02370)
    virtual void RemoveListener(IGameEventListener2* listener) = 0;

    // Slot 6: CreateEvent (RVA: +0x009d5440)
    virtual IGameEvent* CreateEvent(const char* name, bool bForce = false, int* pCookie = nullptr) = 0;

    // Slot 7: FireEvent (RVA: +0x009e3500)
    virtual bool FireEvent(IGameEvent* event, bool bDontBroadcast = false) = 0;

    // Slot 8: FireEventClientSide (RVA: +0x009e3790)
    virtual bool FireEventClientSide(IGameEvent* event) = 0;

    // Slot 9: DuplicateEvent (RVA: +0x009d9990)
    virtual IGameEvent* DuplicateEvent(IGameEvent* event) = 0;

    // Slot 10: FreeEvent (RVA: +0x009e3fe0)
    virtual void FreeEvent(IGameEvent* event) = 0;
};

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_GAME_EVENT_MANAGER_H
