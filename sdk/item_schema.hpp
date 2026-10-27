#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <string_view>
#include <array>
#include <cmath>

#include "sdk/client_classes.hpp"

#ifndef PAD
#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:
#endif

// ============================================================================
// Source 2 Reverse Engineered SDK - Phase 5: Economy & Item Schema Pipeline
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Binary: client.dll (Base: 0x180000000)
// Verified via Ghidra Static Decompilation & Schema Reflection
// ============================================================================

namespace cs2::sdk {

// ============================================================================
// Economy Item Enumerations & Type Classifications
// ============================================================================

enum class EconQuality_t : std::int32_t {
    QUALITY_NORMAL = 0,
    QUALITY_GENUINE = 1,
    QUALITY_VINTAGE = 2,
    QUALITY_UNUSUAL = 3,
    QUALITY_UNIQUE = 4,
    QUALITY_COMMUNITY = 5,
    QUALITY_DEVELOPER = 6,
    QUALITY_SELFMADE = 7,
    QUALITY_CUSTOMIZED = 8,
    QUALITY_STRANGE = 9,
    QUALITY_COMPLETED = 10,
    QUALITY_HAUNTED = 11,
    QUALITY_TOURNAMENT = 12
};

enum class EconRarity_t : std::int32_t {
    RARITY_DEFAULT = 0,
    RARITY_COMMON = 1,       // Consumer Grade (White / Gray)
    RARITY_UNCOMMON = 2,     // Industrial Grade (Light Blue)
    RARITY_RARE = 3,         // Mil-Spec Grade (Blue)
    RARITY_MYTHICAL = 4,     // Restricted (Purple)
    RARITY_LEGENDARY = 5,    // Classified (Pink / Magenta)
    RARITY_ANCIENT = 6,      // Covert (Red)
    RARITY_IMMORTAL = 7      // Contraband (Gold / Yellow)
};

enum class LoadoutSlot_t : std::int32_t {
    LOADOUT_SLOT_MELEE = 0,
    LOADOUT_SLOT_SECONDARY0 = 1,
    LOADOUT_SLOT_SECONDARY1 = 2,
    LOADOUT_SLOT_SECONDARY2 = 3,
    LOADOUT_SLOT_SECONDARY3 = 4,
    LOADOUT_SLOT_SECONDARY4 = 5,
    LOADOUT_SLOT_SMG0 = 6,
    LOADOUT_SLOT_SMG1 = 7,
    LOADOUT_SLOT_SMG2 = 8,
    LOADOUT_SLOT_SMG3 = 9,
    LOADOUT_SLOT_SMG4 = 10,
    LOADOUT_SLOT_RIFLE0 = 11,
    LOADOUT_SLOT_RIFLE1 = 12,
    LOADOUT_SLOT_RIFLE2 = 13,
    LOADOUT_SLOT_RIFLE3 = 14,
    LOADOUT_SLOT_RIFLE4 = 15,
    LOADOUT_SLOT_HEAVY0 = 16,
    LOADOUT_SLOT_HEAVY1 = 17,
    LOADOUT_SLOT_HEAVY2 = 18,
    LOADOUT_SLOT_HEAVY3 = 19,
    LOADOUT_SLOT_HEAVY4 = 20,
    LOADOUT_SLOT_GLOVES = 41,
    LOADOUT_SLOT_CLOTHING_CUSTOMPLAYER = 42
};

enum class EquipmentCategory_t : std::int32_t {
    CATEGORY_OTHER = 0,
    CATEGORY_MELEE = 1,
    CATEGORY_SECONDARY = 2,
    CATEGORY_SMG = 3,
    CATEGORY_RIFLE = 4,
    CATEGORY_HEAVY = 5,
    CATEGORY_EQUIPMENT = 6,
    CATEGORY_GRENADE = 7,
    CATEGORY_GLOVES = 8,
    CATEGORY_AGENT = 9
};

enum class PaintKitStyle_t : std::int32_t {
    PAINT_KIT_STYLE_SOLID_COLOR = 0,
    PAINT_KIT_STYLE_HYDROGRAPHIC = 1,
    PAINT_KIT_STYLE_SPRAY_PAINT = 2,
    PAINT_KIT_STYLE_ANODIZED = 3,
    PAINT_KIT_STYLE_ANODIZED_MULTICOLOR = 4,
    PAINT_KIT_STYLE_ANODIZED_AIRBRUSHED = 5,
    PAINT_KIT_STYLE_CUSTOM_PAINT_JOB = 6,
    PAINT_KIT_STYLE_PATINA = 7,
    PAINT_KIT_STYLE_GUNSMITH = 8
};

enum class WearRating_t : std::int32_t {
    WEAR_FACTORY_NEW = 0,    // [0.00, 0.07)
    WEAR_MINIMAL_WEAR = 1,   // [0.07, 0.15)
    WEAR_FIELD_TESTED = 2,   // [0.15, 0.38)
    WEAR_WELL_WORN = 3,      // [0.38, 0.45)
    WEAR_BATTLE_SCARRED = 4  // [0.45, 1.00]
};

// ============================================================================
// 1. Dynamic Attribute Primitive (CEconItemAttribute)
// Binary: server.dll / client.dll (Size: 0x48 / 72 bytes)
// ============================================================================
class CEconItemAttribute {
public:
    virtual ~CEconItemAttribute() = default;             // +0x00
    PAD(0x28, m_pad08);                                  // +0x08..+0x30
    std::uint16_t m_iAttributeDefinitionIndex;           // +0x30
    PAD(0x02, m_pad32);                                  // +0x32..+0x34
    float m_flValue;                                     // +0x34
    float m_flInitialValue;                              // +0x38
    std::int32_t m_nRefundableCurrency;                  // +0x3C
    bool m_bSetBonus;                                    // +0x40
    PAD(0x07, m_pad41);                                  // +0x41..+0x48
};
static_assert(sizeof(CEconItemAttribute) == 0x48, "CEconItemAttribute size must be 0x48");
static_assert(offsetof(CEconItemAttribute, m_iAttributeDefinitionIndex) == 0x30, "m_iAttributeDefinitionIndex offset mismatch");
static_assert(offsetof(CEconItemAttribute, m_flValue) == 0x34, "m_flValue offset mismatch");
static_assert(offsetof(CEconItemAttribute, m_flInitialValue) == 0x38, "m_flInitialValue offset mismatch");
static_assert(offsetof(CEconItemAttribute, m_nRefundableCurrency) == 0x3C, "m_nRefundableCurrency offset mismatch");
static_assert(offsetof(CEconItemAttribute, m_bSetBonus) == 0x40, "m_bSetBonus offset mismatch");

// ============================================================================
// 2. Attribute Manager & Providers (CAttributeManager)
// Binary: server.dll / client.dll (Size: 0x50 / 80 bytes)
// ============================================================================
class CAttributeManager {
public:
    virtual ~CAttributeManager() = default;              // +0x00
    void* m_Providers;                                   // +0x08: CUtlVector<CHandle<C_BaseEntity>>
    PAD(0x10, m_pad10);                                  // +0x10..+0x20
    std::int32_t m_iReapplyProvisionParity;              // +0x20
    CEntityHandle m_hOuter;                              // +0x24: Owning entity handle
    bool m_bPreventLoopback;                             // +0x28
    PAD(0x03, m_pad29);                                  // +0x29..+0x2C
    std::int32_t m_ProviderType;                         // +0x2C
    void* m_CachedResults;                               // +0x30: CUtlVector<cached_attribute_float_t>
    PAD(0x18, m_pad38);                                  // +0x38..+0x50
};
static_assert(sizeof(CAttributeManager) == 0x50, "CAttributeManager size must be 0x50");
static_assert(offsetof(CAttributeManager, m_Providers) == 0x08, "m_Providers offset mismatch");
static_assert(offsetof(CAttributeManager, m_iReapplyProvisionParity) == 0x20, "m_iReapplyProvisionParity offset mismatch");
static_assert(offsetof(CAttributeManager, m_hOuter) == 0x24, "m_hOuter offset mismatch");
static_assert(offsetof(CAttributeManager, m_bPreventLoopback) == 0x28, "m_bPreventLoopback offset mismatch");
static_assert(offsetof(CAttributeManager, m_ProviderType) == 0x2C, "m_ProviderType offset mismatch");
static_assert(offsetof(CAttributeManager, m_CachedResults) == 0x30, "m_CachedResults offset mismatch");

// ============================================================================
// 3. Dynamic Attribute Collection Container (CAttributeList)
// Binary: server.dll / client.dll (Size: 0x78 / 120 bytes)
// ============================================================================
class CAttributeList {
public:
    virtual ~CAttributeList() = default;                 // +0x00
    PAD(0x68, m_pad08);                                  // +0x08..+0x70
    CAttributeManager* m_pManager;                       // +0x70
};
static_assert(sizeof(CAttributeList) == 0x78, "CAttributeList size must be 0x78");
static_assert(offsetof(CAttributeList, m_pManager) == 0x70, "m_pManager offset mismatch");

// ============================================================================
// 4. Econ Item Interface Base (IEconItemInterface)
// Abstract base interface with virtual method table.
// ============================================================================
class IEconItemInterface {
public:
    virtual ~IEconItemInterface() = default;             // +0x00
};

// ============================================================================
// 5. Client Econ Item View (C_EconItemView)
// Binary: client.dll (Size: 0x5B0 / 1456 bytes)
// Reconstructed via Schema Reflection & Verified Offsets
// ============================================================================
class C_EconItemView : public IEconItemInterface {
public:
    PAD(0x58, m_pad08);                                  // +0x08..+0x60
    bool m_bInventoryImageRgbaRequested;                 // +0x60
    bool m_bInventoryImageTriedCache;                    // +0x61
    PAD(0x1E, m_pad62);                                  // +0x62..+0x80
    std::int32_t m_nInventoryImageRgbaWidth;             // +0x80
    std::int32_t m_nInventoryImageRgbaHeight;            // +0x84
    char m_szCurrentLoadCachedFileName[304];             // +0x88..+0x1B8
    bool m_bRestoreCustomMaterialAfterPrecache;          // +0x1B8
    PAD(0x01, m_pad1B9);                                 // +0x1B9..+0x1BA
    std::uint16_t m_iItemDefinitionIndex;                // +0x1BA
    std::int32_t m_iEntityQuality;                       // +0x1BC
    std::uint32_t m_iEntityLevel;                        // +0x1C0
    PAD(0x04, m_pad1C4);                                 // +0x1C4..+0x1C8
    std::uint64_t m_iItemID;                             // +0x1C8
    std::uint32_t m_iItemIDHigh;                         // +0x1D0
    std::uint32_t m_iItemIDLow;                          // +0x1D4
    std::uint32_t m_iAccountID;                          // +0x1D8
    std::uint32_t m_iInventoryPosition;                  // +0x1DC
    PAD(0x08, m_pad1E0);                                 // +0x1E0..+0x1E8
    bool m_bInitialized;                                 // +0x1E8
    bool m_bDisallowSOC;                                 // +0x1E9
    bool m_bIsStoreItem;                                 // +0x1EA
    bool m_bIsTradeItem;                                 // +0x1EB
    std::int32_t m_iEntityQuantity;                      // +0x1EC
    std::int32_t m_iRarityOverride;                      // +0x1F0
    std::int32_t m_iQualityOverride;                     // +0x1F4
    std::int32_t m_iOriginOverride;                      // +0x1F8
    std::uint8_t m_ubStyleOverride;                      // +0x1FC
    std::uint8_t m_unClientFlags;                        // +0x1FD
    PAD(0x0A, m_pad1FE);                                 // +0x1FE..+0x208
    CAttributeList m_AttributeList;                      // +0x208 (Size 0x78)
    CAttributeList m_NetworkedDynamicAttributes;          // +0x280 (Size 0x78)
    char m_szCustomName[161];                            // +0x2F8
    char m_szCustomNameOverride[161];                    // +0x399
    char m_szCustomNameOverride2[161];                   // +0x43A
    char m_szCustomNameOverride3[161];                   // +0x4DB
    PAD(0x2C, m_pad57C);                                 // +0x57C..+0x5A8
    bool m_bInitializedTags;                             // +0x5A8
    PAD(0x07, m_pad5A9);                                 // +0x5A9..+0x5B0

