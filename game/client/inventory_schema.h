#ifndef CS2_CLIENT_INVENTORY_SCHEMA_H
#define CS2_CLIENT_INVENTORY_SCHEMA_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <array>
#include <cmath>
#include <memory>

#include "sdk/item_schema.hpp"

// ============================================================================
// Source 2 Client Inventory & Econ Item Schema Implementation
// Architecture: x86-64 (MSVC ABI)
// Binary: client.dll
// Reconstructed & Clean-Room Implemented for Counter-Strike 2
// ============================================================================

namespace cs2::client {

// ============================================================================
// 1. Dynamic Attribute Key-Value Store
// ============================================================================

using AttributeValue = std::variant<float, std::int32_t, std::uint32_t, std::string>;

class AttributeKeyValueStore {
public:
    AttributeKeyValueStore() = default;
    ~AttributeKeyValueStore() = default;

    void SetAttribute(std::uint16_t defIndex, AttributeValue value);
    void SetAttributeByName(std::string_view name, AttributeValue value);

    [[nodiscard]] const AttributeValue* GetAttribute(std::uint16_t defIndex) const noexcept;
    [[nodiscard]] float GetAttributeFloat(std::uint16_t defIndex, float defaultValue = 0.0f) const noexcept;
    [[nodiscard]] std::int32_t GetAttributeInt(std::uint16_t defIndex, std::int32_t defaultValue = 0) const noexcept;
    [[nodiscard]] std::uint32_t GetAttributeUInt(std::uint16_t defIndex, std::uint32_t defaultValue = 0) const noexcept;
    [[nodiscard]] std::string_view GetAttributeString(std::uint16_t defIndex, std::string_view defaultValue = "") const noexcept;

    [[nodiscard]] bool HasAttribute(std::uint16_t defIndex) const noexcept;
    void RemoveAttribute(std::uint16_t defIndex) noexcept;
    void Clear() noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;

    static std::uint32_t HashAttributeName(std::string_view name) noexcept;

private:
    std::unordered_map<std::uint16_t, AttributeValue> m_indexedAttributes;
    std::unordered_map<std::uint32_t, AttributeValue> m_namedAttributes;
};

// ============================================================================
// 2. Procedural Texture Coordinate Mapper
// Transforms surface UVs based on seed (0..1000) and paint kit parameters.
// ============================================================================

struct ProceduralUVResult {
    float u_transformed = 0.0f;
    float v_transformed = 0.0f;
    float rotation_deg   = 0.0f;
    float offset_x       = 0.0f;
    float offset_y       = 0.0f;
    float scale          = 1.0f;
};

class ProceduralTextureMapper {
public:
    ProceduralTextureMapper() = default;

    /**
     * @brief Computes pseudo-random offset and rotation factors from an integer seed.
     */
    static void ComputeSeedFactors(
        std::int32_t seed,
        const sdk::CPaintKitDefinition& kit,
        float& out_offsetX,
        float& out_offsetY,
        float& out_rotationDeg) noexcept;

    /**
     * @brief Transforms base UV coordinates according to paint kit projection rules.
     */
    [[nodiscard]] static ProceduralUVResult TransformUV(
        std::int32_t seed,
        const sdk::CPaintKitDefinition& kit,
        float baseU,
        float baseV) noexcept;

    /**
     * @brief Computes a 2D affine transformation matrix [cos, -sin, sin, cos] and translation.
     */
    static void ComputeUVMatrix(
        std::int32_t seed,
        const sdk::CPaintKitDefinition& kit,
        std::array<float, 4>& out_rotationMatrix,
        std::array<float, 2>& out_translation) noexcept;
};

// ============================================================================
// 3. Wear Degradation & Surface Abrasion Pipeline
// ============================================================================

class WearDegradationPipeline {
public:
    WearDegradationPipeline() = default;

    /**
     * @brief Evaluates wear classification rating enum from float wear.
     */
    [[nodiscard]] static constexpr sdk::WearRating_t EvaluateWearCondition(float wear) noexcept {
        return sdk::EvaluateWearCondition(wear);
    }

    /**
     * @brief Returns human-readable wear rating name.
     */
    [[nodiscard]] static constexpr const char* GetWearRatingName(sdk::WearRating_t rating) noexcept {
        return sdk::GetWearRatingName(rating);
    }

    /**
     * @brief Clamps and remaps raw wear to weapon-specific paint kit bounds.
     */
    [[nodiscard]] static constexpr float RemapWear(float rawWear, float minWear, float maxWear) noexcept {
        return sdk::RemapWear(rawWear, minWear, maxWear);
    }

    /**
     * @brief Simulates dynamic abrasion wear degradation ODE:
     *        dw/dt = kappa * (1.0 - w)
     *        w(t + dt) = 1.0 - (1.0 - w(t)) * exp(-kappa * dt)
     */
    [[nodiscard]] static float SimulateWearDegradationODE(
        float initialWear,
        float abrasionRate,
        float durationSeconds,
        float timeStep = 0.01f) noexcept;

