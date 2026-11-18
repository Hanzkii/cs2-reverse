#include "game/network/packet_entities_coalescer.h"

namespace cs2::network {

void VerifyPacketEntitiesPipeline() {
    CPacketEntitiesCoalescer coalescer(true);

    PacketEntityWorkItem item{};
    item.m_nEntityIndex = 42;
    item.m_nClassId = 128;
    item.m_nBaselineFieldsCount = 32;
    item.m_nDeltaFieldsCount = 16;

    PacketEntitiesMergeTelemetry telem{};
    coalescer.ProcessEntityDelta(item, &telem);
}

} // namespace cs2::network
