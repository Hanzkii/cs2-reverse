#include "game_events.h"
#include <algorithm>

namespace cs2::engine2 {

CGameEventDispatcher& CGameEventDispatcher::Get() {
    static CGameEventDispatcher s_Instance;
    return s_Instance;
}

void CGameEventDispatcher::RegisterEventDescriptor(uint32_t id, const std::string& name, const std::vector<EventKeyDescriptor>& keys) {
    m_DescriptorsById[id] = { id, name, keys };
    m_IdByName[name] = id;
}

void CGameEventDispatcher::AddListener(const std::string& eventName, IGameEventListener* listener) {
    auto it = m_IdByName.find(eventName);
    if (it != m_IdByName.end() && listener) {
        m_Listeners[it->second].push_back(listener);
    }
}

void CGameEventDispatcher::RemoveListener(IGameEventListener* listener) {
    if (!listener) return;
    for (auto& pair : m_Listeners) {
        auto& vec = pair.second;
        vec.erase(std::remove(vec.begin(), vec.end(), listener), vec.end());
    }
}

void CGameEventDispatcher::DispatchEvent(const EventInstance& ev) {
    auto it = m_Listeners.find(ev.eventId);
    if (it != m_Listeners.end()) {
        for (auto* listener : it->second) {
            if (listener) {
                listener->OnEventFired(ev);
            }
        }
    }
}

} // namespace cs2::engine2
