#include "controller_pawn.h"

namespace cs2::server {

bool CPlayerLifecycleManager::PossessPawn(PlayerController& controller, PlayerPawn& pawn) {
    if (!pawn.isAlive) {
        return false;
    }

    // Bi-directional link between persistent Controller and transient Pawn
    controller.pawnHandle = pawn.handle;
    pawn.controllerHandle = controller.handle;
    return true;
}

bool CPlayerLifecycleManager::OnPawnDeath(PlayerController& controller, PlayerPawn& deadPawn) {
    deadPawn.isAlive = false;
    deadPawn.health = 0;

    // Pawn unlinked upon death while Controller remains persistent in scoreboard/HUD
    deadPawn.controllerHandle = EntityHandle(INVALID_EHANDLE);
    controller.pawnHandle = EntityHandle(INVALID_EHANDLE);
    return true;
}

bool CPlayerLifecycleManager::TakeoverBotPawn(PlayerController& controller, PlayerPawn& botPawn) {
    if (!botPawn.isAlive || !botPawn.isBot) {
        return false;
    }

    // Seamless bot possession transfer
    return PossessPawn(controller, botPawn);
}

} // namespace cs2::server