    [[nodiscard]] constexpr std::uint16_t GetItemDefinitionIndex() const noexcept {
        return m_iItemDefinitionIndex;
    }

    [[nodiscard]] constexpr std::uint64_t GetItemID() const noexcept {
        return m_iItemID;
    }

    [[nodiscard]] constexpr bool IsInitialized() const noexcept {
        return m_bInitialized;
    }

    [[nodiscard]] const char* GetCustomName() const noexcept {
        return m_szCustomName[0] != '\0' ? m_szCustomName : nullptr;
    }
};
static_assert(sizeof(C_EconItemView) == 0x5B0, "C_EconItemView size must be 0x5B0");
static_assert(offsetof(C_EconItemView, m_bInventoryImageRgbaRequested) == 0x60, "m_bInventoryImageRgbaRequested offset mismatch");
static_assert(offsetof(C_EconItemView, m_bInventoryImageTriedCache) == 0x61, "m_bInventoryImageTriedCache offset mismatch");
static_assert(offsetof(C_EconItemView, m_nInventoryImageRgbaWidth) == 0x80, "m_nInventoryImageRgbaWidth offset mismatch");
static_assert(offsetof(C_EconItemView, m_nInventoryImageRgbaHeight) == 0x84, "m_nInventoryImageRgbaHeight offset mismatch");
static_assert(offsetof(C_EconItemView, m_szCurrentLoadCachedFileName) == 0x88, "m_szCurrentLoadCachedFileName offset mismatch");
static_assert(offsetof(C_EconItemView, m_bRestoreCustomMaterialAfterPrecache) == 0x1B8, "m_bRestoreCustomMaterialAfterPrecache offset mismatch");
static_assert(offsetof(C_EconItemView, m_iItemDefinitionIndex) == 0x1BA, "m_iItemDefinitionIndex offset mismatch");
static_assert(offsetof(C_EconItemView, m_iEntityQuality) == 0x1BC, "m_iEntityQuality offset mismatch");
static_assert(offsetof(C_EconItemView, m_iEntityLevel) == 0x1C0, "m_iEntityLevel offset mismatch");
static_assert(offsetof(C_EconItemView, m_iItemID) == 0x1C8, "m_iItemID offset mismatch");
static_assert(offsetof(C_EconItemView, m_iItemIDHigh) == 0x1D0, "m_iItemIDHigh offset mismatch");
static_assert(offsetof(C_EconItemView, m_iItemIDLow) == 0x1D4, "m_iItemIDLow offset mismatch");
static_assert(offsetof(C_EconItemView, m_iAccountID) == 0x1D8, "m_iAccountID offset mismatch");
static_assert(offsetof(C_EconItemView, m_iInventoryPosition) == 0x1DC, "m_iInventoryPosition offset mismatch");
static_assert(offsetof(C_EconItemView, m_bInitialized) == 0x1E8, "m_bInitialized offset mismatch");
static_assert(offsetof(C_EconItemView, m_bDisallowSOC) == 0x1E9, "m_bDisallowSOC offset mismatch");
static_assert(offsetof(C_EconItemView, m_bIsStoreItem) == 0x1EA, "m_bIsStoreItem offset mismatch");
static_assert(offsetof(C_EconItemView, m_bIsTradeItem) == 0x1EB, "m_bIsTradeItem offset mismatch");
static_assert(offsetof(C_EconItemView, m_iEntityQuantity) == 0x1EC, "m_iEntityQuantity offset mismatch");
static_assert(offsetof(C_EconItemView, m_iRarityOverride) == 0x1F0, "m_iRarityOverride offset mismatch");
static_assert(offsetof(C_EconItemView, m_iQualityOverride) == 0x1F4, "m_iQualityOverride offset mismatch");
static_assert(offsetof(C_EconItemView, m_iOriginOverride) == 0x1F8, "m_iOriginOverride offset mismatch");
static_assert(offsetof(C_EconItemView, m_ubStyleOverride) == 0x1FC, "m_ubStyleOverride offset mismatch");
static_assert(offsetof(C_EconItemView, m_unClientFlags) == 0x1FD, "m_unClientFlags offset mismatch");
static_assert(offsetof(C_EconItemView, m_AttributeList) == 0x208, "m_AttributeList offset mismatch");
static_assert(offsetof(C_EconItemView, m_NetworkedDynamicAttributes) == 0x280, "m_NetworkedDynamicAttributes offset mismatch");
static_assert(offsetof(C_EconItemView, m_szCustomName) == 0x2F8, "m_szCustomName offset mismatch");
static_assert(offsetof(C_EconItemView, m_szCustomNameOverride) == 0x399, "m_szCustomNameOverride offset mismatch");
static_assert(offsetof(C_EconItemView, m_szCustomNameOverride2) == 0x43A, "m_szCustomNameOverride2 offset mismatch");
static_assert(offsetof(C_EconItemView, m_szCustomNameOverride3) == 0x4DB, "m_szCustomNameOverride3 offset mismatch");
static_assert(offsetof(C_EconItemView, m_bInitializedTags) == 0x5A8, "m_bInitializedTags offset mismatch");

// ============================================================================
// 6. Attribute Container (C_AttributeContainer)
// Binary: client.dll (Size: 0x610 / 1552 bytes)
// ============================================================================
class C_AttributeContainer : public CAttributeManager {
public:
    C_EconItemView m_Item;                               // +0x050 (Size: 0x5B0, ends at +0x600)
    std::int32_t m_iExternalItemProviderRegisteredToken; // +0x600
    PAD(0x04, m_pad604);                                 // +0x604..+0x608
    std::uint64_t m_ullRegisteredAsItemID;               // +0x608
};
static_assert(sizeof(C_AttributeContainer) == 0x610, "C_AttributeContainer size must be 0x610");
static_assert(offsetof(C_AttributeContainer, m_Item) == 0x50, "m_Item offset mismatch");
static_assert(offsetof(C_AttributeContainer, m_iExternalItemProviderRegisteredToken) == 0x600, "m_iExternalItemProviderRegisteredToken offset mismatch");
static_assert(offsetof(C_AttributeContainer, m_ullRegisteredAsItemID) == 0x608, "m_ullRegisteredAsItemID offset mismatch");

// ============================================================================
// 7. Base Econ Entity (C_EconEntity)
// Binary: client.dll (Size: 0x1918 / 6424 bytes)
// Direct Base: CBaseAnimGraph (Size: 0x1268 / 4712 bytes)
// ============================================================================
class C_EconEntity : public CBaseAnimGraph {
public:
    PAD(0x10, m_pad1268);                                // +0x1268..+0x1278
    float m_flFlexDelayTime;                             // +0x1278
    PAD(0x04, m_pad127C);                                // +0x127C..+0x1280
    float m_flFlexDelayedWeight;                         // +0x1280
    PAD(0x04, m_pad1284);                                // +0x1284..+0x1288
    bool m_bAttributesInitialized;                       // +0x1288
    PAD(0x07, m_pad1289);                                // +0x1289..+0x1290
    C_AttributeContainer m_AttributeManager;             // +0x1290 (Size: 0x610, ends at +0x18A0)
    std::uint32_t m_OriginalOwnerXuidLow;                // +0x18A0
    std::uint32_t m_OriginalOwnerXuidHigh;               // +0x18A4
    std::int32_t m_nFallbackPaintKit;                    // +0x18A8
    std::int32_t m_nFallbackSeed;                        // +0x18AC
    float m_flFallbackWear;                              // +0x18B0
    std::int32_t m_nFallbackStatTrak;                    // +0x18B4
    bool m_bClientside;                                  // +0x18B8
    bool m_bParticleSystemsCreated;                      // +0x18B9
    PAD(0x06, m_pad18BA);                                // +0x18BA..+0x18C0
    PAD(0x18, m_vecAttachedParticles);                   // +0x18C0..+0x18D8
    CEntityHandle m_hViewmodelAttachment;                // +0x18D8
    std::int32_t m_iOldTeam;                             // +0x18DC
    bool m_bAttachmentDirty;                             // +0x18E0
    PAD(0x03, m_pad18E1);                                // +0x18E1..+0x18E4
    std::int32_t m_nUnloadedModelIndex;                  // +0x18E4
    std::int32_t m_iNumOwnerValidationRetries;           // +0x18E8
    PAD(0x0C, m_pad18EC);                                // +0x18EC..+0x18F8
    CEntityHandle m_hOldProvidee;                        // +0x18F8
    PAD(0x04, m_pad18FC);                                // +0x18FC..+0x1900
    PAD(0x18, m_vecAttachedModels);                      // +0x1900..+0x1918

