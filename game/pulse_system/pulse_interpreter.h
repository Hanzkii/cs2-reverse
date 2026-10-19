#ifndef CS2_PULSE_INTERPRETER_H
#define CS2_PULSE_INTERPRETER_H

#include <cstdint>
#include <vector>
#include <memory>

namespace cs2::pulse_system {

enum class PulseOpcode : uint16_t {
    NOP = 0,
    LOAD_INT = 1,
    LOAD_FLOAT = 2,
    MOVE = 3,
    ADD_INT = 4,
    SUB_INT = 5,
    MUL_FLOAT = 6,
    JUMP = 7,
    JUMP_IF_ZERO = 8,
    CALL_DOMAIN_METHOD = 9,
    YIELD = 10,
    RETURN = 11
};

/**
 * @brief 56-byte aligned instruction descriptor matching pulse_system.dll @ 0x18008f810.
 */
struct alignas(8) PGDInstruction_t {
    PulseOpcode opcode{PulseOpcode::NOP};
    uint16_t flags{0};
    uint16_t dstReg{0};
    uint16_t srcRegA{0};
    uint16_t srcRegB{0};
    uint16_t pad{0};
    int64_t immInt{0};
    double immDouble{0.0};
    uint64_t domainMethodId{0};
    uint64_t extraData[2]{0, 0};
};
static_assert(sizeof(PGDInstruction_t) == 56, "PGDInstruction_t must match exactly 56 bytes");

enum class FiberState : uint8_t {
    Ready = 0,
    Running = 1,
    Yielded = 2,
    Terminated = 3
};

struct PulseFiber_t {
    uint32_t fiberId{0};
    FiberState state{FiberState::Ready};
    size_t programCounter{0};
    int64_t registers[16]{0};
};

/**
 * @brief Reconstructed Pulse Interpreter Engine from pulse_system.dll.
 */
class CPulseInterpreter {
public:
    CPulseInterpreter() = default;

    /**
     * @brief Loads program instructions into bytecode cache.
     */
    void LoadProgram(std::vector<PGDInstruction_t> instructions);

    /**
     * @brief Spawns a new execution fiber coroutine.
     */
    uint32_t SpawnFiber();

    /**
     * @brief Executes instructions for a fiber until yield or return.
     * Matches PulseInstructionDispatchLoop @ 0x18008f810.
     */
    FiberState StepFiber(uint32_t fiberId, size_t maxSteps = 1000);

    [[nodiscard]] const PulseFiber_t* GetFiber(uint32_t fiberId) const;

private:
    std::vector<PGDInstruction_t> m_Code;
    std::vector<PulseFiber_t> m_Fibers;
    uint32_t m_NextFiberId{1};
};

} // namespace cs2::pulse_system

#endif // CS2_PULSE_INTERPRETER_H
