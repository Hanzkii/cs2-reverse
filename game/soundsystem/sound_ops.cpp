// ============================================================================
// Source 2 Spatial Audio, Acoustic Wave Propagation & Operator Dispatcher
// Clean-Room Reconstructed Implementation
// ============================================================================

#include "sound_ops.h"

namespace cs2::soundsystem {

// ----------------------------------------------------------------------------
// Static Material Properties Registry
// Reference: Steam Audio / ISO 9613-1 / VDC Architectural Acoustics
// ----------------------------------------------------------------------------
static const MaterialAcousticProperties s_MaterialTable[] = {
    // Generic
    { "Generic",  { 0.10f, 0.10f, 0.10f }, { 0.050f, 0.030f, 0.010f }, 0.05f },
    // Concrete (Dense, highly reflective, low absorption, high transmission loss)
    { "Concrete", { 0.015f, 0.025f, 0.040f }, { 0.010f, 0.005f, 0.001f }, 0.05f },
    // Brick (Structural masonry, low absorption, moderate transmission loss)
    { "Brick",    { 0.030f, 0.040f, 0.070f }, { 0.020f, 0.010f, 0.004f }, 0.08f },
    // Wood (Plywood / timber partition, moderate mid absorption, moderate transmission)
    { "Wood",     { 0.150f, 0.110f, 0.100f }, { 0.120f, 0.080f, 0.040f }, 0.12f },
    // Glass (Window pane, high bass transmission, low high transmission, specular reflection)
    { "Glass",    { 0.180f, 0.060f, 0.030f }, { 0.200f, 0.100f, 0.030f }, 0.02f },
    // Metal (Rigid sheet metal, very low absorption, high specular reflection)
    { "Metal",    { 0.050f, 0.040f, 0.020f }, { 0.030f, 0.015f, 0.005f }, 0.15f },
    // Plaster (Drywall / gypsum board, moderate absorption)
    { "Plaster",  { 0.100f, 0.080f, 0.050f }, { 0.080f, 0.040f, 0.015f }, 0.06f },
    // Carpet (High treble absorption, deadens high frequencies)
    { "Carpet",   { 0.050f, 0.300f, 0.600f }, { 0.040f, 0.015f, 0.003f }, 0.20f }
};

const MaterialAcousticProperties& AcousticWaveSolver::GetMaterialProperties(AcousticMaterialType material) noexcept {
    const auto idx = static_cast<std::size_t>(material);
    if (idx < static_cast<std::size_t>(AcousticMaterialType::Count)) {
        return s_MaterialTable[idx];
    }
    return s_MaterialTable[0];
}

// ----------------------------------------------------------------------------
// AcousticBarrier Geometric Ray Intersection (Slab / AABB Method)
// ----------------------------------------------------------------------------
bool AcousticBarrier::IntersectsRay(
    const shared::Vector3D& rayOrigin,
    const shared::Vector3D& rayDir,
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
            // Ray parallel to slab: if origin outside, no intersection
            if (origins[i] < mins[i] || origins[i] > maxs[i]) {
                return false;
            }
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

// ----------------------------------------------------------------------------
// Room Acoustic Properties Implementation
// ----------------------------------------------------------------------------
float RoomAcousticProperties::TotalSurfaceArea() const noexcept {
    float total = 0.0f;
    for (const auto& s : surfaces) {
        total += s.areaM2;
    }
    return (total > 1e-4f) ? total : 1.0f;
}

OctaveBand3 RoomAcousticProperties::TotalAbsorptionArea() const noexcept {
    OctaveBand3 totalA{ 0.0f, 0.0f, 0.0f };
    for (const auto& s : surfaces) {
        OctaveBand3 alpha = s.customAbsorption;
        if (s.material != AcousticMaterialType::Generic) {
            alpha = AcousticWaveSolver::GetMaterialProperties(s.material).absorption;
        }
        totalA += alpha * s.areaM2;
    }
    return totalA;
}

OctaveBand3 RoomAcousticProperties::AverageAbsorption() const noexcept {
    const float totalS = TotalSurfaceArea();
    const OctaveBand3 totalA = TotalAbsorptionArea();
    return totalA * (1.0f / totalS);
}

// ----------------------------------------------------------------------------
// Listener Orientation Transformations
// ----------------------------------------------------------------------------
shared::Vector3D ListenerOrientation::WorldToLocal(const shared::Vector3D& worldPos) const noexcept {
    const shared::Vector3D delta = worldPos - origin;
    return {
        right.Dot(delta),   // Local X: Right
        forward.Dot(delta), // Local Y: Ahead
        up.Dot(delta)       // Local Z: Up
    };
}

shared::Vector3D ListenerOrientation::LocalToWorld(const shared::Vector3D& localPos) const noexcept {
    return origin + (right * localPos.x) + (forward * localPos.y) + (up * localPos.z);
}

// ----------------------------------------------------------------------------
// Direct Sound Path: Distance Attenuation
// ----------------------------------------------------------------------------
float AcousticWaveSolver::CalculateDistanceAttenuation(
    float distanceMeters,
    const DistanceAttenuationParams& params
) noexcept {
    if (distanceMeters <= params.minDistance) {
        return 1.0f;
    }
    if (distanceMeters >= params.maxDistance) {
        return 0.0f;
    }

    const float dRange = params.maxDistance - params.minDistance;
    if (dRange < 1e-4f) return 0.0f;

    switch (params.model) {
        case DistanceAttenuationModel::InverseDistance: {
            // Spherical inverse distance law: g(d) = d_min / d, smoothly windowed to 0 at d_max
            const float invLaw = params.minDistance / distanceMeters;
            const float window = (params.maxDistance - distanceMeters) / dRange;
            const float gain = invLaw * std::pow(window, params.rolloffFactor);
            return std::clamp(gain, 0.0f, 1.0f);
        }
        case DistanceAttenuationModel::LinearFalloff: {
            const float fraction = (distanceMeters - params.minDistance) / dRange;
            return std::clamp(1.0f - fraction, 0.0f, 1.0f);
        }
        case DistanceAttenuationModel::Exponential: {
            const float fraction = (distanceMeters - params.minDistance) / dRange;
            const float gain = std::exp(-fraction * 3.0f * params.rolloffFactor) *
                               ((params.maxDistance - distanceMeters) / dRange);
            return std::clamp(gain, 0.0f, 1.0f);
        }
        default:
            return 1.0f;
    }
}

// ----------------------------------------------------------------------------
// Direct Sound Path: Atmospheric Air Absorption
// Reference: ISO 9613-1 / Steam Audio 3-band approximation
// Low (400 Hz), Mid (2.5 kHz), High (15 kHz)
// ----------------------------------------------------------------------------
OctaveBand3 AcousticWaveSolver::CalculateAirAbsorption(
    float distanceMeters,
    float temperatureC,
    float relativeHumidityPct
) noexcept {
    if (distanceMeters <= 0.0f) {
        return { 1.0f, 1.0f, 1.0f };
    }

    // Standard baseline attenuation coefficients in dB/meter at 20°C, 50% RH
    constexpr float BASE_ALPHA_LOW  = 0.0020f; // dB/m @ 400 Hz
    constexpr float BASE_ALPHA_MID  = 0.0150f; // dB/m @ 2500 Hz
    constexpr float BASE_ALPHA_HIGH = 0.1500f; // dB/m @ 15000 Hz

    // Temperature & relative humidity scaling factors
    const float tKelvin = std::max(temperatureC + 273.15f, 250.0f);
    const float tRel    = 293.15f / tKelvin;
    const float rhClamped = std::clamp(relativeHumidityPct, 10.0f, 100.0f);
    const float hRel    = 50.0f / rhClamped;

    // Shift relaxation frequencies with humidity and temperature
    const float alphaLow  = BASE_ALPHA_LOW  * std::sqrt(tRel) * (0.8f + 0.2f * hRel);
    const float alphaMid  = BASE_ALPHA_MID  * tRel            * (0.6f + 0.4f * hRel);
    const float alphaHigh = BASE_ALPHA_HIGH * (tRel * tRel)   * (0.4f + 0.6f * hRel);

    // Linear amplitude gain: g = 10^(-alpha * distance / 20)
    const float gainLow  = std::pow(10.0f, -alphaLow  * distanceMeters / 20.0f);
    const float gainMid  = std::pow(10.0f, -alphaMid  * distanceMeters / 20.0f);
    const float gainHigh = std::pow(10.0f, -alphaHigh * distanceMeters / 20.0f);

    return {
        std::clamp(gainLow,  0.0f, 1.0f),
        std::clamp(gainMid,  0.0f, 1.0f),
        std::clamp(gainHigh, 0.0f, 1.0f)
    };
}

// ----------------------------------------------------------------------------
// Runge-Kutta 4th Order ODE Numerical Solver for Wave Intensity Decay
// dI/dr = -mu * I, where mu = alpha_dB * ln(10) / 10
// ----------------------------------------------------------------------------
float AcousticWaveSolver::SolveAirAbsorptionODE_RK4(
    float initialIntensity,
    float distanceMeters,
    float absorptionCoeffDbPerMeter,
    std::uint32_t stepCount
) noexcept {
    if (distanceMeters <= 0.0f || stepCount == 0) {
        return initialIntensity;
    }

    // Convert attenuation coefficient from dB/m to power absorption coefficient mu (1/m)
    // 10 * log10(I0 / I) = alpha * r => ln(I0 / I) = alpha * ln(10) / 10 * r
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
// Direct Sound Path: Emitter Directivity (Dipole & Sound Cone Models)
// ----------------------------------------------------------------------------
float AcousticWaveSolver::CalculateDirectivity(
    const shared::Vector3D& emitterForward,
    const shared::Vector3D& directionToListener,
    const DirectivityParams& params
) noexcept {
    const float forwardLen = emitterForward.Length();
    const float dirLen     = directionToListener.Length();

    if (forwardLen < 1e-4f || dirLen < 1e-4f) {
        return 1.0f;
    }

    // Cosine of emission angle between emitter forward axis and listener trajectory
    const float cosAngle = std::clamp(emitterForward.Dot(directionToListener) / (forwardLen * dirLen), -1.0f, 1.0f);

    if (params.useConeDirectivity) {
        // Sound Cone Model with inner/outer angles (degrees)
        const float angleRad = std::acos(cosAngle);
        const float angleDeg = angleRad * (180.0f / std::numbers::pi_v<float>);

        if (angleDeg <= params.innerAngleDeg) {
            return 1.0f;
        }
        if (angleDeg >= params.outerAngleDeg) {
            return params.outerGain;
        }

        // Cubic Hermite smoothstep between inner and outer angle thresholds
        const float t = (angleDeg - params.innerAngleDeg) / (params.outerAngleDeg - params.innerAngleDeg);
        const float smooth = t * t * (3.0f - 2.0f * t);
        return 1.0f - (1.0f - params.outerGain) * smooth;
    }

    // Steam Audio Dipole Model: D(theta) = |(1 - alpha) + alpha * cos(theta)|^p
    const float alpha = std::clamp(params.dipoleWeight, 0.0f, 1.0f);
    const float term  = (1.0f - alpha) + alpha * cosAngle;
    const float power = std::max(params.dipolePower, 1.0f);

    const float gain = std::pow(std::abs(term), power);
    return std::clamp(gain, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// Direct Sound Path: Physical Acoustic Barrier Transmission & Occlusion
// ----------------------------------------------------------------------------
OcclusionTransmissionResult AcousticWaveSolver::EvaluateOcclusionAndTransmission(
    const shared::Vector3D& emitterPos,
    const shared::Vector3D& listenerPos,
    const std::vector<AcousticBarrier>& barriers
) noexcept {
    OcclusionTransmissionResult result{};
    const shared::Vector3D rayVec = listenerPos - emitterPos;
    const float dist = rayVec.Length();
    result.distanceMeters = dist;

    if (dist < 1e-4f || barriers.empty()) {
        return result;
    }

    const shared::Vector3D rayDir = rayVec / dist;

    // Track composite transmission across intersecting barriers
    OctaveBand3 netTransmission{ 1.0f, 1.0f, 1.0f };
    float totalOcclusion = 0.0f;
    int barrierHitCount = 0;

    for (const auto& barrier : barriers) {
        float tEnter = 0.0f;
        float tExit  = 0.0f;

        if (barrier.IntersectsRay(emitterPos, rayDir, dist, tEnter, tExit)) {
            // Penetration segment length along acoustic ray (meters)
            const float traversalDist = (tExit - tEnter) * dist;
            if (traversalDist > 1e-4f) {
                barrierHitCount++;
                totalOcclusion = 1.0f;

                const auto& matProps = GetMaterialProperties(barrier.material);
                // Transmission loss scales exponentially with thickness: tau_eff = tau_base * exp(-kappa * t)
                const float thickScale = traversalDist / std::max(barrier.thicknessMeters, 0.05f);
                const float decay = std::clamp(std::exp(-0.5f * thickScale), 0.05f, 1.0f);

                netTransmission = netTransmission * (matProps.transmission * decay);
            }
        }
    }

    if (barrierHitCount > 0) {
        result.occlusionFactor   = totalOcclusion;
        result.transmissionGain  = netTransmission;
        // Direct sound equals occluded portion transmitted plus unoccluded portion
        result.effectiveDirectGain = netTransmission;
    } else {
        result.occlusionFactor   = 0.0f;
        result.transmissionGain  = { 1.0f, 1.0f, 1.0f };
        result.effectiveDirectGain = { 1.0f, 1.0f, 1.0f };
    }

    return result;
}

// ----------------------------------------------------------------------------
// Indirect Sound Path: Ambisonics B-Format Spherical Harmonics
// Implementation conforms to SN3D normalization and ACN indexing:
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
AmbisonicsBFormat AcousticWaveSolver::EncodeAmbisonics(
    const shared::Vector3D& directionNormalized,
    float amplitude,
    AmbisonicsOrder order
) noexcept {
    AmbisonicsBFormat b{};
    const float x = directionNormalized.x;
    const float y = directionNormalized.y;
    const float z = directionNormalized.z;

    // 0th Order: Omnidirectional monopole W
    b.channels[0] = amplitude * 1.0f;

    if (order >= AmbisonicsOrder::Order1) {
        // 1st Order Dipoles: Y (Left/Right), Z (Up/Down), X (Front/Back)
        b.channels[1] = amplitude * y;
        b.channels[2] = amplitude * z;
        b.channels[3] = amplitude * x;
    }

    if (order >= AmbisonicsOrder::Order2) {
        // 2nd Order Quadrupoles
        constexpr float SQRT3       = 1.73205080757f;
        constexpr float HALF_SQRT3  = 0.86602540378f;

        b.channels[4] = amplitude * (SQRT3 * x * y);
        b.channels[5] = amplitude * (SQRT3 * y * z);
        b.channels[6] = amplitude * (0.5f * (3.0f * z * z - 1.0f));
        b.channels[7] = amplitude * (SQRT3 * x * z);
        b.channels[8] = amplitude * (HALF_SQRT3 * (x * x - y * y));
    }

    return b;
}

float AcousticWaveSolver::DecodeAmbisonicsToSpeaker(
    const AmbisonicsBFormat& bFormat,
    const shared::Vector3D& speakerDirectionNormalized,
    AmbisonicsOrder order
) noexcept {
    const AmbisonicsBFormat spkEnc = EncodeAmbisonics(speakerDirectionNormalized, 1.0f, order);

    // Basic projection mode-matching decoding
    // Standard SN3D max-rE order weights for optimal energy preservation
    constexpr float ORDER_WEIGHTS[3] = { 1.0f, 0.57735f, 0.35355f };

    float speakerGain = bFormat.channels[0] * spkEnc.channels[0] * ORDER_WEIGHTS[0];

    if (order >= AmbisonicsOrder::Order1) {
        float order1Sum = 0.0f;
        for (std::size_t i = 1; i <= 3; ++i) {
            order1Sum += bFormat.channels[i] * spkEnc.channels[i];
        }
        speakerGain += order1Sum * ORDER_WEIGHTS[1];
    }

    if (order >= AmbisonicsOrder::Order2) {
        float order2Sum = 0.0f;
        for (std::size_t i = 4; i <= 8; ++i) {
            order2Sum += bFormat.channels[i] * spkEnc.channels[i];
        }
        speakerGain += order2Sum * ORDER_WEIGHTS[2];
    }

    return speakerGain;
}

void AcousticWaveSolver::DecodeAmbisonicsStereo(
    const AmbisonicsBFormat& bFormat,
    float& outLeft,
    float& outRight
) noexcept {
    // Virtual stereo loudspeaker array positioned at +/- 30 degrees azimuth
    // Left:  azimuth = +30 deg => x = cos(30), y = +sin(30), z = 0
    // Right: azimuth = -30 deg => x = cos(30), y = -sin(30), z = 0
    constexpr float COS30 = 0.86602540378f;
    constexpr float SIN30 = 0.50000000000f;

    const shared::Vector3D spkLeft { COS30,  SIN30, 0.0f };
    const shared::Vector3D spkRight{ COS30, -SIN30, 0.0f };

    outLeft  = DecodeAmbisonicsToSpeaker(bFormat, spkLeft,  AmbisonicsOrder::Order2);
    outRight = DecodeAmbisonicsToSpeaker(bFormat, spkRight, AmbisonicsOrder::Order2);
}

// ----------------------------------------------------------------------------
// Indirect Sound Path: Statistical Reverberation (Sabine & Eyring-Norris RT60)
// Sabine: RT60 = 0.161 * V / sum(S_i * alpha_i)
// Eyring: RT60 = 0.161 * V / (-S * ln(1 - mean(alpha)))
// ----------------------------------------------------------------------------
ReverberationTimeRT60 AcousticWaveSolver::CalculateReverberationTime(
    const RoomAcousticProperties& room
) noexcept {
    ReverberationTimeRT60 rt{};
    const float V = std::max(room.volumeM3, 0.1f);
    const float S = room.TotalSurfaceArea();
    const OctaveBand3 A = room.TotalAbsorptionArea();
    const OctaveBand3 meanAlpha = room.AverageAbsorption();

    constexpr float K_SABINE = 0.161f; // Metric Sabine acoustic constant: 24 * ln(10) / c_sound

    // 1. Classic Sabine formulation
    rt.sabineRT60.low  = K_SABINE * V / std::max(A.low,  0.01f);
    rt.sabineRT60.mid  = K_SABINE * V / std::max(A.mid,  0.01f);
    rt.sabineRT60.high = K_SABINE * V / std::max(A.high, 0.01f);

    // 2. Eyring-Norris formulation (well-behaved across absorptive surfaces alpha -> 1)
    auto SolveEyring = [&](float alpha) noexcept -> float {
        const float alphaClamped = std::clamp(alpha, 0.001f, 0.990f);
        const float denominator = -S * std::log(1.0f - alphaClamped);
        return K_SABINE * V / std::max(denominator, 0.01f);
    };

    rt.eyringRT60.low  = SolveEyring(meanAlpha.low);
    rt.eyringRT60.mid  = SolveEyring(meanAlpha.mid);
    rt.eyringRT60.high = SolveEyring(meanAlpha.high);

    return rt;
}

// ----------------------------------------------------------------------------
// Indirect Sound Path: Parametric Impulse Response Filter Synthesis
// Synthesizes a discrete time-domain FIR filter buffer h[n] combining:
// - Direct sound delta impulse at t=0
// - Sparse early reflection arrivals (specular rays)
// - Exponentially decaying diffuse late reverberation tail
// ----------------------------------------------------------------------------
ImpulseResponseBuffer AcousticWaveSolver::SynthesizeImpulseResponse(
    const ImpulseResponseSynthesisParams& params,
    float durationSeconds
) {
    ImpulseResponseBuffer buf{};
    buf.sampleRate  = params.sampleRate;
    buf.durationSec = durationSeconds;

    const std::size_t totalSamples = static_cast<std::size_t>(durationSeconds * static_cast<float>(params.sampleRate));
    buf.samples.assign(totalSamples, 0.0f);

    if (totalSamples == 0) return buf;

    // 1. Direct Sound Arrival at t = 0
    buf.samples[0] = 1.0f;

    // 2. Discrete Early Reflections Cluster
    const std::size_t nEarly1 = static_cast<std::size_t>(params.earlyReflectionDelaySec * static_cast<float>(params.sampleRate));
    const std::size_t nEarly2 = static_cast<std::size_t>(params.earlyReflectionDelaySec * 1.6f * static_cast<float>(params.sampleRate));
    const std::size_t nEarly3 = static_cast<std::size_t>(params.earlyReflectionDelaySec * 2.3f * static_cast<float>(params.sampleRate));

    if (nEarly1 < totalSamples) buf.samples[nEarly1] += params.earlyGain * 0.85f;
    if (nEarly2 < totalSamples) buf.samples[nEarly2] -= params.earlyGain * 0.65f;
    if (nEarly3 < totalSamples) buf.samples[nEarly3] += params.earlyGain * 0.45f;

    // 3. Late Reverberation Decay Tail (Diffuse Sound Field)
    // Energy decay: E(t) = exp(-13.8155 * t / RT60) => Amplitude: A(t) = exp(-6.90776 * t / RT60)
    const float rt60Mean = params.rt60.Average();
    const float tauDecay = (rt60Mean > 0.05f) ? (rt60Mean / 6.90776f) : 0.1f;

    const std::size_t nMix = static_cast<std::size_t>(params.mixingTimeSec * static_cast<float>(params.sampleRate));

    // Linear Congruential Generator for reproducible, zero-dependency white noise
    std::uint32_t lcgState = 0x12345678;
    auto NextRand = [&lcgState]() noexcept -> float {
        lcgState = lcgState * 1664525u + 1013904223u;
        return (static_cast<float>(lcgState & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF)) * 2.0f - 1.0f;
    };

    for (std::size_t n = nMix; n < totalSamples; ++n) {
        const float t = static_cast<float>(n - nMix) / static_cast<float>(params.sampleRate);
        const float decay = std::exp(-t / tauDecay);
        const float noise = NextRand();

        buf.samples[n] += params.lateGain * decay * noise;
    }

    return buf;
}

// ----------------------------------------------------------------------------
// CSoundManager Implementation
// ----------------------------------------------------------------------------
CSoundManager& CSoundManager::Get() {
    static CSoundManager s_Instance;
    return s_Instance;
}

void CSoundManager::Initialize() {
    m_initialized = true;
    m_activeSounds.clear();
    m_barriers.clear();
    m_busVolumes.fill(1.0f);
    m_nextGuid = 1000;

    // Setup default listener at world origin
    m_listener.origin   = { 0.0f, 0.0f, 0.0f };
    m_listener.forward  = { 1.0f, 0.0f, 0.0f };
    m_listener.right    = { 0.0f, 1.0f, 0.0f };
    m_listener.up       = { 0.0f, 0.0f, 1.0f };
    m_listener.velocity = { 0.0f, 0.0f, 0.0f };

    // Setup default room acoustics (200 m^3 room)
    m_room.volumeM3 = 200.0f;
    m_room.surfaces = {
        { 40.0f,  AcousticMaterialType::Concrete, {} }, // Floor
        { 40.0f,  AcousticMaterialType::Plaster,  {} }, // Ceiling
        { 120.0f, AcousticMaterialType::Brick,    {} }  // Walls
    };
    UpdateReverberation();
}

void CSoundManager::Shutdown() {
    m_activeSounds.clear();
    m_barriers.clear();
    m_initialized = false;
}

void CSoundManager::UpdateReverberation() {
    m_rt60 = AcousticWaveSolver::CalculateReverberationTime(m_room);
}

void CSoundManager::SetListenerOrientation(
    const shared::Vector3D& origin,
    const shared::Vector3D& forward,
    const shared::Vector3D& up,
    const shared::Vector3D& velocity
) noexcept {
    m_listener.origin   = origin;
    m_listener.forward  = forward.Normalized();
    m_listener.up       = up.Normalized();
    m_listener.right    = m_listener.forward.Cross(m_listener.up).Normalized();
    m_listener.velocity = velocity;
}

interfaces::StartSoundEventInfo_t CSoundManager::CreateSpatialSoundInfo(
    const shared::Vector3D& origin,
    float volume,
    float pitch,
    shared::CEntityHandle entity
) noexcept {
    interfaces::StartSoundEventInfo_t info{};
    info.m_vecOrigin = origin;
    info.m_flVolume  = volume;
    info.m_flPitch   = pitch;
    info.m_hEntity   = entity;

    // Set 3x4 identity orientation matrix at origin
    info.m_matSpatial[0][0] = 1.0f;
    info.m_matSpatial[1][1] = 1.0f;
    info.m_matSpatial[2][2] = 1.0f;
    info.m_matSpatial[0][3] = origin.x;
    info.m_matSpatial[1][3] = origin.y;
    info.m_matSpatial[2][3] = origin.z;

    return info;
}

std::uint64_t CSoundManager::StartSoundEvent(
    std::string_view eventName,
    const interfaces::StartSoundEventInfo_t* pInfo,
    SoundBus bus
) {
    const std::uint32_t hash = HashSoundEvent(eventName);
    const std::uint64_t guid = StartSoundEventByHash(hash, pInfo, bus);
    if (guid != 0) {
        m_activeSounds[guid].eventName = std::string(eventName);
    }
    return guid;
}

std::uint64_t CSoundManager::StartSoundEventByHash(
    std::uint32_t nEventHash,
    const interfaces::StartSoundEventInfo_t* pInfo,
    SoundBus bus
) {
    if (!m_initialized) {
        Initialize();
    }

    const std::uint64_t guid = ++m_nextGuid;
    ActiveSoundInstance instance{};
    instance.guid       = guid;
    instance.eventHash  = nEventHash;
    instance.bus        = bus;
    instance.isPlaying  = true;

    if (pInfo) {
        instance.origin       = pInfo->m_vecOrigin;
        instance.entityHandle = pInfo->m_hEntity;
        instance.volume       = pInfo->m_flVolume;
        instance.pitch        = pInfo->m_flPitch;
    }

    // Initial acoustic evaluation
    const float dist = (instance.origin - m_listener.origin).Length();
    DistanceAttenuationParams distParams{};
    instance.distanceAttenuation = AcousticWaveSolver::CalculateDistanceAttenuation(dist, distParams);
    instance.airAbsorption        = AcousticWaveSolver::CalculateAirAbsorption(dist);

    const shared::Vector3D toListener = (m_listener.origin - instance.origin).Normalized();
    DirectivityParams dirParams{};
    instance.directivityGain = AcousticWaveSolver::CalculateDirectivity(m_listener.forward, toListener, dirParams);

    const auto occResult = AcousticWaveSolver::EvaluateOcclusionAndTransmission(
        instance.origin, m_listener.origin, m_barriers
    );
    instance.occlusionFactor  = occResult.occlusionFactor;
    instance.transmissionGain = occResult.transmissionGain;

    instance.ambisonics = AcousticWaveSolver::EncodeAmbisonics(toListener, instance.volume);

    m_activeSounds[guid] = instance;
    return guid;
}

void CSoundManager::StopSoundEvent(std::uint64_t guid, float /*flFadeTime*/) {
    auto it = m_activeSounds.find(guid);
    if (it != m_activeSounds.end()) {
        it->second.isPlaying = false;
        m_activeSounds.erase(it);
    }
}

void CSoundManager::SetSoundEventParam(std::uint64_t guid, std::string_view paramName, float flValue) {
    auto it = m_activeSounds.find(guid);
    if (it == m_activeSounds.end()) return;

    if (paramName == "volume") {
        it->second.volume = flValue;
    } else if (paramName == "pitch") {
        it->second.pitch = flValue;
    } else if (paramName == "priority") {
        it->second.priority = flValue;
    } else if (paramName == "reverb_send") {
        it->second.reverbSend = flValue;
    }
}

void CSoundManager::Update(float flDeltaTime) {
    if (!m_initialized) return;

    DistanceAttenuationParams distParams{};
    DirectivityParams dirParams{};

    for (auto it = m_activeSounds.begin(); it != m_activeSounds.end(); ) {
        if (!it->second.isPlaying) {
            it = m_activeSounds.erase(it);
            continue;
        }

        auto& sound = it->second;
        sound.playbackTime += flDeltaTime;

        // Vector from emitter to listener
        const shared::Vector3D delta = m_listener.origin - sound.origin;
        const float dist = delta.Length();
        const shared::Vector3D dirToListener = (dist > 1e-4f) ? (delta / dist) : shared::Vector3D{ 0.0f, 1.0f, 0.0f };

        // 1. Evaluate Direct Sound Path DSP
        sound.distanceAttenuation = AcousticWaveSolver::CalculateDistanceAttenuation(dist, distParams);
        sound.airAbsorption       = AcousticWaveSolver::CalculateAirAbsorption(dist);
        sound.directivityGain     = AcousticWaveSolver::CalculateDirectivity(shared::Vector3D{ 1.0f, 0.0f, 0.0f }, dirToListener, dirParams);

        // 2. Physical Barrier Raycasting (Occlusion & Material Transmission Loss)
        const auto occResult = AcousticWaveSolver::EvaluateOcclusionAndTransmission(
            sound.origin, m_listener.origin, m_barriers
        );
        sound.occlusionFactor  = occResult.occlusionFactor;
        sound.transmissionGain = occResult.transmissionGain;

        // 3. Ambisonics B-format Spherical Harmonics
        sound.ambisonics = AcousticWaveSolver::EncodeAmbisonics(dirToListener, sound.volume);

        // 4. Reverb send proportional to distance and room RT60
        sound.reverbSend = std::clamp(dist / (distParams.maxDistance * 0.5f), 0.1f, 0.8f);

        ++it;
    }
}

void CSoundManager::SetBusVolume(SoundBus bus, float volume) noexcept {
    const auto idx = static_cast<std::size_t>(bus);
    if (idx < m_busVolumes.size()) {
        m_busVolumes[idx] = std::clamp(volume, 0.0f, 2.0f);
    }
}

float CSoundManager::GetBusVolume(SoundBus bus) const noexcept {
    const auto idx = static_cast<std::size_t>(bus);
    if (idx < m_busVolumes.size()) {
        return m_busVolumes[idx];
    }
    return 1.0f;
}

const ActiveSoundInstance* CSoundManager::FindSound(std::uint64_t guid) const noexcept {
    auto it = m_activeSounds.find(guid);
    if (it != m_activeSounds.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace cs2::soundsystem
