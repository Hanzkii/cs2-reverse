#include "inventory_schema.h"

#include <numbers>
#include <algorithm>
#include <cstring>

namespace cs2::client {

// ============================================================================
// 1. Dynamic Attribute Key-Value Store Implementation
// ============================================================================

std::uint32_t AttributeKeyValueStore::HashAttributeName(std::string_view name) noexcept {
    // 32-bit FNV-1a Hash Algorithm
    std::uint32_t hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

void AttributeKeyValueStore::SetAttribute(std::uint16_t defIndex, AttributeValue value) {
    m_indexedAttributes[defIndex] = std::move(value);
}

void AttributeKeyValueStore::SetAttributeByName(std::string_view name, AttributeValue value) {
    const std::uint32_t hash = HashAttributeName(name);
    m_namedAttributes[hash] = std::move(value);
}

const AttributeValue* AttributeKeyValueStore::GetAttribute(std::uint16_t defIndex) const noexcept {
    const auto it = m_indexedAttributes.find(defIndex);
    if (it != m_indexedAttributes.end()) {
        return &it->second;
    }
    return nullptr;
}

float AttributeKeyValueStore::GetAttributeFloat(std::uint16_t defIndex, float defaultValue) const noexcept {
    const auto* attr = GetAttribute(defIndex);
    if (!attr) return defaultValue;

    if (std::holds_alternative<float>(*attr)) {
        return std::get<float>(*attr);
    }
    if (std::holds_alternative<std::int32_t>(*attr)) {
        return static_cast<float>(std::get<std::int32_t>(*attr));
    }
    if (std::holds_alternative<std::uint32_t>(*attr)) {
        return static_cast<float>(std::get<std::uint32_t>(*attr));
    }
    return defaultValue;
}

std::int32_t AttributeKeyValueStore::GetAttributeInt(std::uint16_t defIndex, std::int32_t defaultValue) const noexcept {
    const auto* attr = GetAttribute(defIndex);
    if (!attr) return defaultValue;

    if (std::holds_alternative<std::int32_t>(*attr)) {
        return std::get<std::int32_t>(*attr);
    }
    if (std::holds_alternative<std::uint32_t>(*attr)) {
        return static_cast<std::int32_t>(std::get<std::uint32_t>(*attr));
    }
    if (std::holds_alternative<float>(*attr)) {
        return static_cast<std::int32_t>(std::get<float>(*attr));
    }
    return defaultValue;
}

std::uint32_t AttributeKeyValueStore::GetAttributeUInt(std::uint16_t defIndex, std::uint32_t defaultValue) const noexcept {
    const auto* attr = GetAttribute(defIndex);
    if (!attr) return defaultValue;

    if (std::holds_alternative<std::uint32_t>(*attr)) {
        return std::get<std::uint32_t>(*attr);
    }
    if (std::holds_alternative<std::int32_t>(*attr)) {
        return static_cast<std::uint32_t>(std::get<std::int32_t>(*attr));
    }
    if (std::holds_alternative<float>(*attr)) {
        return static_cast<std::uint32_t>(std::get<float>(*attr));
    }
    return defaultValue;
}

std::string_view AttributeKeyValueStore::GetAttributeString(std::uint16_t defIndex, std::string_view defaultValue) const noexcept {
    const auto* attr = GetAttribute(defIndex);
    if (!attr) return defaultValue;

    if (std::holds_alternative<std::string>(*attr)) {
        return std::get<std::string>(*attr);
    }
    return defaultValue;
}

bool AttributeKeyValueStore::HasAttribute(std::uint16_t defIndex) const noexcept {
    return m_indexedAttributes.find(defIndex) != m_indexedAttributes.end();
}

void AttributeKeyValueStore::RemoveAttribute(std::uint16_t defIndex) noexcept {
    m_indexedAttributes.erase(defIndex);
}

void AttributeKeyValueStore::Clear() noexcept {
    m_indexedAttributes.clear();
    m_namedAttributes.clear();
}

std::size_t AttributeKeyValueStore::Size() const noexcept {
    return m_indexedAttributes.size() + m_namedAttributes.size();
}

bool AttributeKeyValueStore::Empty() const noexcept {
    return m_indexedAttributes.empty() && m_namedAttributes.empty();
}

// ============================================================================
// 2. Procedural Texture Coordinate Mapper Implementation
// ============================================================================

void ProceduralTextureMapper::ComputeSeedFactors(
    std::int32_t seed,
    const sdk::CPaintKitDefinition& kit,
    float& out_offsetX,
    float& out_offsetY,
    float& out_rotationDeg) noexcept
{
    // CS2 32-bit MurmurHash/Wang integer mixing for deterministic procedural seed projection
    std::uint32_t s = static_cast<std::uint32_t>(seed) * 0x9E3779B9u + 0x1234567u;
    s ^= (s >> 16);
    s *= 0x85EBCA6Bu;
    s ^= (s >> 13);
    s *= 0xC2B2AE35u;
    s ^= (s >> 16);

    const float r1 = static_cast<float>(s & 0xFFFFu) / 65535.0f;
    const float r2 = static_cast<float>((s >> 16) & 0xFFFFu) / 65535.0f;

    std::uint32_t s2 = s * 1103515245u + 12345u;
    const float r3 = static_cast<float>((s2 >> 16) & 0xFFFFu) / 65535.0f;

    out_offsetX = kit.m_flPatternOffsetXStart + r1 * (kit.m_flPatternOffsetXEnd - kit.m_flPatternOffsetXStart);
    out_offsetY = kit.m_flPatternOffsetYStart + r2 * (kit.m_flPatternOffsetYEnd - kit.m_flPatternOffsetYStart);
    out_rotationDeg = kit.m_flPatternRotateStart + r3 * (kit.m_flPatternRotateEnd - kit.m_flPatternRotateStart);
}

ProceduralUVResult ProceduralTextureMapper::TransformUV(
    std::int32_t seed,
    const sdk::CPaintKitDefinition& kit,
    float baseU,
    float baseV) noexcept
{
    ProceduralUVResult result{};
    result.scale = kit.m_flPatternScale > 0.0001f ? kit.m_flPatternScale : 1.0f;

    ComputeSeedFactors(seed, kit, result.offset_x, result.offset_y, result.rotation_deg);

    // Apply scale and translation offset
    const float u_scaled = (baseU * result.scale) + result.offset_x;
    const float v_scaled = (baseV * result.scale) + result.offset_y;

    // Apply rotation matrix: R(theta) * [u, v]^T
    const float rad = result.rotation_deg * (std::numbers::pi_v<float> / 180.0f);
    const float cosTheta = std::cos(rad);
    const float sinTheta = std::sin(rad);

    const float u_rot = (cosTheta * u_scaled) - (sinTheta * v_scaled);
    const float v_rot = (sinTheta * u_scaled) + (cosTheta * v_scaled);

    // Modulo wrap to [0.0, 1.0] unit texture space
    result.u_transformed = u_rot - std::floor(u_rot);
    result.v_transformed = v_rot - std::floor(v_rot);

    return result;
}

void ProceduralTextureMapper::ComputeUVMatrix(
    std::int32_t seed,
    const sdk::CPaintKitDefinition& kit,
    std::array<float, 4>& out_rotationMatrix,
    std::array<float, 2>& out_translation) noexcept
{
    float offX = 0.0f;
    float offY = 0.0f;
    float rotDeg = 0.0f;
    ComputeSeedFactors(seed, kit, offX, offY, rotDeg);

    const float scale = kit.m_flPatternScale > 0.0001f ? kit.m_flPatternScale : 1.0f;
    const float rad = rotDeg * (std::numbers::pi_v<float> / 180.0f);
    const float cosTheta = std::cos(rad);
    const float sinTheta = std::sin(rad);

    out_rotationMatrix[0] = cosTheta * scale; // [0,0]
    out_rotationMatrix[1] = -sinTheta * scale;// [0,1]
    out_rotationMatrix[2] = sinTheta * scale; // [1,0]
    out_rotationMatrix[3] = cosTheta * scale;  // [1,1]

    out_translation[0] = (cosTheta * offX) - (sinTheta * offY);
    out_translation[1] = (sinTheta * offX) + (cosTheta * offY);
}

// ============================================================================
// 3. Wear Degradation & Surface Abrasion Pipeline Implementation
// ============================================================================

float WearDegradationPipeline::SimulateWearDegradationODE(
    float initialWear,
    float abrasionRate,
    float durationSeconds,
    float timeStep) noexcept
{
    float w = std::clamp(initialWear, 0.0f, 1.0f);
    if (durationSeconds <= 0.0f || abrasionRate <= 0.0f) {
        return w;
    }

    // Runge-Kutta 4th Order (RK4) ODE integration of dw/dt = kappa * (1.0 - w)
    const auto f = [abrasionRate](float current_w) noexcept -> float {
        return abrasionRate * (1.0f - current_w);
    };

    float t = 0.0f;
    const float dt = std::clamp(timeStep, 0.0001f, 0.1f);

    while (t < durationSeconds) {
        const float step = (t + dt > durationSeconds) ? (durationSeconds - t) : dt;

        const float k1 = f(w);
        const float k2 = f(w + 0.5f * step * k1);
        const float k3 = f(w + 0.5f * step * k2);
        const float k4 = f(w + step * k3);

        w += (step / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
        w = std::clamp(w, 0.0f, 1.0f);

        t += step;
        if (w >= 0.9999f) break;
    }

    return w;
}

float WearDegradationPipeline::EvaluateAbrasionAlpha(
    float remappedWear,
    float maskTextureSample,
    float edgeSmoothness) noexcept
{
    // Smoothstep edge abrasion formula:
    // When mask value < remappedWear, surface paint is scraped off exposing raw steel/patina.
    const float edge = std::max(edgeSmoothness, 0.001f);
    const float edge0 = remappedWear - edge;
    const float edge1 = remappedWear + edge;

    const float t = std::clamp((maskTextureSample - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// 4. Instantiated Client Inventory Item Implementation
// ============================================================================

InventoryItem::InventoryItem(
    std::uint64_t itemID,
    std::uint16_t defIndex,
    sdk::EconQuality_t quality,
    sdk::EconRarity_t rarity,
    std::int32_t paintKitIndex,
    std::int32_t seed,
    float wear,
    std::int32_t statTrakCount,
    std::string_view customName)
    : m_itemID(itemID),
      m_defIndex(defIndex),
      m_quality(quality),
      m_rarity(rarity),
      m_paintKitIndex(paintKitIndex),
      m_seed(seed),
      m_wear(std::clamp(wear, 0.0f, 1.0f)),
      m_statTrakCount(statTrakCount),
      m_customName(customName)
{
}

// ============================================================================
// 5. Economy Item Schema Catalog Implementation
// ============================================================================

EconItemCatalog& EconItemCatalog::GetInstance() noexcept {
    static EconItemCatalog instance;
    return instance;
}

bool EconItemCatalog::RegisterItemDefinition(const sdk::CEconItemDefinition& def) {
    if (def.m_nDefIndex == 0) return false;

    m_itemsByIndex[def.m_nDefIndex] = def;
    if (def.m_pszItemBaseName && def.m_pszItemBaseName[0] != '\0') {
        m_itemsByName[def.m_pszItemBaseName] = def.m_nDefIndex;
    }
    return true;
}

bool EconItemCatalog::RegisterPaintKit(const sdk::CPaintKitDefinition& kit) {
    if (kit.m_nID < 0) return false;

    m_paintKitsById[kit.m_nID] = kit;
    if (kit.m_pszName && kit.m_pszName[0] != '\0') {
        m_paintKitsByName[kit.m_pszName] = kit.m_nID;
    }
    return true;
}

const sdk::CEconItemDefinition* EconItemCatalog::FindItemDefinition(std::uint16_t defIndex) const noexcept {
    const auto it = m_itemsByIndex.find(defIndex);
    if (it != m_itemsByIndex.end()) {
        return &it->second;
    }
    return nullptr;
}

const sdk::CEconItemDefinition* EconItemCatalog::FindItemDefinitionByName(std::string_view name) const noexcept {
    const auto it = m_itemsByName.find(std::string(name));
    if (it != m_itemsByName.end()) {
        return FindItemDefinition(it->second);
    }
    return nullptr;
}

const sdk::CPaintKitDefinition* EconItemCatalog::FindPaintKit(std::int32_t paintKitId) const noexcept {
    const auto it = m_paintKitsById.find(paintKitId);
    if (it != m_paintKitsById.end()) {
        return &it->second;
    }
    return nullptr;
}

const sdk::CPaintKitDefinition* EconItemCatalog::FindPaintKitByName(std::string_view name) const noexcept {
    const auto it = m_paintKitsByName.find(std::string(name));
    if (it != m_paintKitsByName.end()) {
        return FindPaintKit(it->second);
    }
    return nullptr;
}

void EconItemCatalog::Clear() noexcept {
    m_itemsByIndex.clear();
    m_itemsByName.clear();
    m_paintKitsById.clear();
    m_paintKitsByName.clear();
}

void EconItemCatalog::InitializeDefaultCatalog() {
    Clear();

    // ------------------------------------------------------------------------
    // Canonical Weapon Definitions (items_game.txt)
    // ------------------------------------------------------------------------

    // Desert Eagle
    RegisterItemDefinition({
        .m_nDefIndex = 1,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_SECONDARY0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_SECONDARY,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_LEGENDARY,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_deagle",
        .m_pszItemTypeName = "#CSGO_Type_Pistol",
        .m_pszItemDescription = "#CSGO_Item_Desc_Deagle",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_deagle",
        .m_pszModelPlayer = "weapons/models/pistol/desert_eagle/weapon_desert_eagle.vmdl",
        .m_pszModelWorld = "weapons/models/pistol/desert_eagle/weapon_desert_eagle_dropped.vmdl"
    });

    // Glock-18
    RegisterItemDefinition({
        .m_nDefIndex = 4,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_SECONDARY0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_SECONDARY,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_COMMON,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_glock",
        .m_pszItemTypeName = "#CSGO_Type_Pistol",
        .m_pszItemDescription = "#CSGO_Item_Desc_Glock",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_glock",
        .m_pszModelPlayer = "weapons/models/pistol/glock/weapon_glock.vmdl",
        .m_pszModelWorld = "weapons/models/pistol/glock/weapon_glock_dropped.vmdl"
    });

    // AK-47
    RegisterItemDefinition({
        .m_nDefIndex = 7,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_RIFLE0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_RIFLE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_ak47",
        .m_pszItemTypeName = "#CSGO_Type_Rifle",
        .m_pszItemDescription = "#CSGO_Item_Desc_AK47",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_ak47",
        .m_pszModelPlayer = "weapons/models/rifles/ak47/weapon_ak47.vmdl",
        .m_pszModelWorld = "weapons/models/rifles/ak47/weapon_ak47_dropped.vmdl"
    });

    // AWP
    RegisterItemDefinition({
        .m_nDefIndex = 9,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_RIFLE3,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_RIFLE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_awp",
        .m_pszItemTypeName = "#CSGO_Type_SniperRifle",
        .m_pszItemDescription = "#CSGO_Item_Desc_AWP",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_awp",
        .m_pszModelPlayer = "weapons/models/rifles/awp/weapon_awp.vmdl",
        .m_pszModelWorld = "weapons/models/rifles/awp/weapon_awp_dropped.vmdl"
    });

    // M4A4
    RegisterItemDefinition({
        .m_nDefIndex = 16,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_RIFLE0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_RIFLE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_m4a1",
        .m_pszItemTypeName = "#CSGO_Type_Rifle",
        .m_pszItemDescription = "#CSGO_Item_Desc_M4A4",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_m4a1",
        .m_pszModelPlayer = "weapons/models/rifles/m4a4/weapon_m4a4.vmdl",
        .m_pszModelWorld = "weapons/models/rifles/m4a4/weapon_m4a4_dropped.vmdl"
    });

    // SSG 08
    RegisterItemDefinition({
        .m_nDefIndex = 40,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_RIFLE2,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_RIFLE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_RARE,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_ssg08",
        .m_pszItemTypeName = "#CSGO_Type_SniperRifle",
        .m_pszItemDescription = "#CSGO_Item_Desc_SSG08",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_ssg08",
        .m_pszModelPlayer = "weapons/models/rifles/ssg08/weapon_ssg08.vmdl",
        .m_pszModelWorld = "weapons/models/rifles/ssg08/weapon_ssg08_dropped.vmdl"
    });

    // M4A1-S
    RegisterItemDefinition({
        .m_nDefIndex = 60,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_RIFLE0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_RIFLE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_m4a1_silencer",
        .m_pszItemTypeName = "#CSGO_Type_Rifle",
        .m_pszItemDescription = "#CSGO_Item_Desc_M4A1_Silencer",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_m4a1_silencer",
        .m_pszModelPlayer = "weapons/models/rifles/m4a1_silencer/weapon_m4a1_silencer.vmdl",
        .m_pszModelWorld = "weapons/models/rifles/m4a1_silencer/weapon_m4a1_silencer_dropped.vmdl"
    });

    // USP-S
    RegisterItemDefinition({
        .m_nDefIndex = 61,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_SECONDARY0,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_SECONDARY,
        .m_nQuality = sdk::EconQuality_t::QUALITY_NORMAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_RARE,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_usp_silencer",
        .m_pszItemTypeName = "#CSGO_Type_Pistol",
        .m_pszItemDescription = "#CSGO_Item_Desc_USP_Silencer",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_usp_silencer",
        .m_pszModelPlayer = "weapons/models/pistol/usp_silencer/weapon_usp_silencer.vmdl",
        .m_pszModelWorld = "weapons/models/pistol/usp_silencer/weapon_usp_silencer_dropped.vmdl"
    });

    // Karambit
    RegisterItemDefinition({
        .m_nDefIndex = 507,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_MELEE,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_MELEE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_UNUSUAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_knife_karambit",
        .m_pszItemTypeName = "#CSGO_Type_Knife",
        .m_pszItemDescription = "#CSGO_Item_Desc_Knife_Karambit",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_knife_karambit",
        .m_pszModelPlayer = "weapons/models/knife/karambit/weapon_knife_karambit.vmdl",
        .m_pszModelWorld = "weapons/models/knife/karambit/weapon_knife_karambit_dropped.vmdl"
    });

    // M9 Bayonet
    RegisterItemDefinition({
        .m_nDefIndex = 508,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_MELEE,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_MELEE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_UNUSUAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_knife_m9_bayonet",
        .m_pszItemTypeName = "#CSGO_Type_Knife",
        .m_pszItemDescription = "#CSGO_Item_Desc_Knife_M9_Bayonet",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_knife_m9_bayonet",
        .m_pszModelPlayer = "weapons/models/knife/m9_bayonet/weapon_knife_m9_bayonet.vmdl",
        .m_pszModelWorld = "weapons/models/knife/m9_bayonet/weapon_knife_m9_bayonet_dropped.vmdl"
    });

    // Butterfly Knife
    RegisterItemDefinition({
        .m_nDefIndex = 515,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_MELEE,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_MELEE,
        .m_nQuality = sdk::EconQuality_t::QUALITY_UNUSUAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_knife_butterfly",
        .m_pszItemTypeName = "#CSGO_Type_Knife",
        .m_pszItemDescription = "#CSGO_Item_Desc_Knife_Butterfly",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_knife_butterfly",
        .m_pszModelPlayer = "weapons/models/knife/butterfly/weapon_knife_butterfly.vmdl",
        .m_pszModelWorld = "weapons/models/knife/butterfly/weapon_knife_butterfly_dropped.vmdl"
    });

    // Sport Gloves
    RegisterItemDefinition({
        .m_nDefIndex = 5030,
        .m_nLoadoutSlot = sdk::LoadoutSlot_t::LOADOUT_SLOT_GLOVES,
        .m_nEquipmentCategory = sdk::EquipmentCategory_t::CATEGORY_GLOVES,
        .m_nQuality = sdk::EconQuality_t::QUALITY_UNUSUAL,
        .m_nRarity = sdk::EconRarity_t::RARITY_ANCIENT,
        .m_nDefaultDropQuantity = 1,
        .m_nItemLevelMin = 1,
        .m_nItemLevelMax = 1,
        .m_pszItemBaseName = "weapon_gloves_sport",
        .m_pszItemTypeName = "#CSGO_Type_Gloves",
        .m_pszItemDescription = "#CSGO_Item_Desc_Gloves_Sport",
        .m_pszInventoryImage = "econ/weapons/base_weapons/weapon_gloves_sport",
        .m_pszModelPlayer = "weapons/models/gloves/sport/glove_sport.vmdl",
        .m_pszModelWorld = "weapons/models/gloves/sport/glove_sport_dropped.vmdl"
    });

    // ------------------------------------------------------------------------
    // Canonical Paint Kit Definitions (items_paintkits.txt)
    // ------------------------------------------------------------------------

    // Fade (ID: 38)
    RegisterPaintKit({
        .m_nID = 38,
        .m_pszName = "aa_fade",
        .m_pszDescription = "#PaintKit_aa_fade",
        .m_pszTemplate = "workshop/paintkits/templates/anodized_airbrushed.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_ANODIZED_AIRBRUSHED,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 0.08f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 1.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 1.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 360.00f,
        .m_flPearlescent = 0.00f
    });

    // Case Hardened (ID: 44)
    RegisterPaintKit({
        .m_nID = 44,
        .m_pszName = "hy_case_hardened",
        .m_pszDescription = "#PaintKit_hy_case_hardened",
        .m_pszTemplate = "workshop/paintkits/templates/hydrographic.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_HYDROGRAPHIC,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 1.00f,
        .m_flPatternScale = 4.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 1.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 1.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 360.00f,
        .m_flPearlescent = 0.00f
    });

    // Fire Serpent (ID: 180)
    RegisterPaintKit({
        .m_nID = 180,
        .m_pszName = "cu_fire_serpent",
        .m_pszDescription = "#PaintKit_cu_fire_serpent",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.06f,
        .m_flWearRemapMax = 0.76f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Asiimov (ID: 255)
    RegisterPaintKit({
        .m_nID = 255,
        .m_pszName = "cu_asiimov",
        .m_pszDescription = "#PaintKit_cu_asiimov",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.18f,
        .m_flWearRemapMax = 1.00f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Redline (ID: 282)
    RegisterPaintKit({
        .m_nID = 282,
        .m_pszName = "cu_redline",
        .m_pszDescription = "#PaintKit_cu_redline",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.10f,
        .m_flWearRemapMax = 0.70f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Howl (ID: 309)
    RegisterPaintKit({
        .m_nID = 309,
        .m_pszName = "cu_howl",
        .m_pszDescription = "#PaintKit_cu_howl",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 0.40f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Dragon Lore (ID: 344)
    RegisterPaintKit({
        .m_nID = 344,
        .m_pszName = "cu_dragon_lore",
        .m_pszDescription = "#PaintKit_cu_dragon_lore",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 0.70f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Doppler (ID: 415)
    RegisterPaintKit({
        .m_nID = 415,
        .m_pszName = "am_doppler",
        .m_pszDescription = "#PaintKit_am_doppler",
        .m_pszTemplate = "workshop/paintkits/templates/anodized_airbrushed.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_ANODIZED_AIRBRUSHED,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 0.08f,
        .m_flPatternScale = 4.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 1.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 1.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 360.00f,
        .m_flPearlescent = 0.00f
    });

    // Hyper Beast (ID: 430)
    RegisterPaintKit({
        .m_nID = 430,
        .m_pszName = "cu_hyper_beast",
        .m_pszDescription = "#PaintKit_cu_hyper_beast",
        .m_pszTemplate = "workshop/paintkits/templates/custom_paint_job.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 1.00f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 0.00f
    });

    // Printstream (ID: 984)
    RegisterPaintKit({
        .m_nID = 984,
        .m_pszName = "gs_printstream",
        .m_pszDescription = "#PaintKit_gs_printstream",
        .m_pszTemplate = "workshop/paintkits/templates/gunsmith.vcompmat",
        .m_nStyle = sdk::PaintKitStyle_t::PAINT_KIT_STYLE_GUNSMITH,
        .m_flWearRemapMin = 0.00f,
        .m_flWearRemapMax = 0.80f,
        .m_flPatternScale = 1.00f,
        .m_flPatternOffsetXStart = 0.00f,
        .m_flPatternOffsetXEnd = 0.00f,
        .m_flPatternOffsetYStart = 0.00f,
        .m_flPatternOffsetYEnd = 0.00f,
        .m_flPatternRotateStart = 0.00f,
        .m_flPatternRotateEnd = 0.00f,
        .m_flPearlescent = 15.00f
    });
}

} // namespace cs2::client
