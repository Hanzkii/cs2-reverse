#ifndef CS2_SOUNDSYSTEM_SOUND_OPS_H
#define CS2_SOUNDSYSTEM_SOUND_OPS_H

// ============================================================================
// Source 2 Spatial Audio, Acoustic Wave Propagation & Operator Dispatcher
//
// Clean-room reconstructed from:
// - soundsystem.dll (VTable @ 0x1804AB140, ISoundOpSystem "SoundOpSystem001")
// - steamaudio.dll / phonon.dll (Valve Steam Audio open-source SDK architecture)
// - client.dll (Sound Event Dispatcher @ 0xB4FB20)
//
// Supported Subsystems:
// 1. Direct Sound Path: Spherical inverse square attenuation, 3-band atmospheric
//    air absorption ODE, emitter directivity (dipole / cardioid sound cones),
//    and physical barrier acoustic transmission / occlusion raycasting.
// 2. Indirect Sound Path: 2nd-order Ambisonics B-format spherical harmonics
//    (ACN / SN3D: W, Y, Z, X, V, T, R, S, U), statistical reverberation modeling
//    (Eyring / Sabine RT60), and parametric impulse response filter synthesis.
// 3. Audio Bus Routing & Sound Event System: Multi-bus mixing hierarchy,
//    listener orientation transformation, and 3D event lifecycle management.
//
// Target Architecture: x86-64 (MSVC ABI)
// Language Standard: ISO C++20 (/std:c++20 /W4 /WX)
// External Dependencies: Zero (Pure Standard Library & Game Engine Headers)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numbers>
#include <string>
#include <unordered_map>

#include "../interfaces/sound_system.h"
#include "../tier0/utl_string_token.h"
#include "../shared/math_types.h"
#include "../shared/entity_handle.h"

namespace cs2::soundsystem {

// ----------------------------------------------------------------------------
// Acoustic Constants & Physical Parameters
// ----------------------------------------------------------------------------
inline constexpr float SPEED_OF_SOUND_AIR       = 343.0f;    // Speed of acoustic waves in dry air at 20°C (m/s)
inline constexpr float AIR_DENSITY_STANDARD     = 1.204f;    // Ambient air density at 20°C, 101.325 kPa (kg/m^3)
inline constexpr float REFERENCE_DISTANCE_M     = 1.0f;      // Unit reference distance (meters)
inline constexpr float SOURCE2_HAMMER_UNIT_TO_M = 0.0254f;   // 1 Hammer unit = 1 inch = 0.0254 meters
inline constexpr float M_TO_SOURCE2_HAMMER_UNIT = 39.3700787f; // 1 meter = 39.3700787 Hammer units

// ----------------------------------------------------------------------------
// Octave Band Representation (3 Standard Acoustic Bands)
// Steam Audio / Source 2 default frequency centers: Low (400 Hz), Mid (2.5 kHz), High (15 kHz)
// ----------------------------------------------------------------------------
struct OctaveBand3 {
    float low{ 0.0f };   // Centered at 400 Hz (Bass fundamental, structural resonance)
    float mid{ 0.0f };   // Centered at 2500 Hz (Speech intelligibility, presence)
    float high{ 0.0f };  // Centered at 15000 Hz (Air absorption roll-off, brilliance)

    constexpr OctaveBand3() noexcept = default;
    constexpr OctaveBand3(float l, float m, float h) noexcept : low(l), mid(m), high(h) {}

