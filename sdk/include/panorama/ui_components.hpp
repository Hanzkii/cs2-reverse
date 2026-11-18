#pragma once

// ============================================================================
// Source 2 Reverse Engineered SDK - Panorama UI Component Schemas
// Target Binaries: client.dll, panorama.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Documented & Reconstructed Schema Components:
//   - C_PanoramaImage (Image scaling, tint, texture resource binding)
//   - C_PanoramaLabel (Typography, dialog variables, localization)
//   - C_PanoramaButton (State machine, activation sounds, commands)
//   - C_PanoramaProgressBar (Min/Max range, progress fraction, fill directions)
//   - C_BaseClientUIEntity / C_PointClientUIHUD / C_PointClientUIWorldPanel
// ============================================================================

#include "cuipanel.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace cs2::panorama {

// ============================================================================
// 1. Color RGBA Helper
// ============================================================================
struct Color_t {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};

    [[nodiscard]] constexpr float R() const noexcept { return static_cast<float>(r) / 255.0f; }
    [[nodiscard]] constexpr float G() const noexcept { return static_cast<float>(g) / 255.0f; }
    [[nodiscard]] constexpr float B() const noexcept { return static_cast<float>(b) / 255.0f; }
    [[nodiscard]] constexpr float A() const noexcept { return static_cast<float>(a) / 255.0f; }
};

// ============================================================================
// 2. Schema Enums
// ============================================================================
enum class EImageScalingMode : std::uint8_t {
    Stretch = 0,
    None    = 1,
    Cover   = 2,
    Contain = 3
};

enum class ETextAlignment : std::uint8_t {
    Left    = 0,
    Center  = 1,
    Right   = 2,
    Justify = 3
};

enum class EProgressDirection : std::uint8_t {
    LeftToRight = 0,
    RightToLeft = 1,
    BottomToTop = 2,
    TopToBottom = 3,
    Radial      = 4
};

// ============================================================================
// 3. Schema Struct: HUDPanelDialogVariableString_t (Size: 32 bytes)
// Schema: client.dll / server.dll (Verified via extracted_schemas)
// ============================================================================
struct HUDPanelDialogVariableString_t {
    std::uint8_t   pad00[8]{};             // 0x00..0x07
    std::uint16_t  m_nPanelIdIndex{0};     // 0x08
    std::int32_t   m_nDialogVariableIndex{0};// 0x0A..0x0D
    std::uint8_t   pad0E[2]{};             // 0x0E..0x0F
    std::uint64_t  m_sValue{0};            // 0x10 (CUtlSymbolLarge or string pointer)
    bool           m_bIsSet{false};        // 0x18
    std::uint8_t   pad19[7]{};             // 0x19..0x1F
};
static_assert(sizeof(HUDPanelDialogVariableString_t) == 32, "HUDPanelDialogVariableString_t size must be 32 bytes");

// ============================================================================
// 4. Schema Struct: HUDPanelHasClass_t (Size: 8 bytes)
// Schema: client.dll / server.dll (Verified via extracted_schemas)
// ============================================================================
struct HUDPanelHasClass_t {
    std::uint16_t  m_nPanelIdIndex{0};     // 0x00
    std::uint16_t  m_nClassNameIndex{0};   // 0x02
    std::uint32_t  m_eClassStatus{0};      // 0x04
};
static_assert(sizeof(HUDPanelHasClass_t) == 8, "HUDPanelHasClass_t size must be 8 bytes");

// ============================================================================
// 5. C_PanoramaImage Component
// Corresponds to <Image> tag and client-side CCSGO_BackbufferImagePanel
// ============================================================================
class C_PanoramaImage : public CUIPanel {
public:
    std::string        m_strImagePath;                     // Source 2 resource path (e.g. "s2r://panorama/images/...")
    EImageScalingMode  m_eScaling{EImageScalingMode::Cover};
    Color_t            m_tintColor{255, 255, 255, 255};
    float              m_flBlurRadius{0.0f};
    bool               m_bDisableMipGen{false};
    bool               m_bOnlyRenderToTexture{false};
    void*              m_hTextureHandle{nullptr};          // GPU resource texture reference

