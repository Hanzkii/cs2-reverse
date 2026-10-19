#ifndef CS2_ENGINE2_GAME_EVENTS_H
#define CS2_ENGINE2_GAME_EVENTS_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace cs2::engine2 {

struct EventKeyDescriptor {
    std::string keyName;
    int dataType; // 1: bool, 2: int32, 3: float, 4: string
};

struct EventDescriptor {
    uint32_t eventId;
    std::string eventName;
    std::vector<EventKeyDescriptor> keys;
};

struct EventInstance {
    uint32_t eventId;
    std::unordered_map<std::string, std::string> values;
};

class IGameEventListener {
public:
    virtual ~IGameEventListener() = default;
    virtual void OnEventFired(const EventInstance& ev) = 0;
};

/**
 * @brief Reconstructed 2-Phase GameEvent Dispatcher from engine2.dll
 * Phase 1: CSVCMsg_GameEventList signon dictionary synchronization
 * Phase 2: CSVCMsgList_GameEvents batched wire delivery
 */
class CGameEventDispatcher {
public:
    static CGameEventDispatcher& Get();

    void RegisterEventDescriptor(uint32_t id, const std::string& name, const std::vector<EventKeyDescriptor>& keys);
    void AddListener(const std::string& eventName, IGameEventListener* listener);
    void RemoveListener(IGameEventListener* listener);

    void DispatchEvent(const EventInstance& ev);

private:
    CGameEventDispatcher() = default;
    std::unordered_map<uint32_t, EventDescriptor> m_DescriptorsById;
    std::unordered_map<std::string, uint32_t> m_IdByName;
    std::unordered_map<uint32_t, std::vector<IGameEventListener*>> m_Listeners;
};

} // namespace cs2::engine2

#endif // CS2_ENGINE2_GAME_EVENTS_H