    [[nodiscard]] constexpr OctaveBand3 operator+(const OctaveBand3& o) const noexcept {
        return { low + o.low, mid + o.mid, high + o.high };
    }
    [[nodiscard]] constexpr OctaveBand3 operator-(const OctaveBand3& o) const noexcept {
        return { low - o.low, mid - o.mid, high - o.high };
    }
    [[nodiscard]] constexpr OctaveBand3 operator*(float s) const noexcept {
        return { low * s, mid * s, high * s };
    }
    [[nodiscard]] constexpr OctaveBand3 operator*(const OctaveBand3& o) const noexcept {
        return { low * o.low, mid * o.mid, high * o.high };
    }
    constexpr OctaveBand3& operator+=(const OctaveBand3& o) noexcept {
        low += o.low; mid += o.mid; high += o.high; return *this;
    }
    constexpr OctaveBand3& operator*=(float s) noexcept {
        low *= s; mid *= s; high *= s; return *this;
    }
    [[nodiscard]] constexpr float Average() const noexcept {
        return (low + mid + high) / 3.0f;
    }
};

// ----------------------------------------------------------------------------
// Distance Attenuation Models
// ----------------------------------------------------------------------------
enum class DistanceAttenuationModel : std::uint8_t {
    InverseDistance = 0,    // Standard spherical inverse law (1/d) with clamping
    LinearFalloff   = 1,    // Linear slope between min and max distance
    Exponential     = 2     // Exponential decay curve for localized ambient emitters
};

struct DistanceAttenuationParams {
    float                   minDistance{ 1.0f };     // Distance within which gain remains 1.0 (meters)
    float                   maxDistance{ 50.0f };    // Distance beyond which sound attenuates to silence (meters)
    float                   rolloffFactor{ 1.0f };  // Curve steepness multiplier
    DistanceAttenuationModel model{ DistanceAttenuationModel::InverseDistance };
};

// ----------------------------------------------------------------------------
// Emitter Directivity & Radiation Patterns
// ----------------------------------------------------------------------------
struct DirectivityParams {
    float dipoleWeight{ 0.0f };  // Dipole mixture alpha in [0.0..1.0] (0: omni, 0.5: cardioid, 1.0: dipole)
    float dipolePower{ 1.0f };   // Dipole exponent p >= 1.0 (sharpens emission beam)
    float innerAngleDeg{ 45.0f };// Inner cone angle (degrees) where directivity is 1.0
    float outerAngleDeg{ 90.0f };// Outer cone angle (degrees) where attenuation reaches outerGain
    float outerGain{ 0.1f };     // Minimum gain outside the outer cone [0.0..1.0]
    bool  useConeDirectivity{ false }; // Whether to evaluate cone model instead of dipole
};

// ----------------------------------------------------------------------------
// Physical Acoustic Materials & Transmission Coefficients
// ----------------------------------------------------------------------------
enum class AcousticMaterialType : std::uint8_t {
    Generic = 0,
    Concrete,
    Brick,
    Wood,
    Glass,
    Metal,
    Plaster,
    Carpet,
    Count
};

struct MaterialAcousticProperties {
    std::string_view name{ "Generic" };
    OctaveBand3      absorption{ 0.10f, 0.10f, 0.10f };   // Fraction of energy absorbed [0..1]
    OctaveBand3      transmission{ 0.05f, 0.03f, 0.01f }; // Fraction of energy transmitted through standard barrier [0..1]
    float            scattering{ 0.05f };                 // Fraction of diffuse acoustic reflection [0..1]
};

// ----------------------------------------------------------------------------
// Physical Acoustic Barrier (Geometric Obstacle)
// ----------------------------------------------------------------------------
struct AcousticBarrier {
    shared::Vector3D     minBounds{};             // World-space minimum AABB bounds (meters)
    shared::Vector3D     maxBounds{};             // World-space maximum AABB bounds (meters)
    float                thicknessMeters{ 0.20f };// Physical barrier wall thickness
    AcousticMaterialType material{ AcousticMaterialType::Concrete };

