#ifndef CS2_SERVER_CONTROLLER_PAWN_H
#define CS2_SERVER_CONTROLLER_PAWN_H

#include <cstdint>
#include <string>

namespace cs2::server {

constexpr uint32_t INVALID_EHANDLE = 0xFFFFFFFF;
constexpr uint32_t ENTRY_INDEX_MASK = 0x7FFF;
constexpr uint32_t SERIAL_NUMBER_SHIFT = 15;

/**
 * @brief 32-bit CEntityHandle packed bitfield
 * Bits 0..14: Entry Index (0..32,767)
 * Bits 15..31: Serial Number (ABA protection)
 */
struct EntityHandle {
    uint32_t raw = INVALID_EHANDLE;

    constexpr EntityHandle() noexcept = default;
    constexpr explicit EntityHandle(uint32_t val) noexcept : raw(val) {}
    constexpr EntityHandle(uint32_t entryIndex, uint32_t serialNum) noexcept
        : raw((entryIndex & ENTRY_INDEX_MASK) | (serialNum << SERIAL_NUMBER_SHIFT)) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return raw != INVALID_EHANDLE; }
    [[nodiscard]] constexpr uint32_t GetEntryIndex() const noexcept { return raw & ENTRY_INDEX_MASK; }
    [[nodiscard]] constexpr uint32_t GetSerialNumber() const noexcept { return raw >> SERIAL_NUMBER_SHIFT; }
};

struct PlayerPawn {
    EntityHandle handle;
    EntityHandle controllerHandle;
    int health = 100;
    bool isAlive = true;
    bool isBot = false;
};

struct PlayerController {
    EntityHandle handle;
    EntityHandle pawnHandle;
    std::string playerName;
    int ping = 15;
    int score = 0;
};

/**
 * @brief Reconstructed Controller vs Pawn Topology from server.dll
 * Decoupled controller entity possession, observer camera handover, and bot takeover.
 */
class CPlayerLifecycleManager {
public:
    static bool PossessPawn(PlayerController& controller, PlayerPawn& pawn);
    static bool OnPawnDeath(PlayerController& controller, PlayerPawn& deadPawn);
    static bool TakeoverBotPawn(PlayerController& controller, PlayerPawn& botPawn);
};

} // namespace cs2::server

#endif // CS2_SERVER_CONTROLLER_PAWN_H
