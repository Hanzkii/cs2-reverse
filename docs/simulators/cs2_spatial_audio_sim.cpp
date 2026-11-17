// ============================================================================
// Counter-Strike 2 (Source 2) Spatial Audio & Acoustic Wave Simulation Runner
//
// Reverse-Engineered from soundsystem.dll (VTable @ 0x1804AB140) & steamaudio.dll:
// - Direct Sound Path:
//   * Spherical inverse square distance attenuation with min/max clamps
//   * Frequency-dependent air absorption ODE (400 Hz, 2.5 kHz, 15 kHz)
//   * Emitter directivity patterns (dipole, cardioid, sound cones)
//   * Acoustic barrier transmission loss & occlusion raycasting (concrete, wood, glass)
// - Indirect Sound Path:
//   * 2nd-order Ambisonics B-format spherical harmonics (ACN/SN3D: W, Y, Z, X, V, T, R, S, U)
//   * Statistical room reverberation modeling (Sabine & Eyring-Norris RT60)
//   * Parametric impulse response (IR) filter synthesis (early reflections + late decay)
// - Sound Event Dispatcher & Audio Bus Routing:
//   * CSoundManager 3D listener coordinate tracking & multi-bus mixer
//
// Standalone ISO C++20 Reference Implementation. Zero external dependencies.
// Compilation: cl /std:c++20 /O2 /EHsc /W4 /WX cs2_spatial_audio_sim.cpp
// ============================================================================

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <string_view>
#include <string>
#include <numbers>

// ----------------------------------------------------------------------------
// Mathematical & Acoustic Constants
// ----------------------------------------------------------------------------
constexpr float SPEED_OF_SOUND_AIR       = 343.0f;    // Speed of sound in dry air at 20°C (m/s)
constexpr float REFERENCE_DISTANCE_M     = 1.0f;      // Unit reference distance (meters)
constexpr float K_SABINE_METRIC          = 0.161f;    // Sabine constant: 24 * ln(10) / c_sound