    [[nodiscard]] inline C_EconItemView& GetItemView() noexcept {
        return m_AttributeManager.m_Item;
    }

    [[nodiscard]] inline const C_EconItemView& GetItemView() const noexcept {
        return m_AttributeManager.m_Item;
    }

    [[nodiscard]] constexpr std::int32_t GetFallbackPaintKit() const noexcept {
        return m_nFallbackPaintKit;
    }

    [[nodiscard]] constexpr std::int32_t GetFallbackSeed() const noexcept {
        return m_nFallbackSeed;
    }

    [[nodiscard]] constexpr float GetFallbackWear() const noexcept {
        return m_flFallbackWear;
    }

    [[nodiscard]] constexpr std::int32_t GetFallbackStatTrak() const noexcept {
        return m_nFallbackStatTrak;
    }
};
static_assert(sizeof(C_EconEntity) == 0x1918, "C_EconEntity size must be 0x1918");
static_assert(offsetof(C_EconEntity, m_flFlexDelayTime) == 0x1278, "m_flFlexDelayTime offset mismatch");
static_assert(offsetof(C_EconEntity, m_flFlexDelayedWeight) == 0x1280, "m_flFlexDelayedWeight offset mismatch");
static_assert(offsetof(C_EconEntity, m_bAttributesInitialized) == 0x1288, "m_bAttributesInitialized offset mismatch");
static_assert(offsetof(C_EconEntity, m_AttributeManager) == 0x1290, "m_AttributeManager offset mismatch");
static_assert(offsetof(C_EconEntity, m_OriginalOwnerXuidLow) == 0x18A0, "m_OriginalOwnerXuidLow offset mismatch");
static_assert(offsetof(C_EconEntity, m_OriginalOwnerXuidHigh) == 0x18A4, "m_OriginalOwnerXuidHigh offset mismatch");
static_assert(offsetof(C_EconEntity, m_nFallbackPaintKit) == 0x18A8, "m_nFallbackPaintKit offset mismatch");
static_assert(offsetof(C_EconEntity, m_nFallbackSeed) == 0x18AC, "m_nFallbackSeed offset mismatch");
static_assert(offsetof(C_EconEntity, m_flFallbackWear) == 0x18B0, "m_flFallbackWear offset mismatch");
static_assert(offsetof(C_EconEntity, m_nFallbackStatTrak) == 0x18B4, "m_nFallbackStatTrak offset mismatch");
static_assert(offsetof(C_EconEntity, m_bClientside) == 0x18B8, "m_bClientside offset mismatch");
static_assert(offsetof(C_EconEntity, m_bParticleSystemsCreated) == 0x18B9, "m_bParticleSystemsCreated offset mismatch");
static_assert(offsetof(C_EconEntity, m_hViewmodelAttachment) == 0x18D8, "m_hViewmodelAttachment offset mismatch");
static_assert(offsetof(C_EconEntity, m_iOldTeam) == 0x18DC, "m_iOldTeam offset mismatch");
static_assert(offsetof(C_EconEntity, m_bAttachmentDirty) == 0x18E0, "m_bAttachmentDirty offset mismatch");
static_assert(offsetof(C_EconEntity, m_nUnloadedModelIndex) == 0x18E4, "m_nUnloadedModelIndex offset mismatch");
static_assert(offsetof(C_EconEntity, m_iNumOwnerValidationRetries) == 0x18E8, "m_iNumOwnerValidationRetries offset mismatch");
static_assert(offsetof(C_EconEntity, m_hOldProvidee) == 0x18F8, "m_hOldProvidee offset mismatch");

// ============================================================================
// 8. Static Item Definition Schema (CEconItemDefinition)
// Canonical item archetype loaded from items_game.txt.
// ============================================================================
struct CEconItemDefinition {
    std::uint16_t m_nDefIndex = 0;
    LoadoutSlot_t m_nLoadoutSlot = LoadoutSlot_t::LOADOUT_SLOT_MELEE;
    EquipmentCategory_t m_nEquipmentCategory = EquipmentCategory_t::CATEGORY_OTHER;
    EconQuality_t m_nQuality = EconQuality_t::QUALITY_NORMAL;
    EconRarity_t m_nRarity = EconRarity_t::RARITY_DEFAULT;
    std::uint32_t m_nDefaultDropQuantity = 1;
    std::uint32_t m_nItemLevelMin = 1;
    std::uint32_t m_nItemLevelMax = 1;
    const char* m_pszItemBaseName = "";
    const char* m_pszItemTypeName = "";
    const char* m_pszItemDescription = "";
    const char* m_pszInventoryImage = "";
    const char* m_pszModelPlayer = "";
    const char* m_pszModelWorld = "";