    void SetImage(std::string_view svPath) {
        m_strImagePath = svPath;
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::LayoutDirty;
    }

    void SetScaling(EImageScalingMode mode) noexcept {
        m_eScaling = mode;
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::LayoutDirty;
    }

    void SetTint(Color_t color) noexcept {
        m_tintColor = color;
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::StylesDirty;
    }
};

// ============================================================================
// 6. C_PanoramaLabel Component
// Corresponds to <Label> tag and client-side CCSGO_GameTimeLabel
// ============================================================================
class C_PanoramaLabel : public CUIPanel {
public:
    std::string        m_strText;                          // Raw or formatted string
    std::string        m_strLocalizationKey;               // Localization token (e.g. "#CSGO_Scoreboard_Title")
    std::string        m_strFontFamily{"Stratum2"};
    float              m_flFontSize{14.0f};
    float              m_flLineHeight{1.2f};
    ETextAlignment     m_eAlignment{ETextAlignment::Left};
    Color_t            m_textColor{255, 255, 255, 255};
    bool               m_bHtmlAllowed{false};
    bool               m_bAutoScroll{false};
    bool               m_bEllipsis{true};

    // Dialog variable dictionary (e.g. {s:player_name}, {d:score})
    std::unordered_map<std::string, std::string> m_dialogVariables;

    void SetText(std::string_view svText) {
        m_strText = svText;
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::LayoutDirty;
    }

    void SetDialogVariable(std::string_view svKey, std::string_view svVal) {
        m_dialogVariables[std::string(svKey)] = std::string(svVal);
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::LayoutDirty;
    }

    [[nodiscard]] std::string GetEvaluatedText() const {
        if (m_dialogVariables.empty()) return m_strText;
        std::string result = m_strText;
        for (const auto& [k, v] : m_dialogVariables) {
            std::string needle = "{" + k + "}";
            size_t pos = 0;
            while ((pos = result.find(needle, pos)) != std::string::npos) {
                result.replace(pos, needle.length(), v);
                pos += v.length();
            }
        }
        return result;
    }
};

// ============================================================================
// 7. C_PanoramaButton Component
// Corresponds to <Button> tag
// ============================================================================
class C_PanoramaButton : public CUIPanel {
public:
    std::string        m_strActivationCommand;             // Console command or JS dispatcher
    std::string        m_strSoundHover{"panorama.sound_focus"};
    std::string        m_strSoundPress{"panorama.sound_click"};
    bool               m_bAcceptsRightClick{false};

    [[nodiscard]] bool IsHovered() const noexcept {
        return (m_nStyleFlags & EStyleFlags::Hover) != EStyleFlags::None;
    }

    [[nodiscard]] bool IsActive() const noexcept {
        return (m_nStyleFlags & EStyleFlags::Active) != EStyleFlags::None;
    }

    [[nodiscard]] bool IsDisabled() const noexcept {
        return (m_nStyleFlags & EStyleFlags::Disabled) != EStyleFlags::None;
    }

    void SetDisabled(bool bDisabled) noexcept {
        if (bDisabled) {
            m_nStyleFlags = m_nStyleFlags | EStyleFlags::Disabled;
        } else {
            m_nStyleFlags = static_cast<EStyleFlags>(static_cast<std::uint16_t>(m_nStyleFlags) & ~static_cast<std::uint16_t>(EStyleFlags::Disabled));
        }
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::StylesDirty;
    }
};

// ============================================================================
// 8. C_PanoramaProgressBar Component
// Corresponds to <ProgressBar> tag and client-side CCSGO_HudProgressBar
// ============================================================================
class C_PanoramaProgressBar : public CUIPanel {
public:
    float              m_flMin{0.0f};
    float              m_flMax{100.0f};
    float              m_flValue{100.0f};
    EProgressDirection m_eDirection{EProgressDirection::LeftToRight};
    float              m_flTransitionDuration{0.25f};      // Smooth interpolation transition
    bool               m_bIndeterminate{false};

    void SetRange(float flMin, float flMax) noexcept {
        m_flMin = flMin;
        m_flMax = flMax;
        SetValue(m_flValue);
    }