    /**
     * @brief Evaluates the surface abrasion wear mask alpha using smoothstep transition.
     *        Exposes the base patina or raw alloy beneath worn paint.
     */
    [[nodiscard]] static float EvaluateAbrasionAlpha(
        float remappedWear,
        float maskTextureSample,
        float edgeSmoothness = 0.04f) noexcept;
};

// ============================================================================
// 4. Instantiated Client Inventory Item
// Composite runtime object mirroring C_EconItemView with client accessories.
// ============================================================================

class InventoryItem {
public:
    InventoryItem() = default;
    InventoryItem(
        std::uint64_t itemID,
        std::uint16_t defIndex,
        sdk::EconQuality_t quality,
        sdk::EconRarity_t rarity,
        std::int32_t paintKitIndex = 0,
        std::int32_t seed = 0,
        float wear = 0.0f,
        std::int32_t statTrakCount = -1,
        std::string_view customName = "");

    [[nodiscard]] std::uint64_t GetItemID() const noexcept { return m_itemID; }
    [[nodiscard]] std::uint16_t GetDefinitionIndex() const noexcept { return m_defIndex; }
    [[nodiscard]] sdk::EconQuality_t GetQuality() const noexcept { return m_quality; }
    [[nodiscard]] sdk::EconRarity_t GetRarity() const noexcept { return m_rarity; }
    [[nodiscard]] std::int32_t GetPaintKitIndex() const noexcept { return m_paintKitIndex; }
    [[nodiscard]] std::int32_t GetSeed() const noexcept { return m_seed; }
    [[nodiscard]] float GetWear() const noexcept { return m_wear; }
    [[nodiscard]] std::int32_t GetStatTrakCount() const noexcept { return m_statTrakCount; }
    [[nodiscard]] std::string_view GetCustomName() const noexcept { return m_customName; }

    [[nodiscard]] bool HasStatTrak() const noexcept { return m_statTrakCount >= 0; }
    [[nodiscard]] bool HasCustomName() const noexcept { return !m_customName.empty(); }

    [[nodiscard]] sdk::WearRating_t GetWearRating() const noexcept {
        return WearDegradationPipeline::EvaluateWearCondition(m_wear);
    }

    [[nodiscard]] const char* GetWearRatingName() const noexcept {
        return WearDegradationPipeline::GetWearRatingName(GetWearRating());
    }

    [[nodiscard]] float GetRemappedWear(const sdk::CPaintKitDefinition& kit) const noexcept {
        return WearDegradationPipeline::RemapWear(m_wear, kit.m_flWearRemapMin, kit.m_flWearRemapMax);
    }

    void SetWear(float wear) noexcept { m_wear = (wear < 0.0f) ? 0.0f : ((wear > 1.0f) ? 1.0f : wear); }
    void SetSeed(std::int32_t seed) noexcept { m_seed = seed; }
    void SetPaintKit(std::int32_t paintKit) noexcept { m_paintKitIndex = paintKit; }
    void SetStatTrakCount(std::int32_t count) noexcept { m_statTrakCount = count; }
    void SetCustomName(std::string_view name) { m_customName = name; }

    AttributeKeyValueStore& GetAttributes() noexcept { return m_attributes; }
    [[nodiscard]] const AttributeKeyValueStore& GetAttributes() const noexcept { return m_attributes; }

private:
    std::uint64_t m_itemID = 0;
    std::uint16_t m_defIndex = 0;
    sdk::EconQuality_t m_quality = sdk::EconQuality_t::QUALITY_NORMAL;
    sdk::EconRarity_t m_rarity = sdk::EconRarity_t::RARITY_DEFAULT;
    std::int32_t m_paintKitIndex = 0;
    std::int32_t m_seed = 0;
    float m_wear = 0.0f;
    std::int32_t m_statTrakCount = -1;
    std::string m_customName;
    AttributeKeyValueStore m_attributes;
};

// ============================================================================
// 5. Economy Item Schema Catalog
// Lookup catalog for Item Definitions and Paint Kits.
// ============================================================================

class EconItemCatalog {
public:
    EconItemCatalog() = default;
    ~EconItemCatalog() = default;

    static EconItemCatalog& GetInstance() noexcept;

    bool RegisterItemDefinition(const sdk::CEconItemDefinition& def);
    bool RegisterPaintKit(const sdk::CPaintKitDefinition& kit);

    [[nodiscard]] const sdk::CEconItemDefinition* FindItemDefinition(std::uint16_t defIndex) const noexcept;
    [[nodiscard]] const sdk::CEconItemDefinition* FindItemDefinitionByName(std::string_view name) const noexcept;

    [[nodiscard]] const sdk::CPaintKitDefinition* FindPaintKit(std::int32_t paintKitId) const noexcept;
    [[nodiscard]] const sdk::CPaintKitDefinition* FindPaintKitByName(std::string_view name) const noexcept;

    [[nodiscard]] std::size_t GetItemCount() const noexcept { return m_itemsByIndex.size(); }
    [[nodiscard]] std::size_t GetPaintKitCount() const noexcept { return m_paintKitsById.size(); }

    void Clear() noexcept;
    void InitializeDefaultCatalog();

private:
    std::unordered_map<std::uint16_t, sdk::CEconItemDefinition> m_itemsByIndex;
    std::unordered_map<std::string, std::uint16_t> m_itemsByName;

    std::unordered_map<std::int32_t, sdk::CPaintKitDefinition> m_paintKitsById;
    std::unordered_map<std::string, std::int32_t> m_paintKitsByName;
};

} // namespace cs2::client

#endif // CS2_CLIENT_INVENTORY_SCHEMA_H
