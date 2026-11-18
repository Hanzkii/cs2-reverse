# Engine Clock Drift Synchronization & Pre-Decode Packet Entities Coalescer

## 1. Executive Subsystem Summary

In recent Counter-Strike 2 engine revisions (including Build `2026-11-11_26598758`), Valve significantly overhauled two core client networking execution paths in `engine2.dll`:
1. **Sub-Tick Clock Drift Compensation & Authoritative Slamming (`CNetworkGameClient::ApplyClockDrift`)**:
   Located at virtual method slot 73 (`0x180067AA0`) of `CNetworkGameClient`, this routine regulates the relationship between client host ticks (`m_nClientTick` at offset `+0x39c`) and authoritative server packet ticks (`m_nServerTick` at offset `+0x398`). It evaluates network latency jitter margins against the maximum allowable clock drift window configured by `cl_clockdrift_max_secs`.
2. **Pre-Decode Packet Entities Delta Coalescing (`cl_packetentities_merge_before_decode`)**:
   Located in `CPacketEntitiesProcessor::ProcessEntityDeltaStream` (`0x1800441A0`), this architectural enhancement replaces the legacy two-pass entity deserialization pipeline with a unified, single-pass scratch bitstream arena merger, completely eliminating redundant intermediate field deserializations and change callback dispatch overhead.

---

## 2. Clock Drift Synchronization (`CNetworkGameClient::ApplyClockDrift`)

### 2.1 Mathematical Formulation & Drift Gating

Every incoming network packet delivers an authoritative server tick index $k_{\text{server}}$ alongside the active simulation ticks window $W_{\text{sim}}$. The client engine evaluates the clock deviation $\Delta k = k_{\text{client}} - k_{\text{server}}$.

The allowable drift threshold $T_{\text{max}}$ is dynamically calculated in `engine2.dll` @ `0x180067AA0`:

$$\begin{aligned}
k_{\text{drift\_max}} &= \text{cl\_clockdrift\_max\_secs} \times 64.0 \\
k_{\text{jitter}} &= m\_flJitterMarginSecs \times 64.0 \\
T_{\text{base}} &= \lceil k_{\text{drift\_max}} + k_{\text{jitter}} \rceil \\
T_{\text{total}} &= T_{\text{base}} + W_{\text{sim}}
\end{aligned}$$

When the absolute delta $|\Delta k|$ exceeds $T_{\text{total}}$, the engine immediately aborts smooth continuous time interpolation and executes an **authoritative tick slam**:

$$\begin{aligned}
\text{if } |\Delta k| > T_{\text{total}} \implies k_{\text{target}} = (k_{\text{client}} - W_{\text{sim}}) + 1
\end{aligned}$$

```
+-----------------------------------------------------------------------------------+
|               Authoritative Clock Drift Threshold Gating Window                   |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Server Tick (k_srv)             Allowable Margin (T_total)                       |
|         |                                                                         |
|         V                                                                         |
|  [======*===============================|======================================]  |
|         <--- Continuous Proportional --->  SLAM! (Snap to Server Window)          |
|              Adjustment Region                                                    |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

### 2.2 Verified Disassembly & Log Specifiers

When slamming is triggered, `engine2.dll` dispatches through the tier0 logging system:

```c
// Format specifier at 0x1805CE640:
"Slamming client tick to server tick. abs( server tick %d - client tick %d = %d ) > ( cl_clockdrift_max_ticks %.1f + sim ticks %d + jitter margin %.1f = %d )\n"