    void SetValue(float flVal) noexcept {
        if (flVal < m_flMin) flVal = m_flMin;
        if (flVal > m_flMax) flVal = m_flMax;
        m_flValue = flVal;
        m_nLayoutFlags = m_nLayoutFlags | EPanelLayoutFlags::StylesDirty;
    }

    [[nodiscard]] float GetProgressFraction() const noexcept {
        if (m_flMax <= m_flMin) return 0.0f;
        return (m_flValue - m_flMin) / (m_flMax - m_flMin);
    }
};

// ============================================================================
// 9. Client-Side Entity UI Base: C_BaseClientUIEntity
// Schema: client.dll (Size: 4296 bytes)
// ============================================================================
class C_BaseClientUIEntity {
public:
    std::uint8_t   pad00[4256]{};       // 0x0000..0x109F (C_BaseModelEntity base)
    bool           m_bEnabled{true};    // 0x10A0 (4256)
    std::uint8_t   pad10A1[7]{};        // 0x10A1..0x10A7
    std::uint64_t  m_DialogXMLName{0};  // 0x10A8 (4264)
    std::uint64_t  m_PanelClassName{0}; // 0x10B0 (4272)
    std::uint8_t   m_PanelID[16]{};     // 0x10B8 (4280) Vector4D / CUtlSymbolLarge
};
static_assert(offsetof(C_BaseClientUIEntity, m_bEnabled) == 4256, "m_bEnabled offset mismatch");
static_assert(offsetof(C_BaseClientUIEntity, m_DialogXMLName) == 4264, "m_DialogXMLName offset mismatch");
static_assert(offsetof(C_BaseClientUIEntity, m_PanelClassName) == 4272, "m_PanelClassName offset mismatch");
static_assert(offsetof(C_BaseClientUIEntity, m_PanelID) == 4280, "m_PanelID offset mismatch");

// ============================================================================
// 10. Client-Side HUD Point Entity: C_PointClientUIHUD
// Schema: client.dll (Size: 4752 bytes)
// ============================================================================
class C_PointClientUIHUD : public C_BaseClientUIEntity {
public:
    bool           m_bCheckCSSClasses{false}; // 0x10D0 (4304)
    std::uint8_t   pad10D1[375]{};            // 0x10D1..0x1247
    bool           m_bIgnoreInput{false};     // 0x1248 (4680)
    std::uint8_t   pad1249[3]{};              // 0x1249..0x124B
    float          m_flWidth{1920.0f};        // 0x124C (4684)
    float          m_flHeight{1080.0f};       // 0x1250 (4688)
    float          m_flDPI{96.0f};            // 0x1254 (4692)
    float          m_flInteractDistance{0.0f};// 0x1258 (4696)
    float          m_flDepthOffset{0.0f};     // 0x125C (4700)
    std::uint32_t  m_unOwnerContext{0};       // 0x1260 (4704)
    std::uint32_t  m_unHorizontalAlign{0};    // 0x1264 (4708)
    std::uint32_t  m_unVerticalAlign{0};      // 0x1268 (4712)
    std::uint32_t  m_unOrientation{0};        // 0x126C (4716)
    bool           m_bAllowInteractionAll{false}; // 0x1270 (4720)
    std::uint8_t   pad1271[7]{};              // 0x1271..0x1277
    float          m_vecCSSClasses[3]{};      // 0x1278 (4728)
};
static_assert(offsetof(C_PointClientUIHUD, m_bCheckCSSClasses) == 4304, "m_bCheckCSSClasses offset mismatch");
static_assert(offsetof(C_PointClientUIHUD, m_bIgnoreInput) == 4680, "m_bIgnoreInput offset mismatch");
static_assert(offsetof(C_PointClientUIHUD, m_flWidth) == 4684, "m_flWidth offset mismatch");
static_assert(offsetof(C_PointClientUIHUD, m_flHeight) == 4688, "m_flHeight offset mismatch");
static_assert(offsetof(C_PointClientUIHUD, m_flDPI) == 4692, "m_flDPI offset mismatch");
static_assert(offsetof(C_PointClientUIHUD, m_unOwnerContext) == 4704, "m_unOwnerContext offset mismatch");