    [[nodiscard]] bool IntersectsRay(
        const shared::Vector3D& rayOrigin,
        const shared::Vector3D& rayDir,
        float maxDistance,
        float& outTEnter,
        float& outTExit
    ) const noexcept;
};

// ----------------------------------------------------------------------------
// Raycast Transmission & Occlusion Evaluation Result
// ----------------------------------------------------------------------------
struct OcclusionTransmissionResult {
    float       occlusionFactor{ 0.0f };                // 0.0 = completely unobstructed, 1.0 = fully blocked
    OctaveBand3 transmissionGain{ 1.0f, 1.0f, 1.0f };   // 3-band spectral gain transmitted through barrier
    OctaveBand3 effectiveDirectGain{ 1.0f, 1.0f, 1.0f };// Combined direct path gain (unoccluded + transmitted)
    float       distanceMeters{ 0.0f };                 // Emitter-to-listener Euclidean distance
};

// ----------------------------------------------------------------------------
// Ambisonics B-Format Spherical Harmonics
// Supports 0th, 1st, and 2nd Order Spherical Harmonics (ACN / SN3D standard)
// Channel Count:
// - Order 0: 1 channel  (ACN 0: W)
// - Order 1: 4 channels (ACN 0..3: W, Y, Z, X)
// - Order 2: 9 channels (ACN 0..8: W, Y, Z, X, V, T, R, S, U)
// ----------------------------------------------------------------------------
enum class AmbisonicsOrder : std::uint8_t {
    Order0 = 0, // 1 channel
    Order1 = 1, // 4 channels
    Order2 = 2  // 9 channels
};

struct AmbisonicsBFormat {
    // 9 spherical harmonic expansion coefficients
    std::array<float, 9> channels{};

    constexpr AmbisonicsBFormat() noexcept = default;

    // Direct named channel accessors (Furse-Malham & ACN conventions)
    [[nodiscard]] constexpr float W() const noexcept { return channels[0]; } // ACN 0: Omnidirectional (l=0, m=0)
    [[nodiscard]] constexpr float Y() const noexcept { return channels[1]; } // ACN 1: Left/Right (l=1, m=-1)
    [[nodiscard]] constexpr float Z() const noexcept { return channels[2]; } // ACN 2: Up/Down (l=1, m=0)
    [[nodiscard]] constexpr float X() const noexcept { return channels[3]; } // ACN 3: Front/Back (l=1, m=+1)
    [[nodiscard]] constexpr float V() const noexcept { return channels[4]; } // ACN 4: Diagonal XY (l=2, m=-2)
    [[nodiscard]] constexpr float T() const noexcept { return channels[5]; } // ACN 5: Vertical YZ (l=2, m=-1)
    [[nodiscard]] constexpr float R() const noexcept { return channels[6]; } // ACN 6: Vertical Z (l=2, m=0)
    [[nodiscard]] constexpr float S() const noexcept { return channels[7]; } // ACN 7: Vertical XZ (l=2, m=+1)
    [[nodiscard]] constexpr float U() const noexcept { return channels[8]; } // ACN 8: Planar X^2-Y^2 (l=2, m=+2)

    constexpr void SetW(float val) noexcept { channels[0] = val; }
    constexpr void SetY(float val) noexcept { channels[1] = val; }
    constexpr void SetZ(float val) noexcept { channels[2] = val; }
    constexpr void SetX(float val) noexcept { channels[3] = val; }
    constexpr void SetV(float val) noexcept { channels[4] = val; }
    constexpr void SetT(float val) noexcept { channels[5] = val; }
    constexpr void SetR(float val) noexcept { channels[6] = val; }
    constexpr void SetS(float val) noexcept { channels[7] = val; }
    constexpr void SetU(float val) noexcept { channels[8] = val; }

    [[nodiscard]] constexpr AmbisonicsBFormat operator+(const AmbisonicsBFormat& o) const noexcept {
        AmbisonicsBFormat res{};
        for (std::size_t i = 0; i < 9; ++i) res.channels[i] = channels[i] + o.channels[i];
        return res;
    }
    [[nodiscard]] constexpr AmbisonicsBFormat operator*(float s) const noexcept {
        AmbisonicsBFormat res{};
        for (std::size_t i = 0; i < 9; ++i) res.channels[i] = channels[i] * s;
        return res;
    }
    constexpr AmbisonicsBFormat& operator+=(const AmbisonicsBFormat& o) noexcept {
        for (std::size_t i = 0; i < 9; ++i) channels[i] += o.channels[i];
        return *this;
    }
    constexpr AmbisonicsBFormat& operator*=(float s) noexcept {
        for (std::size_t i = 0; i < 9; ++i) channels[i] *= s;
        return *this;
    }
};

// ----------------------------------------------------------------------------
// Room Acoustics & Statistical Reverberation (Sabine & Eyring Models)
// ----------------------------------------------------------------------------
struct RoomSurface {
    float                areaM2{ 1.0f };                           // Surface area in square meters
    AcousticMaterialType material{ AcousticMaterialType::Generic };// Material composition
    OctaveBand3          customAbsorption{ 0.1f, 0.1f, 0.1f };     // Absorption coefficients
};

struct RoomAcousticProperties {
    float                    volumeM3{ 100.0f }; // Room enclosed spatial volume (m^3)
    std::vector<RoomSurface> surfaces{};         // Bounding structural boundary surfaces

