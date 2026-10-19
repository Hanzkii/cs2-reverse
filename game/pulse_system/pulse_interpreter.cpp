#include "pulse_interpreter.h"
#include <algorithm>

namespace cs2::pulse_system {

void CPulseInterpreter::LoadProgram(std::vector<PGDInstruction_t> instructions) {
    m_Code = std::move(instructions);
}

uint32_t CPulseInterpreter::SpawnFiber() {
    PulseFiber_t fiber;
    fiber.fiberId = m_NextFiberId++;
    fiber.state = FiberState::Ready;
    fiber.programCounter = 0;
    m_Fibers.push_back(fiber);
    return fiber.fiberId;
}

const PulseFiber_t* CPulseInterpreter::GetFiber(uint32_t fiberId) const {
    for (const auto& fiber : m_Fibers) {
        if (fiber.fiberId == fiberId) {
            return &fiber;
        }
    }
    return nullptr;
}

FiberState CPulseInterpreter::StepFiber(uint32_t fiberId, size_t maxSteps) {
    auto it = std::find_if(m_Fibers.begin(), m_Fibers.end(), [fiberId](const PulseFiber_t& f) {
        return f.fiberId == fiberId;
    });

    if (it == m_Fibers.end()) return FiberState::Terminated;
    PulseFiber_t& fiber = *it;

    if (fiber.state == FiberState::Terminated) return FiberState::Terminated;
    fiber.state = FiberState::Running;

    size_t executedSteps = 0;

    // PulseInstructionDispatchLoop (0x18008f810)
    while (fiber.programCounter < m_Code.size() && executedSteps < maxSteps) {
        const PGDInstruction_t& inst = m_Code[fiber.programCounter++];
        executedSteps++;

        switch (inst.opcode) {
        case PulseOpcode::NOP:
            break;

        case PulseOpcode::LOAD_INT:
            if (inst.dstReg < 16) {
                fiber.registers[inst.dstReg] = inst.immInt;
            }
            break;

        case PulseOpcode::LOAD_FLOAT:
            if (inst.dstReg < 16) {
                // Store float as bit pattern in register
                *reinterpret_cast<double*>(&fiber.registers[inst.dstReg]) = inst.immDouble;
            }
            break;

        case PulseOpcode::MOVE:
            if (inst.dstReg < 16 && inst.srcRegA < 16) {
                fiber.registers[inst.dstReg] = fiber.registers[inst.srcRegA];
            }
            break;

        case PulseOpcode::ADD_INT:
            if (inst.dstReg < 16 && inst.srcRegA < 16 && inst.srcRegB < 16) {
                fiber.registers[inst.dstReg] = fiber.registers[inst.srcRegA] + fiber.registers[inst.srcRegB];
            }
            break;

        case PulseOpcode::SUB_INT:
            if (inst.dstReg < 16 && inst.srcRegA < 16 && inst.srcRegB < 16) {
                fiber.registers[inst.dstReg] = fiber.registers[inst.srcRegA] - fiber.registers[inst.srcRegB];
            }
            break;

        case PulseOpcode::MUL_FLOAT:
            if (inst.dstReg < 16 && inst.srcRegA < 16 && inst.srcRegB < 16) {
                double a = *reinterpret_cast<double*>(&fiber.registers[inst.srcRegA]);
                double b = *reinterpret_cast<double*>(&fiber.registers[inst.srcRegB]);
                double res = a * b;
                *reinterpret_cast<double*>(&fiber.registers[inst.dstReg]) = res;
            }
            break;

        case PulseOpcode::JUMP:
            fiber.programCounter = static_cast<size_t>(inst.immInt);
            break;

        case PulseOpcode::JUMP_IF_ZERO:
            if (inst.srcRegA < 16 && fiber.registers[inst.srcRegA] == 0) {
                fiber.programCounter = static_cast<size_t>(inst.immInt);
            }
            break;

        case PulseOpcode::CALL_DOMAIN_METHOD:
            // Placeholder for native CEngine / Schema binding method dispatch
            break;

        case PulseOpcode::YIELD:
            fiber.state = FiberState::Yielded;
            return FiberState::Yielded;

        case PulseOpcode::RETURN:
            fiber.state = FiberState::Terminated;
            return FiberState::Terminated;
        }
    }

    if (fiber.programCounter >= m_Code.size()) {
        fiber.state = FiberState::Terminated;
    }

    return fiber.state;
}

} // namespace cs2::pulse_system