    [[nodiscard]] constexpr std::uint16_t GetDefinitionIndex() const noexcept { return m_nDefIndex; }
    [[nodiscard]] constexpr LoadoutSlot_t GetLoadoutSlot() const noexcept { return m_nLoadoutSlot; }
    [[nodiscard]] constexpr EquipmentCategory_t GetEquipmentCategory() const noexcept { return m_nEquipmentCategory; }
    [[nodiscard]] constexpr EconQuality_t GetQuality() const noexcept { return m_nQuality; }
    [[nodiscard]] constexpr EconRarity_t GetRarity() const noexcept { return m_nRarity; }
    [[nodiscard]] constexpr const char* GetItemBaseName() const noexcept { return m_pszItemBaseName; }
    [[nodiscard]] constexpr const char* GetItemTypeName() const noexcept { return m_pszItemTypeName; }
    [[nodiscard]] constexpr const char* GetItemDescription() const noexcept { return m_pszItemDescription; }
    [[nodiscard]] constexpr const char* GetInventoryImage() const noexcept { return m_pszInventoryImage; }
    [[nodiscard]] constexpr const char* GetModelPlayer() const noexcept { return m_pszModelPlayer; }
    [[nodiscard]] constexpr const char* GetModelWorld() const noexcept { return m_pszModelWorld; }
    [[nodiscard]] constexpr bool IsWeapon() const noexcept {
        return m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_SECONDARY ||
               m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_SMG ||
               m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_RIFLE ||
               m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_HEAVY;
    }
    [[nodiscard]] constexpr bool IsKnife() const noexcept {
        return m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_MELEE;
    }
    [[nodiscard]] constexpr bool IsGlove() const noexcept {
        return m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_GLOVES;
    }
    [[nodiscard]] constexpr bool IsAgent() const noexcept {
        return m_nEquipmentCategory == EquipmentCategory_t::CATEGORY_AGENT;
    }
};

// ============================================================================
// 9. Paint Kit Descriptor (CPaintKitDefinition)
// Weapon skin surface finish parameters & procedural texture projection rules.
// ============================================================================
struct CPaintKitDefinition {
    std::int32_t m_nID = 0;
    const char* m_pszName = "";
    const char* m_pszDescription = "";
    const char* m_pszTemplate = "";
    PaintKitStyle_t m_nStyle = PaintKitStyle_t::PAINT_KIT_STYLE_CUSTOM_PAINT_JOB;
    float m_flWearRemapMin = 0.00f;
    float m_flWearRemapMax = 1.00f;
    float m_flPatternScale = 1.00f;
    float m_flPatternOffsetXStart = 0.00f;
    float m_flPatternOffsetXEnd = 1.00f;
    float m_flPatternOffsetYStart = 0.00f;
    float m_flPatternOffsetYEnd = 1.00f;
    float m_flPatternRotateStart = 0.00f;
    float m_flPatternRotateEnd = 360.00f;
    float m_flPearlescent = 0.00f;
};

// ============================================================================
// 10. Wear Degradation Mathematics & Procedural Projections
// ============================================================================

[[nodiscard]] constexpr WearRating_t EvaluateWearCondition(float wear) noexcept {
    if (wear < 0.07f) return WearRating_t::WEAR_FACTORY_NEW;
    if (wear < 0.15f) return WearRating_t::WEAR_MINIMAL_WEAR;
    if (wear < 0.38f) return WearRating_t::WEAR_FIELD_TESTED;
    if (wear < 0.45f) return WearRating_t::WEAR_WELL_WORN;
    return WearRating_t::WEAR_BATTLE_SCARRED;
}

[[nodiscard]] constexpr const char* GetWearRatingName(WearRating_t rating) noexcept {
    switch (rating) {
        case WearRating_t::WEAR_FACTORY_NEW: return "Factory New";
        case WearRating_t::WEAR_MINIMAL_WEAR: return "Minimal Wear";
        case WearRating_t::WEAR_FIELD_TESTED: return "Field-Tested";
        case WearRating_t::WEAR_WELL_WORN: return "Well-Worn";
        case WearRating_t::WEAR_BATTLE_SCARRED: return "Battle-Scarred";
        default: return "Unknown";
    }
}

[[nodiscard]] constexpr float RemapWear(float rawWear, float minWear, float maxWear) noexcept {
    float clamped = rawWear < 0.0f ? 0.0f : (rawWear > 1.0f ? 1.0f : rawWear);
    return minWear + clamped * (maxWear - minWear);
}

} // namespace cs2::sdk