    [[nodiscard]] float TotalSurfaceArea() const noexcept;
    [[nodiscard]] OctaveBand3 TotalAbsorptionArea() const noexcept; // Equivalent absorption area A (metric Sabins)
    [[nodiscard]] OctaveBand3 AverageAbsorption() const noexcept;   // Dimensionless mean absorption alpha
};

struct ReverberationTimeRT60 {
    OctaveBand3 sabineRT60{};  // Classic Sabine RT60 (seconds)
    OctaveBand3 eyringRT60{};  // Eyring-Norris RT60 (seconds, accurate for high absorption)
};

// ----------------------------------------------------------------------------
// Parametric Impulse Response (IR) Filter Synthesis
// ----------------------------------------------------------------------------
struct ImpulseResponseSynthesisParams {
    float       earlyReflectionDelaySec{ 0.015f };  // Arrival latency of primary specular reflection (seconds)
    float       mixingTimeSec{ 0.040f };            // Transition threshold from discrete reflections to diffuse tail
    OctaveBand3 rt60{ 1.2f, 1.0f, 0.7f };           // Reverberation time decay per frequency band
    float       earlyGain{ 0.35f };                 // Amplitude gain of early specular reflection cluster
    float       lateGain{ 0.25f };                  // Amplitude gain of late diffuse decay tail
    std::uint32_t sampleRate{ 48000 };              // Digital signal processing sampling rate (Hz)
};

struct ImpulseResponseBuffer {
    std::vector<float> samples{};       // Discrete audio time samples
    std::uint32_t      sampleRate{ 48000 };
    float              durationSec{ 0.0f };

    [[nodiscard]] std::size_t SampleCount() const noexcept { return samples.size(); }
};

// ----------------------------------------------------------------------------
// Audio Bus Routing & Sound Event Management
// ----------------------------------------------------------------------------
enum class SoundBus : std::uint8_t {
    Master = 0,
    SFX,
    Music,
    Footsteps,
    Voice,
    Ambient,
    UI,
    Count
};

struct ActiveSoundInstance {
    std::uint64_t         guid{ 0 };
    std::uint32_t         eventHash{ 0 };
    std::string           eventName{};
    shared::Vector3D      origin{};
    shared::CEntityHandle entityHandle{};
    float                 volume{ 1.0f };
    float                 pitch{ 1.0f };
    SoundBus              bus{ SoundBus::SFX };
    float                 priority{ 1.0f };
    bool                  isPlaying{ true };
    float                 playbackTime{ 0.0f };

    // Evaluated DSP acoustic cache
    float                 distanceAttenuation{ 1.0f };
    OctaveBand3           airAbsorption{ 1.0f, 1.0f, 1.0f };
    float                 directivityGain{ 1.0f };
    float                 occlusionFactor{ 0.0f };
    OctaveBand3           transmissionGain{ 1.0f, 1.0f, 1.0f };
    AmbisonicsBFormat     ambisonics{};
    float                 reverbSend{ 0.2f };
};

struct ListenerOrientation {
    shared::Vector3D origin{ 0.0f, 0.0f, 0.0f };
    shared::Vector3D forward{ 1.0f, 0.0f, 0.0f };
    shared::Vector3D right{ 0.0f, 1.0f, 0.0f };
    shared::Vector3D up{ 0.0f, 0.0f, 1.0f };
    shared::Vector3D velocity{ 0.0f, 0.0f, 0.0f };