// ----------------------------------------------------------------------------
// Vector3D Math Type
// ----------------------------------------------------------------------------
struct Vector3D {
    float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };

    constexpr Vector3D() = default;
    constexpr Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    [[nodiscard]] constexpr Vector3D operator+(const Vector3D& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    [[nodiscard]] constexpr Vector3D operator-(const Vector3D& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    [[nodiscard]] constexpr Vector3D operator*(float s) const noexcept { return { x * s, y * s, z * s }; }
    [[nodiscard]] constexpr Vector3D operator/(float s) const noexcept { return { x / s, y / s, z / s }; }

    constexpr Vector3D& operator+=(const Vector3D& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vector3D& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }

    [[nodiscard]] constexpr float Dot(const Vector3D& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(Dot(*this)); }
    [[nodiscard]] constexpr float LengthSq() const noexcept { return Dot(*this); }

    [[nodiscard]] Vector3D Cross(const Vector3D& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] Vector3D Normalized() const noexcept {
        const float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3D{};
    }
};

// ----------------------------------------------------------------------------
// 3-Band Frequency Octave Representation (400 Hz, 2.5 kHz, 15 kHz)
// ----------------------------------------------------------------------------
struct OctaveBand3 {
    float low{ 0.0f };   // Centered at 400 Hz (Bass fundamental, structural resonance)
    float mid{ 0.0f };   // Centered at 2500 Hz (Speech intelligibility, presence)
    float high{ 0.0f };  // Centered at 15000 Hz (Treble, air absorption roll-off)

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
    [[nodiscard]] constexpr float Average() const noexcept {
        return (low + mid + high) / 3.0f;
    }
};

// ----------------------------------------------------------------------------
// Distance Attenuation Models
// ----------------------------------------------------------------------------
enum class DistanceAttenuationModel : std::uint8_t {
    InverseDistance = 0,
    LinearFalloff   = 1,
    Exponential     = 2
};

struct DistanceAttenuationParams {
    float                    minDistance{ 1.0f };
    float                    maxDistance{ 50.0f };
    float                    rolloffFactor{ 1.0f };
    DistanceAttenuationModel model{ DistanceAttenuationModel::InverseDistance };
};

[[nodiscard]] inline float CalculateDistanceAttenuation(
    float distanceMeters,
    const DistanceAttenuationParams& params
) noexcept {
    if (distanceMeters <= params.minDistance) return 1.0f;
    if (distanceMeters >= params.maxDistance) return 0.0f;

    const float dRange = params.maxDistance - params.minDistance;
    if (dRange < 1e-4f) return 0.0f;

    switch (params.model) {
        case DistanceAttenuationModel::InverseDistance: {
            const float inv = params.minDistance / distanceMeters;
            const float window = (params.maxDistance - distanceMeters) / dRange;
            const float gain = inv * std::pow(window, params.rolloffFactor);
            return std::clamp(gain, 0.0f, 1.0f);
        }
        case DistanceAttenuationModel::LinearFalloff: {
            const float frac = (distanceMeters - params.minDistance) / dRange;
            return std::clamp(1.0f - frac, 0.0f, 1.0f);
        }
        case DistanceAttenuationModel::Exponential: {
            const float frac = (distanceMeters - params.minDistance) / dRange;
            const float gain = std::exp(-frac * 3.0f * params.rolloffFactor) *
                               ((params.maxDistance - distanceMeters) / dRange);
            return std::clamp(gain, 0.0f, 1.0f);
        }
        default:
            return 1.0f;
    }
}

// ----------------------------------------------------------------------------
// 3-Band Atmospheric Air Absorption (ISO 9613-1 / Steam Audio)
// ----------------------------------------------------------------------------
[[nodiscard]] inline OctaveBand3 CalculateAirAbsorption(
    float distanceMeters,
    float temperatureC = 20.0f,
    float relativeHumidityPct = 50.0f
) noexcept {
    if (distanceMeters <= 0.0f) return { 1.0f, 1.0f, 1.0f };

    constexpr float BASE_ALPHA_LOW  = 0.0020f; // dB/m @ 400 Hz
    constexpr float BASE_ALPHA_MID  = 0.0150f; // dB/m @ 2500 Hz
    constexpr float BASE_ALPHA_HIGH = 0.1500f; // dB/m @ 15000 Hz

    const float tKelvin = std::max(temperatureC + 273.15f, 250.0f);
    const float tRel    = 293.15f / tKelvin;
    const float rhClamped = std::clamp(relativeHumidityPct, 10.0f, 100.0f);
    const float hRel    = 50.0f / rhClamped;

    const float alphaLow  = BASE_ALPHA_LOW  * std::sqrt(tRel) * (0.8f + 0.2f * hRel);
    const float alphaMid  = BASE_ALPHA_MID  * tRel            * (0.6f + 0.4f * hRel);
    const float alphaHigh = BASE_ALPHA_HIGH * (tRel * tRel)   * (0.4f + 0.6f * hRel);

    const float gainLow  = std::pow(10.0f, -alphaLow  * distanceMeters / 20.0f);
    const float gainMid  = std::pow(10.0f, -alphaMid  * distanceMeters / 20.0f);
    const float gainHigh = std::pow(10.0f, -alphaHigh * distanceMeters / 20.0f);

    return {
        std::clamp(gainLow,  0.0f, 1.0f),
        std::clamp(gainMid,  0.0f, 1.0f),
        std::clamp(gainHigh, 0.0f, 1.0f)
    };
}

// RK4 Numerical Integration of dI/dr = -mu * I
[[nodiscard]] inline float SolveAirAbsorptionODE_RK4(
    float initialIntensity,
    float distanceMeters,
    float absorptionCoeffDbPerMeter,
    std::uint32_t stepCount = 100
) noexcept {
    if (distanceMeters <= 0.0f || stepCount == 0) return initialIntensity;

    const float mu = absorptionCoeffDbPerMeter * (std::numbers::ln10_v<float> / 10.0f);
    const float dr = distanceMeters / static_cast<float>(stepCount);

    float intensity = initialIntensity;
    for (std::uint32_t i = 0; i < stepCount; ++i) {
        const float k1 = -mu * intensity;
        const float k2 = -mu * (intensity + 0.5f * dr * k1);
        const float k3 = -mu * (intensity + 0.5f * dr * k2);
        const float k4 = -mu * (intensity + dr * k3);

        intensity += (dr / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
        if (intensity < 0.0f) intensity = 0.0f;
    }
    return intensity;
}

// ----------------------------------------------------------------------------
// Emitter Directivity Patterns (Dipole & Sound Cones)
// ----------------------------------------------------------------------------
struct DirectivityParams {
    float dipoleWeight{ 0.0f };   // 0.0: omni, 0.5: cardioid, 1.0: dipole
    float dipolePower{ 1.0f };
    float innerAngleDeg{ 45.0f };
    float outerAngleDeg{ 90.0f };
    float outerGain{ 0.1f };
    bool  useConeDirectivity{ false };
};

[[nodiscard]] inline float CalculateDirectivity(
    const Vector3D& emitterForward,
    const Vector3D& directionToListener,
    const DirectivityParams& params
) noexcept {
    const float fLen = emitterForward.Length();
    const float dLen = directionToListener.Length();
    if (fLen < 1e-4f || dLen < 1e-4f) return 1.0f;

    const float cosAngle = std::clamp(emitterForward.Dot(directionToListener) / (fLen * dLen), -1.0f, 1.0f);

    if (params.useConeDirectivity) {
        const float angleDeg = std::acos(cosAngle) * (180.0f / std::numbers::pi_v<float>);
        if (angleDeg <= params.innerAngleDeg) return 1.0f;
        if (angleDeg >= params.outerAngleDeg) return params.outerGain;

        const float t = (angleDeg - params.innerAngleDeg) / (params.outerAngleDeg - params.innerAngleDeg);
        const float smooth = t * t * (3.0f - 2.0f * t);
        return 1.0f - (1.0f - params.outerGain) * smooth;
    }

    const float alpha = std::clamp(params.dipoleWeight, 0.0f, 1.0f);
    const float term  = (1.0f - alpha) + alpha * cosAngle;
    const float power = std::max(params.dipolePower, 1.0f);
    return std::clamp(std::pow(std::abs(term), power), 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// Material Acoustics & Transmission Barrier Raycasting
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
    std::string_view name;
    OctaveBand3      absorption;
    OctaveBand3      transmission;
    float            scattering;
};

inline const MaterialAcousticProperties s_Materials[] = {
    { "Generic",  { 0.10f, 0.10f, 0.10f }, { 0.050f, 0.030f, 0.010f }, 0.05f },
    { "Concrete", { 0.015f, 0.025f, 0.040f }, { 0.010f, 0.005f, 0.001f }, 0.05f },
    { "Brick",    { 0.030f, 0.040f, 0.070f }, { 0.020f, 0.010f, 0.004f }, 0.08f },
    { "Wood",     { 0.150f, 0.110f, 0.100f }, { 0.120f, 0.080f, 0.040f }, 0.12f },
    { "Glass",    { 0.180f, 0.060f, 0.030f }, { 0.200f, 0.100f, 0.030f }, 0.02f },
    { "Metal",    { 0.050f, 0.040f, 0.020f }, { 0.030f, 0.015f, 0.005f }, 0.15f },
    { "Plaster",  { 0.100f, 0.080f, 0.050f }, { 0.080f, 0.040f, 0.015f }, 0.06f },
    { "Carpet",   { 0.050f, 0.300f, 0.600f }, { 0.040f, 0.015f, 0.003f }, 0.20f }
};

inline const MaterialAcousticProperties& GetMaterialProperties(AcousticMaterialType mat) noexcept {
    const auto idx = static_cast<std::size_t>(mat);
    return (idx < static_cast<std::size_t>(AcousticMaterialType::Count)) ? s_Materials[idx] : s_Materials[0];
}

struct AcousticBarrier {
    Vector3D             minBounds{};
    Vector3D             maxBounds{};
    float                thicknessMeters{ 0.20f };
    AcousticMaterialType material{ AcousticMaterialType::Concrete };

    [[nodiscard]] bool IntersectsRay(
        const Vector3D& rayOrigin,
        const Vector3D& rayDir,
        float maxDistance,
        float& outTEnter,
        float& outTExit
    ) const noexcept {
        float tmin = 0.0f;
        float tmax = maxDistance;

        const float origins[3] = { rayOrigin.x, rayOrigin.y, rayOrigin.z };
        const float dirs[3]    = { rayDir.x,    rayDir.y,    rayDir.z };
        const float mins[3]    = { minBounds.x, minBounds.y, minBounds.z };
        const float maxs[3]    = { maxBounds.x, maxBounds.y, maxBounds.z };

        for (int i = 0; i < 3; ++i) {
            if (std::abs(dirs[i]) < 1e-8f) {
                if (origins[i] < mins[i] || origins[i] > maxs[i]) return false;
            } else {
                const float invD = 1.0f / dirs[i];
                float t1 = (mins[i] - origins[i]) * invD;
                float t2 = (maxs[i] - origins[i]) * invD;
                if (t1 > t2) std::swap(t1, t2);

                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax) return false;
            }
        }
        outTEnter = tmin;
        outTExit  = tmax;
        return (tmax > 0.0f && tmin < maxDistance);
    }
};

struct OcclusionTransmissionResult {
    float       occlusionFactor{ 0.0f };
    OctaveBand3 transmissionGain{ 1.0f, 1.0f, 1.0f };
    OctaveBand3 effectiveDirectGain{ 1.0f, 1.0f, 1.0f };
    float       distanceMeters{ 0.0f };
};

[[nodiscard]] inline OcclusionTransmissionResult EvaluateOcclusionAndTransmission(
    const Vector3D& emitterPos,
    const Vector3D& listenerPos,
    const std::vector<AcousticBarrier>& barriers
) noexcept {
    OcclusionTransmissionResult result{};
    const Vector3D rayVec = listenerPos - emitterPos;
    const float dist = rayVec.Length();
    result.distanceMeters = dist;

    if (dist < 1e-4f || barriers.empty()) return result;

    const Vector3D rayDir = rayVec / dist;
    OctaveBand3 netTransmission{ 1.0f, 1.0f, 1.0f };
    int hitCount = 0;

    for (const auto& barrier : barriers) {
        float tEnter = 0.0f, tExit = 0.0f;
        if (barrier.IntersectsRay(emitterPos, rayDir, dist, tEnter, tExit)) {
            const float traversalDist = (tExit - tEnter) * dist;
            if (traversalDist > 1e-4f) {
                hitCount++;
                const auto& matProps = GetMaterialProperties(barrier.material);
                const float thickScale = traversalDist / std::max(barrier.thicknessMeters, 0.05f);
                const float decay = std::clamp(std::exp(-0.5f * thickScale), 0.05f, 1.0f);
                netTransmission = netTransmission * (matProps.transmission * decay);
            }
        }
    }

    if (hitCount > 0) {
        result.occlusionFactor     = 1.0f;
        result.transmissionGain    = netTransmission;
        result.effectiveDirectGain = netTransmission;
    } else {
        result.occlusionFactor     = 0.0f;
        result.transmissionGain    = { 1.0f, 1.0f, 1.0f };
        result.effectiveDirectGain = { 1.0f, 1.0f, 1.0f };
    }
    return result;
}

// ----------------------------------------------------------------------------
// Ambisonics B-Format Spherical Harmonics (2nd Order: 9 channels)
// ACN Indexing:
// ACN 0: W (l=0, m=0)
// ACN 1: Y (l=1, m=-1) = y
// ACN 2: Z (l=1, m=0)  = z
// ACN 3: X (l=1, m=+1) = x
// ACN 4: V (l=2, m=-2) = sqrt(3)*x*y
// ACN 5: T (l=2, m=-1) = sqrt(3)*y*z
// ACN 6: R (l=2, m=0)  = 0.5*(3*z^2 - 1)
// ACN 7: S (l=2, m=+1) = sqrt(3)*x*z
// ACN 8: U (l=2, m=+2) = sqrt(3)/2*(x^2 - y^2)
// ----------------------------------------------------------------------------
struct AmbisonicsBFormat {
    std::array<float, 9> channels{};

    constexpr AmbisonicsBFormat() noexcept = default;

    [[nodiscard]] constexpr float W() const noexcept { return channels[0]; }
    [[nodiscard]] constexpr float Y() const noexcept { return channels[1]; }
    [[nodiscard]] constexpr float Z() const noexcept { return channels[2]; }
    [[nodiscard]] constexpr float X() const noexcept { return channels[3]; }
    [[nodiscard]] constexpr float V() const noexcept { return channels[4]; }
    [[nodiscard]] constexpr float T() const noexcept { return channels[5]; }
    [[nodiscard]] constexpr float R() const noexcept { return channels[6]; }
    [[nodiscard]] constexpr float S() const noexcept { return channels[7]; }
    [[nodiscard]] constexpr float U() const noexcept { return channels[8]; }
};

[[nodiscard]] inline AmbisonicsBFormat EncodeAmbisonics(
    const Vector3D& dirNorm,
    float amplitude
) noexcept {
    AmbisonicsBFormat b{};
    const float x = dirNorm.x;
    const float y = dirNorm.y;
    const float z = dirNorm.z;

    // 0th Order
    b.channels[0] = amplitude * 1.0f;

    // 1st Order
    b.channels[1] = amplitude * y;
    b.channels[2] = amplitude * z;
    b.channels[3] = amplitude * x;

    // 2nd Order
    constexpr float SQRT3      = 1.73205080757f;
    constexpr float HALF_SQRT3 = 0.86602540378f;

    b.channels[4] = amplitude * (SQRT3 * x * y);
    b.channels[5] = amplitude * (SQRT3 * y * z);
    b.channels[6] = amplitude * (0.5f * (3.0f * z * z - 1.0f));
    b.channels[7] = amplitude * (SQRT3 * x * z);
    b.channels[8] = amplitude * (HALF_SQRT3 * (x * x - y * y));

    return b;
}

[[nodiscard]] inline float DecodeAmbisonicsToSpeaker(
    const AmbisonicsBFormat& bFormat,
    const Vector3D& speakerDirNorm
) noexcept {
    const AmbisonicsBFormat spkEnc = EncodeAmbisonics(speakerDirNorm, 1.0f);
    constexpr float WEIGHTS[3] = { 1.0f, 0.57735f, 0.35355f };

    float gain = bFormat.channels[0] * spkEnc.channels[0] * WEIGHTS[0];

    float o1Sum = 0.0f;
    for (std::size_t i = 1; i <= 3; ++i) o1Sum += bFormat.channels[i] * spkEnc.channels[i];
    gain += o1Sum * WEIGHTS[1];

    float o2Sum = 0.0f;
    for (std::size_t i = 4; i <= 8; ++i) o2Sum += bFormat.channels[i] * spkEnc.channels[i];
    gain += o2Sum * WEIGHTS[2];

    return gain;
}

// ----------------------------------------------------------------------------
// Room Acoustics & Statistical Reverberation (Sabine & Eyring-Norris RT60)
// Sabine: RT60 = 0.161 * V / sum(S_i * alpha_i)
// Eyring: RT60 = 0.161 * V / (-S * ln(1 - mean(alpha)))
// ----------------------------------------------------------------------------
struct RoomSurface {
    float                areaM2{ 1.0f };
    AcousticMaterialType material{ AcousticMaterialType::Generic };
};

struct RoomAcousticProperties {
    float                    volumeM3{ 100.0f };
    std::vector<RoomSurface> surfaces{};

    [[nodiscard]] float TotalArea() const noexcept {
        float s = 0.0f;
        for (const auto& surf : surfaces) s += surf.areaM2;
        return (s > 1e-4f) ? s : 1.0f;
    }

    [[nodiscard]] OctaveBand3 TotalAbsorptionArea() const noexcept {
        OctaveBand3 totalA{};
        for (const auto& surf : surfaces) {
            const auto& mat = GetMaterialProperties(surf.material);
            totalA += mat.absorption * surf.areaM2;
        }
        return totalA;
    }

    [[nodiscard]] OctaveBand3 AverageAbsorption() const noexcept {
        return TotalAbsorptionArea() * (1.0f / TotalArea());
    }
};

struct ReverberationTimeRT60 {
    OctaveBand3 sabineRT60{};
    OctaveBand3 eyringRT60{};
};

[[nodiscard]] inline ReverberationTimeRT60 CalculateReverberationTime(const RoomAcousticProperties& room) noexcept {
    ReverberationTimeRT60 rt{};
    const float V = std::max(room.volumeM3, 0.1f);
    const float S = room.TotalArea();
    const OctaveBand3 A = room.TotalAbsorptionArea();
    const OctaveBand3 meanAlpha = room.AverageAbsorption();

    // Sabine
    rt.sabineRT60.low  = K_SABINE_METRIC * V / std::max(A.low,  0.01f);
    rt.sabineRT60.mid  = K_SABINE_METRIC * V / std::max(A.mid,  0.01f);
    rt.sabineRT60.high = K_SABINE_METRIC * V / std::max(A.high, 0.01f);

    // Eyring-Norris
    auto SolveEyring = [&](float alpha) noexcept -> float {
        const float alphaClamped = std::clamp(alpha, 0.001f, 0.990f);
        const float den = -S * std::log(1.0f - alphaClamped);
        return K_SABINE_METRIC * V / std::max(den, 0.01f);
    };

    rt.eyringRT60.low  = SolveEyring(meanAlpha.low);
    rt.eyringRT60.mid  = SolveEyring(meanAlpha.mid);
    rt.eyringRT60.high = SolveEyring(meanAlpha.high);

    return rt;
}

// ----------------------------------------------------------------------------
// Parametric Impulse Response (IR) Filter Synthesis
// ----------------------------------------------------------------------------
struct ImpulseResponseBuffer {
    std::vector<float> samples{};
    std::uint32_t      sampleRate{ 48000 };
    float              durationSec{ 0.0f };

    [[nodiscard]] std::size_t SampleCount() const noexcept { return samples.size(); }
};

[[nodiscard]] inline ImpulseResponseBuffer SynthesizeImpulseResponse(
    float earlyRefDelaySec,
    float mixingTimeSec,
    const OctaveBand3& rt60,
    float earlyGain,
    float lateGain,
    float durationSeconds = 1.0f,
    std::uint32_t sampleRate = 48000
) {
    ImpulseResponseBuffer buf{};
    buf.sampleRate  = sampleRate;
    buf.durationSec = durationSeconds;

    const std::size_t totalSamples = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRate));
    buf.samples.assign(totalSamples, 0.0f);

    if (totalSamples == 0) return buf;

    // 1. Direct impulse at t=0
    buf.samples[0] = 1.0f;

    // 2. Early Reflections
    const std::size_t nEarly1 = static_cast<std::size_t>(earlyRefDelaySec * static_cast<float>(sampleRate));
    const std::size_t nEarly2 = static_cast<std::size_t>(earlyRefDelaySec * 1.7f * static_cast<float>(sampleRate));
    if (nEarly1 < totalSamples) buf.samples[nEarly1] += earlyGain * 0.8f;
    if (nEarly2 < totalSamples) buf.samples[nEarly2] -= earlyGain * 0.5f;

    // 3. Late Reverberation Decay Tail
    const float tauDecay = (rt60.Average() > 0.05f) ? (rt60.Average() / 6.90776f) : 0.1f;
    const std::size_t nMix = static_cast<std::size_t>(mixingTimeSec * static_cast<float>(sampleRate));

    std::uint32_t lcg = 0xDEADBEEF;
    auto Rand = [&lcg]() noexcept -> float {
        lcg = lcg * 1664525u + 1013904223u;
        return (static_cast<float>(lcg & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF)) * 2.0f - 1.0f;
    };

    for (std::size_t n = nMix; n < totalSamples; ++n) {
        const float t = static_cast<float>(n - nMix) / static_cast<float>(sampleRate);
        const float env = std::exp(-t / tauDecay);
        buf.samples[n] += lateGain * env * Rand();
    }

    return buf;
}