// ============================================================================
// 11. Client-Side World Panel Entity: C_PointClientUIWorldPanel
// Schema: client.dll (Size: 4864 bytes)
// ============================================================================
class C_PointClientUIWorldPanel : public C_BaseClientUIEntity {
public:
    bool           m_bForceRecreateNextUpdate{false}; // 0x10D0 (4304)
    bool           m_bMoveViewToPlayerNextThink{false};// 0x10D1 (4305)
    bool           m_bCheckCSSClasses{false};         // 0x10D2 (4306)
    std::uint8_t   pad10D3[13]{};                     // 0x10D3..0x10DF
    std::uint8_t   m_anchorDeltaTransform[400]{};     // 0x10E0 (4320)
    void*          m_pOffScreenIndicator{nullptr};    // 0x1270 (4720)
    std::uint8_t   pad1278[32]{};                     // 0x1278..0x1297
    bool           m_bIgnoreInput{false};             // 0x1298 (4760)
    bool           m_bLit{false};                     // 0x1299 (4761)
    bool           m_bFollowPlayerAcrossTeleport{false};// 0x129A (4762)
    std::uint8_t   pad129B[1]{};                      // 0x129B
    float          m_flWidth{100.0f};                 // 0x129C (4764)
    float          m_flHeight{100.0f};                // 0x12A0 (4768)
    float          m_flDPI{96.0f};                    // 0x12A4 (4772)
    float          m_flWindowUIScale{1.0f};           // 0x12A8 (4776)
    float          m_flInteractDistance{150.0f};       // 0x12AC (4780)
    float          m_flDepthOffset{0.0f};             // 0x12B0 (4784)
    std::uint32_t  m_unOwnerContext{0};               // 0x12B4 (4788)
    std::uint32_t  m_unHorizontalAlign{0};            // 0x12B8 (4792)
    std::uint32_t  m_unVerticalAlign{0};              // 0x12BC (4796)
    std::uint32_t  m_unOrientation{0};                // 0x12C0 (4800)
    bool           m_bAllowInteractionFromAllSceneWorlds{false}; // 0x12C4 (4804)
    std::uint8_t   pad12C5[3]{};                      // 0x12C5..0x12C7
    float          m_vecCSSClasses[3]{};              // 0x12C8 (4808)
    std::uint8_t   pad12D4[12]{};                     // 0x12D4..0x12DF
    bool           m_bOpaque{false};                  // 0x12E0 (4832)
    bool           m_bNoDepth{false};                 // 0x12E1 (4833)
    bool           m_bVisibleWhenParentNoDraw{false}; // 0x12E2 (4834)
    bool           m_bRenderBackface{false};          // 0x12E3 (4835)
    bool           m_bUseOffScreenIndicator{false};   // 0x12E4 (4836)
    bool           m_bExcludeFromSaveGames{false};    // 0x12E5 (4837)
    bool           m_bGrabbable{false};               // 0x12E6 (4838)
    bool           m_bOnlyRenderToTexture{false};     // 0x12E7 (4839)
    bool           m_bDisableMipGen{false};           // 0x12E8 (4840)
    std::uint8_t   pad12E9[3]{};                      // 0x12E9..0x12EB
    std::int32_t   m_nExplicitImageLayout{0};         // 0x12EC (4844)
    bool           m_bIgnoreParentOrientation{false}; // 0x12F0 (4848)
};
static_assert(offsetof(C_PointClientUIWorldPanel, m_bForceRecreateNextUpdate) == 4304, "m_bForceRecreateNextUpdate offset mismatch");
static_assert(offsetof(C_PointClientUIWorldPanel, m_flWidth) == 4764, "m_flWidth offset mismatch");
static_assert(offsetof(C_PointClientUIWorldPanel, m_flHeight) == 4768, "m_flHeight offset mismatch");
static_assert(offsetof(C_PointClientUIWorldPanel, m_bOnlyRenderToTexture) == 4839, "m_bOnlyRenderToTexture offset mismatch");

} // namespace cs2::panorama