    [[nodiscard]] shared::Vector3D WorldToLocal(const shared::Vector3D& worldPos) const noexcept;
    [[nodiscard]] shared::Vector3D LocalToWorld(const shared::Vector3D& localPos) const noexcept;
};

// ----------------------------------------------------------------------------
// Acoustic Calculations & Wave Propagation Utilities
// ----------------------------------------------------------------------------
class AcousticWaveSolver {
public:
    /**
     * @brief Evaluates geometric distance attenuation with spherical inverse square roll-off.
     */
    [[nodiscard]] static float CalculateDistanceAttenuation(
        float distanceMeters,
        const DistanceAttenuationParams& params
    ) noexcept;

    /**
     * @brief Computes frequency-dependent atmospheric air absorption across 3 standard octave bands.
     * Uses empirical attenuation coefficients (dB/m) at standard temperature (20°C) and humidity (50%).
     * Solves the acoustic intensity differential equation dI/dr = -mu * I.
     */
    [[nodiscard]] static OctaveBand3 CalculateAirAbsorption(
        float distanceMeters,
        float temperatureC = 20.0f,
        float relativeHumidityPct = 50.0f
    ) noexcept;

    /**
     * @brief Solves the 1D acoustic intensity ODE dI/dr = -mu * I via 4th-order Runge-Kutta numerical integration.
     */
    [[nodiscard]] static float SolveAirAbsorptionODE_RK4(
        float initialIntensity,
        float distanceMeters,
        float absorptionCoeffDbPerMeter,
        std::uint32_t stepCount = 100
    ) noexcept;

    /**
     * @brief Computes sound emitter directivity gain based on dipole or sound cone models.
     */
    [[nodiscard]] static float CalculateDirectivity(
        const shared::Vector3D& emitterForward,
        const shared::Vector3D& directionToListener,
        const DirectivityParams& params
    ) noexcept;

    /**
     * @brief Evaluates raycast occlusion and transmission loss through a collection of physical acoustic barriers.
     */
    [[nodiscard]] static OcclusionTransmissionResult EvaluateOcclusionAndTransmission(
        const shared::Vector3D& emitterPos,
        const shared::Vector3D& listenerPos,
        const std::vector<AcousticBarrier>& barriers
    ) noexcept;

    /**
     * @brief Encodes a directional spatial sound wave into Ambisonics B-format spherical harmonics (0th to 2nd order).
     * @param directionNormalized Unit vector from listener pointing towards the sound emitter.
     * @param amplitude Input scalar signal gain.
     */
    [[nodiscard]] static AmbisonicsBFormat EncodeAmbisonics(
        const shared::Vector3D& directionNormalized,
        float amplitude,
        AmbisonicsOrder order = AmbisonicsOrder::Order2
    ) noexcept;

    /**
     * @brief Decodes an Ambisonics B-format sound field to a virtual loudspeaker positioned at a specific unit direction.
     */
    [[nodiscard]] static float DecodeAmbisonicsToSpeaker(
        const AmbisonicsBFormat& bFormat,
        const shared::Vector3D& speakerDirectionNormalized,
        AmbisonicsOrder order = AmbisonicsOrder::Order2
    ) noexcept;

    /**
     * @brief Decodes 2nd-order Ambisonics to a standard stereo pair (Left: -30 deg, Right: +30 deg).
     */
    static void DecodeAmbisonicsStereo(
        const AmbisonicsBFormat& bFormat,
        float& outLeft,
        float& outRight
    ) noexcept;

    /**
     * @brief Computes statistical reverberation time (RT60) using Sabine and Eyring-Norris formulas.
     * Sabine: RT60 = 0.161 * V / sum(S_i * alpha_i)
     * Eyring: RT60 = 0.161 * V / (-S * ln(1 - mean(alpha)))
     */
    [[nodiscard]] static ReverberationTimeRT60 CalculateReverberationTime(
        const RoomAcousticProperties& room
    ) noexcept;

    /**
     * @brief Synthesizes a parametric discrete impulse response filter buffer.
     */
    [[nodiscard]] static ImpulseResponseBuffer SynthesizeImpulseResponse(
        const ImpulseResponseSynthesisParams& params,
        float durationSeconds = 2.0f
    );