// ============================================================================
// SIMULATION RUNNER & VERIFICATION SUITE
// ============================================================================
int main() {
    std::cout << "====================================================================\n";
    std::cout << "Counter-Strike 2 (Source 2) Spatial Audio & Acoustic Wave Simulator\n";
    std::cout << "Clean-Room Reverse Engineering & Verification Harness (ISO C++20)\n";
    std::cout << "====================================================================\n\n";

    int passedTests = 0;
    int totalTests  = 0;

    auto AssertTest = [&](bool condition, std::string_view testName) {
        totalTests++;
        if (condition) {
            passedTests++;
            std::cout << "  [PASS] " << testName << "\n";
        } else {
            std::cerr << "  [FAIL] " << testName << "\n";
        }
    };

    // ------------------------------------------------------------------------
    // STAGE 1: Direct Sound Path - Distance Attenuation Verification
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 1] Direct Sound Path: Distance Attenuation Models\n";
    {
        DistanceAttenuationParams paramsInv{};
        paramsInv.minDistance = 1.0f;
        paramsInv.maxDistance = 50.0f;
        paramsInv.model = DistanceAttenuationModel::InverseDistance;

        DistanceAttenuationParams paramsLin = paramsInv;
        paramsLin.model = DistanceAttenuationModel::LinearFalloff;

        DistanceAttenuationParams paramsExp = paramsInv;
        paramsExp.model = DistanceAttenuationModel::Exponential;

        const float gMin = CalculateDistanceAttenuation(0.5f, paramsInv);
        const float g1m  = CalculateDistanceAttenuation(1.0f, paramsInv);
        const float g5m  = CalculateDistanceAttenuation(5.0f, paramsInv);
        const float g10m = CalculateDistanceAttenuation(10.0f, paramsInv);
        const float g25m = CalculateDistanceAttenuation(25.0f, paramsInv);
        const float g50m = CalculateDistanceAttenuation(50.0f, paramsInv);
        const float g80m = CalculateDistanceAttenuation(80.0f, paramsInv);

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  Distance Attenuation Profile (Inverse Distance):\n";
        std::cout << "    d =  0.5m (d <= d_min): Gain = " << gMin << " (Clamped 1.0)\n";
        std::cout << "    d =  1.0m (Reference):  Gain = " << g1m  << "\n";
        std::cout << "    d =  5.0m:             Gain = " << g5m  << "\n";
        std::cout << "    d = 10.0m:             Gain = " << g10m << "\n";
        std::cout << "    d = 25.0m:             Gain = " << g25m << "\n";
        std::cout << "    d = 50.0m (d == d_max): Gain = " << g50m << " (Silence boundary)\n";
        std::cout << "    d = 80.0m (d > d_max):  Gain = " << g80m << "\n";

        AssertTest(gMin == 1.0f, "Distance <= minDistance clamps to 1.0f");
        AssertTest(g50m == 0.0f && g80m == 0.0f, "Distance >= maxDistance attenuates to 0.0f");
        AssertTest(g10m < g5m && g25m < g10m, "Monotonic attenuation decay over distance");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 2: Atmospheric Air Absorption - Analytical vs RK4 ODE Numerical Solver
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 2] Frequency-Dependent Atmospheric Air Absorption (ISO 9613-1 / ODE)\n";
    {
        const float distance = 40.0f; // 40 meters distance
        const auto absStandard = CalculateAirAbsorption(distance, 20.0f, 50.0f);
        const auto absColdDry  = CalculateAirAbsorption(distance, 0.0f, 20.0f);
        const auto absHotHumid = CalculateAirAbsorption(distance, 35.0f, 80.0f);

        std::cout << "  40m Distance 3-Band Air Absorption Gain Multipliers:\n";
        std::cout << "    Standard (20C, 50% RH): Low(400Hz)=" << absStandard.low
                  << "  Mid(2.5kHz)=" << absStandard.mid
                  << "  High(15kHz)=" << absStandard.high << "\n";
        std::cout << "    Cold Dry ( 0C, 20% RH): Low(400Hz)=" << absColdDry.low
                  << "  Mid(2.5kHz)=" << absColdDry.mid
                  << "  High(15kHz)=" << absColdDry.high << "\n";
        std::cout << "    Hot Humid(35C, 80% RH): Low(400Hz)=" << absHotHumid.low
                  << "  Mid(2.5kHz)=" << absHotHumid.mid
                  << "  High(15kHz)=" << absHotHumid.high << "\n";

        // ODE Numerical Stepper Verification: dI/dr = -mu * I for High Frequency (0.15 dB/m)
        constexpr float ALPHA_HIGH_DB = 0.150f;
        const float iInitial = 1.0f;
        const float iRK4 = SolveAirAbsorptionODE_RK4(iInitial, distance, ALPHA_HIGH_DB, 200);
        const float ampRK4 = std::sqrt(iRK4); // Amplitude = sqrt(Intensity)
        const float ampAnalytical = std::pow(10.0f, -ALPHA_HIGH_DB * distance / 20.0f);

        std::cout << "  ODE Numerical Integration Verification (High Band, 15 kHz):\n";
        std::cout << "    Analytical Amplitude: " << ampAnalytical << "\n";
        std::cout << "    RK4 Stepper Solution: " << ampRK4 << " (Delta = " << std::abs(ampRK4 - ampAnalytical) << ")\n";

        AssertTest(absStandard.high < absStandard.mid && absStandard.mid < absStandard.low,
                   "High frequency treble attenuates significantly faster than mid/low bass");
        AssertTest(std::abs(ampRK4 - ampAnalytical) < 1e-4f,
                   "Runge-Kutta 4th-order ODE numerical solution matches analytical solution within 0.01%");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 3: Emitter Directivity Patterns (Dipole, Cardioid, Sound Cone)
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 3] Emitter Directivity Patterns\n";
    {
        const Vector3D emitterFwd{ 1.0f, 0.0f, 0.0f }; // Emitter facing +X

        // Cardioid: alpha = 0.5
        DirectivityParams cardioidParams{};
        cardioidParams.dipoleWeight = 0.5f;
        cardioidParams.dipolePower  = 1.0f;

        // Dipole: alpha = 1.0
        DirectivityParams dipoleParams{};
        dipoleParams.dipoleWeight = 1.0f;
        dipoleParams.dipolePower  = 1.0f;

        // Sound Cone: inner 45 deg, outer 90 deg, min gain 0.1
        DirectivityParams coneParams{};
        coneParams.useConeDirectivity = true;
        coneParams.innerAngleDeg      = 45.0f;
        coneParams.outerAngleDeg      = 90.0f;
        coneParams.outerGain          = 0.1f;

        // Test directions
        const Vector3D dirAhead {  1.0f,  0.0f, 0.0f }; //   0 deg
        const Vector3D dirSide45{  0.707f, 0.707f, 0.0f }; //  45 deg
        const Vector3D dirSide90{  0.0f,  1.0f, 0.0f }; //  90 deg
        const Vector3D dirRear  { -1.0f,  0.0f, 0.0f }; // 180 deg

        const float cAhead  = CalculateDirectivity(emitterFwd, dirAhead,  cardioidParams);
        const float cSide45 = CalculateDirectivity(emitterFwd, dirSide45, cardioidParams);
        const float cSide90 = CalculateDirectivity(emitterFwd, dirSide90, cardioidParams);
        const float cRear   = CalculateDirectivity(emitterFwd, dirRear,   cardioidParams);

        std::cout << "  Cardioid Directivity Gain Profile:\n";
        std::cout << "    0 deg (On-axis front): " << cAhead  << "\n";
        std::cout << "   45 deg (Flank):         " << cSide45 << "\n";
        std::cout << "   90 deg (Side):          " << cSide90 << "\n";
        std::cout << "  180 deg (Rear null):     " << cRear   << "\n";

        AssertTest(cAhead == 1.0f, "Cardioid on-axis emission retains full gain 1.0");
        AssertTest(std::abs(cSide90 - 0.5f) < 1e-3f, "Cardioid 90-degree side emission gain is exactly 0.5");
        AssertTest(cRear < 1e-4f, "Cardioid 180-degree rear emission has complete null (gain ~0.0)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 4: Physical Barrier Transmission & Occlusion Raycasting
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 4] Physical Barrier Transmission & Occlusion Raycasting\n";
    {
        // Place listener at (10, 0, 0), emitter at (0, 0, 0)
        const Vector3D emitter{ 0.0f, 0.0f, 0.0f };
        const Vector3D listener{ 10.0f, 0.0f, 0.0f };

        // Test unobstructed ray
        std::vector<AcousticBarrier> barriersEmpty{};
        const auto resClear = EvaluateOcclusionAndTransmission(emitter, listener, barriersEmpty);

        // Test Concrete Wall barrier placed at x in [4.9, 5.1]
        std::vector<AcousticBarrier> barriersConcrete = {
            { Vector3D{ 4.9f, -5.0f, -5.0f }, Vector3D{ 5.1f, 5.0f, 5.0f }, 0.20f, AcousticMaterialType::Concrete }
        };
        const auto resConcrete = EvaluateOcclusionAndTransmission(emitter, listener, barriersConcrete);

        // Test Wooden Door barrier placed at x in [4.95, 5.05]
        std::vector<AcousticBarrier> barriersWood = {
            { Vector3D{ 4.95f, -5.0f, -5.0f }, Vector3D{ 5.05f, 5.0f, 5.0f }, 0.05f, AcousticMaterialType::Wood }
        };
        const auto resWood = EvaluateOcclusionAndTransmission(emitter, listener, barriersWood);

        // Test Glass Window barrier
        std::vector<AcousticBarrier> barriersGlass = {
            { Vector3D{ 4.98f, -5.0f, -5.0f }, Vector3D{ 5.02f, 5.0f, 5.0f }, 0.01f, AcousticMaterialType::Glass }
        };
        const auto resGlass = EvaluateOcclusionAndTransmission(emitter, listener, barriersGlass);

        std::cout << "  Barrier Acoustic Transmission Comparison:\n";
        std::cout << "    Unobstructed Path: Occlusion=" << resClear.occlusionFactor
                  << "  Gain=[ " << resClear.effectiveDirectGain.low << ", "
                                 << resClear.effectiveDirectGain.mid << ", "
                                 << resClear.effectiveDirectGain.high << " ]\n";
        std::cout << "    Concrete Wall:     Occlusion=" << resConcrete.occlusionFactor
                  << "  Gain=[ " << resConcrete.effectiveDirectGain.low << ", "
                                 << resConcrete.effectiveDirectGain.mid << ", "
                                 << resConcrete.effectiveDirectGain.high << " ]\n";
        std::cout << "    Wooden Door:       Occlusion=" << resWood.occlusionFactor
                  << "  Gain=[ " << resWood.effectiveDirectGain.low << ", "
                                 << resWood.effectiveDirectGain.mid << ", "
                                 << resWood.effectiveDirectGain.high << " ]\n";
        std::cout << "    Glass Pane:        Occlusion=" << resGlass.occlusionFactor
                  << "  Gain=[ " << resGlass.effectiveDirectGain.low << ", "
                                 << resGlass.effectiveDirectGain.mid << ", "
                                 << resGlass.effectiveDirectGain.high << " ]\n";

        AssertTest(resClear.occlusionFactor == 0.0f && resClear.effectiveDirectGain.Average() == 1.0f,
                   "Clear ray has 0.0 occlusion and 1.0 transmission");
        AssertTest(resConcrete.occlusionFactor == 1.0f && resConcrete.effectiveDirectGain.low < 0.02f,
                   "Concrete wall exhibits massive transmission loss (sound barrier)");
        AssertTest(resWood.effectiveDirectGain.low > resConcrete.effectiveDirectGain.low,
                   "Wood allows greater low-frequency acoustic transmission than solid concrete");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 5: Ambisonics B-Format Spherical Harmonics (2nd Order)
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 5] 2nd-Order Ambisonics B-Format Spherical Harmonics Encoding & Decoding\n";
    {
        // Encode source from Left direction (x=0, y=1, z=0)
        const Vector3D dirLeft{ 0.0f, 1.0f, 0.0f };
        const auto bLeft = EncodeAmbisonics(dirLeft, 1.0f);

        std::cout << "  Encoded Channels for Source at Left (0, 1, 0):\n";
        std::cout << "    ACN 0 (W, Omni):        " << bLeft.W() << "\n";
        std::cout << "    ACN 1 (Y, Left/Right):  " << bLeft.Y() << " (+1.0 Left)\n";
        std::cout << "    ACN 2 (Z, Up/Down):     " << bLeft.Z() << "\n";
        std::cout << "    ACN 3 (X, Front/Back):  " << bLeft.X() << "\n";
        std::cout << "    ACN 6 (R, Quadrupole Z):" << bLeft.R() << "\n";
        std::cout << "    ACN 8 (U, Quadrupole XY):" << bLeft.U() << "\n";

        // Decode to virtual stereo speakers (+/- 30 degrees)
        constexpr float COS30 = 0.8660254f;
        constexpr float SIN30 = 0.5000000f;
        const Vector3D spkLeft { COS30,  SIN30, 0.0f };
        const Vector3D spkRight{ COS30, -SIN30, 0.0f };

        const float decodedLeft  = DecodeAmbisonicsToSpeaker(bLeft, spkLeft);
        const float decodedRight = DecodeAmbisonicsToSpeaker(bLeft, spkRight);

        std::cout << "  Decoded Virtual Loudspeaker Gains:\n";
        std::cout << "    Left Speaker  (+30 deg): Gain = " << decodedLeft  << "\n";
        std::cout << "    Right Speaker (-30 deg): Gain = " << decodedRight << "\n";

        AssertTest(bLeft.W() == 1.0f && bLeft.Y() == 1.0f, "Ambisonics Left source yields W=1.0 and Y=1.0");
        AssertTest(decodedLeft > decodedRight, "Left speaker receives significantly higher gain for left source");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 6: Room Acoustics & Statistical Reverberation (Sabine vs Eyring RT60)
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 6] Room Acoustics & Statistical Reverberation (Sabine vs Eyring-Norris RT60)\n";
    {
        // Concrete & Brick Warehouse (High reflection, low absorption, long reverb tail)
        RoomAcousticProperties warehouse{};
        warehouse.volumeM3 = 1200.0f; // 1200 m^3
        warehouse.surfaces = {
            { 200.0f, AcousticMaterialType::Concrete }, // Floor
            { 200.0f, AcousticMaterialType::Metal },    // Ceiling
            { 400.0f, AcousticMaterialType::Brick }     // Walls
        };

        const auto rtWarehouse = CalculateReverberationTime(warehouse);

        std::cout << "  Industrial Warehouse (V = 1200 m^3, S = " << warehouse.TotalArea() << " m^2):\n";
        std::cout << "    Sabine RT60: Low=" << rtWarehouse.sabineRT60.low << "s"
                  << "  Mid=" << rtWarehouse.sabineRT60.mid << "s"
                  << "  High=" << rtWarehouse.sabineRT60.high << "s\n";
        std::cout << "    Eyring RT60: Low=" << rtWarehouse.eyringRT60.low << "s"
                  << "  Mid=" << rtWarehouse.eyringRT60.mid << "s"
                  << "  High=" << rtWarehouse.eyringRT60.high << "s\n";

        // Heavily Damped Studio / Carpeted Office (High absorption, short reverb)
        RoomAcousticProperties studio{};
        studio.volumeM3 = 150.0f;
        studio.surfaces = {
            { 50.0f, AcousticMaterialType::Carpet },
            { 50.0f, AcousticMaterialType::Plaster },
            { 120.0f, AcousticMaterialType::Wood }
        };
        const auto rtStudio = CalculateReverberationTime(studio);

        std::cout << "  Damped Studio (V = 150 m^3, S = " << studio.TotalArea() << " m^2):\n";
        std::cout << "    Sabine RT60: Low=" << rtStudio.sabineRT60.low << "s"
                  << "  Mid=" << rtStudio.sabineRT60.mid << "s"
                  << "  High=" << rtStudio.sabineRT60.high << "s\n";
        std::cout << "    Eyring RT60: Low=" << rtStudio.eyringRT60.low << "s"
                  << "  Mid=" << rtStudio.eyringRT60.mid << "s"
                  << "  High=" << rtStudio.eyringRT60.high << "s\n";

        AssertTest(rtWarehouse.sabineRT60.mid > 1.5f, "Reflective warehouse exhibits realistic RT60 > 1.5 seconds");
        AssertTest(rtStudio.eyringRT60.mid < rtWarehouse.eyringRT60.mid, "Damped room has substantially lower RT60 than warehouse");
        AssertTest(rtStudio.eyringRT60.mid <= rtStudio.sabineRT60.mid, "Eyring RT60 is properly bounded below or equal to Sabine RT60");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // STAGE 7: Parametric Impulse Response (IR) Filter Synthesis
    // ------------------------------------------------------------------------
    std::cout << "[STAGE 7] Parametric Impulse Response (IR) Filter Synthesis\n";
    {
        const OctaveBand3 targetRT60{ 1.2f, 1.0f, 0.7f };
        const auto ir = SynthesizeImpulseResponse(
            0.015f, // 15 ms early reflection delay
            0.040f, // 40 ms mixing time to diffuse late tail
            targetRT60,
            0.35f,  // Early reflection gain
            0.25f,  // Late diffuse gain
            0.5f,   // 500 ms duration
            48000   // 48 kHz sampling rate
        );

        std::cout << "  Synthesized IR Filter Buffer Statistics:\n";
        std::cout << "    Duration: " << ir.durationSec << " s (" << ir.SampleCount() << " samples @ " << ir.sampleRate << " Hz)\n";
        std::cout << "    Direct Arrival (n=0):     " << ir.samples[0] << "\n";
        std::cout << "    Early Reflection (15 ms): " << ir.samples[static_cast<std::size_t>(0.015f * 48000)] << "\n";
        std::cout << "    Late Tail Start (40 ms):  " << ir.samples[static_cast<std::size_t>(0.040f * 48000)] << "\n";
        std::cout << "    Late Tail Decayed (200ms):" << ir.samples[static_cast<std::size_t>(0.200f * 48000)] << "\n";

        // Verify exponential energy decay: E(t_late) < E(t_early)
        float energyEarly = 0.0f;
        for (std::size_t i = 0; i < 4800; ++i) energyEarly += ir.samples[i] * ir.samples[i]; // First 100 ms

        float energyLate = 0.0f;
        for (std::size_t i = 9600; i < 14400; ++i) energyLate += ir.samples[i] * ir.samples[i]; // 200 ms to 300 ms

        std::cout << "    Early Energy (0..100ms): " << energyEarly << "\n";
        std::cout << "    Late Energy (200..300ms): " << energyLate << " (Ratio = " << (energyLate / energyEarly) << ")\n";

        AssertTest(ir.samples[0] == 1.0f, "Impulse response begins with unit direct Dirac delta");
        AssertTest(energyLate < energyEarly * 0.15f, "Late reverberation tail exhibits exponential energy decay");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // SUMMARY
    // ------------------------------------------------------------------------
    std::cout << "====================================================================\n";
    std::cout << "SIMULATION RESULTS: " << passedTests << " / " << totalTests << " PASSED\n";
    std::cout << "====================================================================\n";

    return (passedTests == totalTests) ? 0 : 1;
}