// Continuous correction format specifier at 0x1805CE600:
"[s%5d c%5d t%6.3f] ApplyClockDrift correcting %d -> %d\n"
```

### 2.3 Verified Member Offsets in `CNetworkGameClient`

| Offset | Type | Member Identifier | Description |
|---|---|---|---|
| `+0x398` | `int32_t` | `m_nServerTick` | Authoritative server tick index from latest packet |
| `+0x39c` | `int32_t` | `m_nClientTick` | Predicted client execution tick |
| `+0x2c356c` | `int32_t` | `m_nTargetTickClamped` | Local tick floor clamping boundary |
| `+0x2caf1c` | `float` | `m_flJitterMarginSecs` | Network packet transit jitter accumulator (seconds) |
| `+0x2caf24` | `float` | `m_flAverageDriftSecs` | Exponential moving average drift filter |
| `+0x2caf2c` | `int32_t` | `m_nSlamCount` | Total tick slamming events since map load |

---

## 3. Pre-Decode Entity Packet Coalescer (`cl_packetentities_merge_before_decode`)

### 3.1 Legacy Two-Pass Deserialization vs Single-Pass Pre-Decode Merge

In legacy Source 2 network processing, entity updates required two full deserialization passes:
1. **Pass 1 (Baseline Decompression)**: Decompresses the complete baseline entity bitstream into the entity memory slot. This deserializes all baseline fields (typically 32 to 48 fields) and triggers initial client state change callbacks.
2. **Pass 2 (Wire Delta Overlay)**: Decompresses the incoming packet delta bitstream over the same entity memory slot, overwriting baseline values with modified delta fields (typically 12 to 16 fields).

This legacy model caused two major performance bottlenecks:
- Redundant CPU deserialization work for every field present in both baseline and delta.
- Multiple redundant field change callbacks fired within the exact same frame.

### 3.2 Single-Pass Pre-Decode Consolidation Architecture

When `cl_packetentities_merge_before_decode` (ConVar at `0x1807305E8`) is enabled, `CPacketEntitiesProcessor::ProcessEntityDeltaStream` (`0x1800441A0`) short-circuits the dual decode:

```
+-----------------------------------------------------------------------------------------+
|                  Single-Pass Pre-Decode Consolidation Pipeline                          |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|   1. Baseline Bitstream  (lVar6)  ---+                                                  |
|                                      |--> MergeBitStreams() (0x60)                      |
|   2. Wire Delta Packet   (0x0c)   ---+        |                                         |
|                                               V                                         |
|                                      [ Scratch Arena Buffer ] (Allocated via 0x0)       |
|                                               |                                         |
|                                               V                                         |
|                                      Single-Pass Decode() (0x38)                        |
|                                               |                                         |
|                                               V                                         |
|                                      ReleaseScratchBuffer() (0x8)                       |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

1. **Arena Scratch Allocation**: Invokes slot `0x0` on the allocator vtable to lease a zero-copy bitstream scratch buffer from thread-local memory.
2. **Bitstream-Level Merging**: Invokes slot `0x60` (`MergeBitStreams`). The delta bits overwrite baseline bits directly within the compressed bitstream stream without deserializing intermediate C++ objects.
3. **Unified Single-Pass Deserialization**: Invokes slot `0x38` on the flattened serializer once. All final field values are unpacked into the entity memory block simultaneously with 0 redundant callbacks.
4. **Scratch Recycling**: Invokes slot `0x8` to return the scratch buffer to the thread-local allocation arena.

---

## 4. Multi-Threaded Atomic Work-Stealing Job Dispatcher

Packet entity processing across high player counts or complex maps is parallelized through `CParallelPacketEntitiesJob::Run` (`0x1800475C0`).

Worker threads dynamically dequeue `PacketEntityWorkItem` units via atomic fetch-and-add:

```x86asm
; engine2.dll @ 0x1800475CA
lock xadd   [rdi + 0x10], rbx   ; rbx = 0xb0 (176 bytes work item stride)
cmp         rax, [rdi + 0x18]   ; check bounds against total packet items
jae         loc_finish_job
```

Each work item (`0xb0` = 176 bytes) encapsulates:
- Entity handle & class descriptor index.
- Baseline bitstream pointer & delta packet bitstream pointer.
- Scratch buffer leased pointer.
- Field counts and completion telemetry flags.

---

## 5. Reconstructed Clean-Room C++20 SDK Headers

The authoritative clean-room implementations are maintained under:
- `game/engine/clock_drift.h` & `game/engine/clock_drift.cpp`: Exact mathematical tick slamming and drift filter.
- `game/network/packet_entities_coalescer.h` & `game/network/packet_entities_coalescer.cpp`: Pre-decode bitstream merger and parallel job work-item stride definition.
- `sdk/include/engine/clock_drift.hpp` & `sdk/include/network/packet_entities_coalescer.hpp`: SDK exposed aliases for client integration.