    /**
     * @brief Resolves default acoustic properties for standard structural materials.
     */
    [[nodiscard]] static const MaterialAcousticProperties& GetMaterialProperties(
        AcousticMaterialType material
    ) noexcept;
};

// ----------------------------------------------------------------------------
// CSoundManager (Sound Operator Subsystem & Event Dispatcher)
// Reconstructed from soundsystem.dll @ 0x1804AB140 and client.dll + 0xB4FB20
// ----------------------------------------------------------------------------
class CSoundManager {
public:
    static CSoundManager& Get();

    /**
     * @brief Computes 32-bit MurmurHash2 sound event identifier.
     */
    static constexpr std::uint32_t HashSoundEvent(std::string_view name) noexcept {
        return tier0::CUtlStringToken::Hash(name);
    }

    /**
     * @brief Formats a 3D sound event parameter packet (StartSoundEventInfo_t).
     */
    static interfaces::StartSoundEventInfo_t CreateSpatialSoundInfo(
        const shared::Vector3D& origin,
        float volume = 1.0f,
        float pitch = 1.0f,
        shared::CEntityHandle entity = shared::CEntityHandle{}
    ) noexcept;

    // Subsystem Lifecycle
    void Initialize();
    void Shutdown();

    // Listener State
    void SetListenerOrientation(
        const shared::Vector3D& origin,
        const shared::Vector3D& forward,
        const shared::Vector3D& up,
        const shared::Vector3D& velocity = shared::Vector3D{}
    ) noexcept;
    [[nodiscard]] const ListenerOrientation& GetListener() const noexcept { return m_listener; }

    // Physical Acoustic Environment Management
    void SetRoomAcoustics(const RoomAcousticProperties& room) { m_room = room; UpdateReverberation(); }
    void AddAcousticBarrier(const AcousticBarrier& barrier) { m_barriers.push_back(barrier); }
    void ClearAcousticBarriers() noexcept { m_barriers.clear(); }
    [[nodiscard]] const ReverberationTimeRT60& GetReverberationTime() const noexcept { return m_rt60; }

    // Sound Event Operations
    std::uint64_t StartSoundEvent(
        std::string_view eventName,
        const interfaces::StartSoundEventInfo_t* pInfo,
        SoundBus bus = SoundBus::SFX
    );

    std::uint64_t StartSoundEventByHash(
        std::uint32_t nEventHash,
        const interfaces::StartSoundEventInfo_t* pInfo,
        SoundBus bus = SoundBus::SFX
    );

    void StopSoundEvent(std::uint64_t guid, float flFadeTime = 0.0f);
    void SetSoundEventParam(std::uint64_t guid, std::string_view paramName, float flValue);

    // Dynamic Frame Tick Update (Evaluates raycasts, DSP filters, Ambisonics, and bus mixing)
    void Update(float flDeltaTime);

    // Audio Bus Control
    void SetBusVolume(SoundBus bus, float volume) noexcept;
    [[nodiscard]] float GetBusVolume(SoundBus bus) const noexcept;

    [[nodiscard]] std::size_t GetActiveSoundCount() const noexcept { return m_activeSounds.size(); }
    [[nodiscard]] const ActiveSoundInstance* FindSound(std::uint64_t guid) const noexcept;

private:
    CSoundManager() = default;
    ~CSoundManager() = default;
    CSoundManager(const CSoundManager&) = delete;
    CSoundManager& operator=(const CSoundManager&) = delete;

    void UpdateReverberation();

    ListenerOrientation                         m_listener{};
    RoomAcousticProperties                      m_room{};
    ReverberationTimeRT60                       m_rt60{};
    std::vector<AcousticBarrier>                m_barriers{};
    std::unordered_map<std::uint64_t, ActiveSoundInstance> m_activeSounds{};
    std::array<float, static_cast<std::size_t>(SoundBus::Count)> m_busVolumes{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::uint64_t                               m_nextGuid{ 1000 };
    bool                                        m_initialized{ false };
};

} // namespace cs2::soundsystem

#endif // CS2_SOUNDSYSTEM_SOUND_OPS_H
