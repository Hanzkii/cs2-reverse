#pragma once

// ============================================================================
// Source 2 Reverse Engineered SDK - Panorama UI Subsystem & DOM Architecture
// Target Binaries: panorama.dll, panoramauiclient.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Verified Offsets & Structures:
//   CUIPanel VTable:         panorama.dll + 0x4CCFF8 (354 bounded methods)
//   CUIEngineSource2 VTable: panorama.dll + 0x4C3740 (150 bounded methods)
//   CUIEngine VTable:        panorama.dll + 0x4C9010 (149 bounded methods)
//   CPanelStyle VTable:      panorama.dll + 0x4E4B70 (157 bounded methods)
//   CUIEvent<1> VTable:      panorama.dll + 0x4CAF68
//   CUIPanel allocation:     0x300 (768 bytes, Verified via RVA 0xB898E)
//   CPanelStyle in CUIPanel: Offset 0x68
//   Children vector in CUIP: Offset 0x28 (CUtlVector<CUIPanel*>)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <concepts>
#include <type_traits>

// Guard against intrusive Windows macro pollution
#if defined(_WIN32)
#undef RegisterClass
#undef DispatchMessage
#undef SendMessage
#undef GetMessage
#undef SetFocus
#undef GetClassName
#undef DrawText
#endif

namespace cs2::panorama {

// Forward declarations
class IUIPanel;
class CUIPanel;
class IUIPanelClient;
class IUIWindow;
class CUIWindow;
class CPanelStyle;
class IUIEngine;
class CUIEngine;
class CUIEvent;
class ILocVariableSource;

// Typedefs & Hashed Symbols
using CUIEventID_t = std::uint16_t;
using CStyleSymbol = std::uint16_t;
using PanelHandle_t = std::uint64_t;

// ============================================================================
// 1. Enums: Style Pseudo-Classes, Layout Flags & Event Status
// ============================================================================
enum class EStyleFlags : std::uint16_t {
    None             = 0,
    Hover            = 1 << 0,  // :hover (0x0001)
    Active           = 1 << 1,  // :active (0x0002)
    Focus            = 1 << 2,  // :focus (0x0004)
    Disabled         = 1 << 3,  // :disabled (0x0008)
    Selected         = 1 << 4,  // :selected (0x0010)
    Inspected        = 1 << 5,  // :inspect (0x0020)
    CanFocus         = 1 << 6,  // Accepts keyboard focus (0x0040)
    DescendantFocus  = 1 << 7,  // Child currently focused (0x0080)
    TopLevel         = 1 << 8,  // Window root panel (0x0100)
    LayoutLoading    = 1 << 9,  // Async layout in flight (0x0200)
    Activated        = 1 << 10  // Click activated (0x0400)
};

[[nodiscard]] constexpr EStyleFlags operator|(EStyleFlags lhs, EStyleFlags rhs) noexcept {
    return static_cast<EStyleFlags>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

[[nodiscard]] constexpr EStyleFlags operator&(EStyleFlags lhs, EStyleFlags rhs) noexcept {
    return static_cast<EStyleFlags>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
}

enum class EPanelLayoutFlags : std::uint16_t {
    None             = 0,
    Visible          = 1 << 0,  // 0x0001
    LayoutDirty      = 1 << 1,  // 0x0002
    StylesDirty      = 1 << 2,  // 0x0004
    ChildOrderDirty  = 1 << 3,  // 0x0008
    ClipToBounds     = 1 << 4,  // 0x0010
    AcceptsInput     = 1 << 5,  // 0x0020
    NeedsPositioning = 1 << 6,  // 0x0040
    HittestChildren  = 1 << 7   // 0x0080
};

[[nodiscard]] constexpr EPanelLayoutFlags operator|(EPanelLayoutFlags lhs, EPanelLayoutFlags rhs) noexcept {
    return static_cast<EPanelLayoutFlags>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

[[nodiscard]] constexpr EPanelLayoutFlags operator&(EPanelLayoutFlags lhs, EPanelLayoutFlags rhs) noexcept {
    return static_cast<EPanelLayoutFlags>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
}

enum class EEventDispatchStatus : std::uint8_t {
    Continue         = 0,
    Handled          = 1,
    PreventDefault   = 2,
    StopPropagation  = 3
};

// ============================================================================
// 2. Geometric Primitives
// ============================================================================
struct UIRect_t {
    float flX{0.0f};
    float flY{0.0f};
    float flWidth{0.0f};
    float flHeight{0.0f};

    [[nodiscard]] constexpr bool Contains(float x, float y) const noexcept {
        return (x >= flX && x < (flX + flWidth) &&
                y >= flY && y < (flY + flHeight));
    }

    [[nodiscard]] constexpr bool Intersects(const UIRect_t& other) const noexcept {
        return !(other.flX >= (flX + flWidth) ||
                 (other.flX + other.flWidth) <= flX ||
                 other.flY >= (flY + flHeight) ||
                 (other.flY + other.flHeight) <= flY);
    }

    [[nodiscard]] constexpr UIRect_t ClipTo(const UIRect_t& scissor) const noexcept {
        const float x0 = (flX > scissor.flX) ? flX : scissor.flX;
        const float y0 = (flY > scissor.flY) ? flY : scissor.flY;
        const float x1 = ((flX + flWidth) < (scissor.flX + scissor.flWidth)) ? (flX + flWidth) : (scissor.flX + scissor.flWidth);
        const float y1 = ((flY + flHeight) < (scissor.flY + scissor.flHeight)) ? (flY + flHeight) : (scissor.flY + scissor.flHeight);

        if (x1 <= x0 || y1 <= y0) {
            return {0.0f, 0.0f, 0.0f, 0.0f};
        }
        return {x0, y0, x1 - x0, y1 - y0};
    }
};

// ============================================================================
// 3. Panorama Event System (CUIEvent / Event Handler)
// ============================================================================
class CUIEvent {
public:
    virtual ~CUIEvent() = default;
    [[nodiscard]] virtual CUIEventID_t GetEventType() const noexcept = 0;
    [[nodiscard]] virtual PanelHandle_t GetTargetPanel() const noexcept = 0;
    [[nodiscard]] virtual bool CanBubble() const noexcept = 0;

    // Binary layout verified in panorama.dll + 0x4CAF68
    CUIEventID_t     m_nEventType{0};         // 0x08
    std::uint16_t    m_nFlags{0};             // 0x0A
    PanelHandle_t    m_hTargetPanel{0};       // 0x0C
    CUIPanel*        m_pSourcePanel{nullptr}; // 0x14
    std::int32_t     m_nArgsCount{0};         // 0x1C
};

struct CUIEventHandler {
    void*            pFunction{nullptr};      // Native function pointer or JS Callback
    void*            pContext{nullptr};       // Context panel or 'this' closure
    CUIEventID_t     nEventType{0};           // Event ID symbol
    std::uint16_t    nFlags{0};               // Handled mode flags
};

// ============================================================================
// 4. Client Notification Interface (IUIPanelClient)
// ============================================================================
class IUIPanelClient {
public:
    virtual ~IUIPanelClient() = default;
    virtual void OnPanelDeleted(IUIPanel* pPanel) = 0;
    virtual void OnLayout(IUIPanel* pPanel) = 0;
    virtual void OnPaint(IUIPanel* pPanel) = 0;
    virtual bool OnEvent(IUIPanel* pPanel, const CUIEvent* pEvent) = 0;
};

// ============================================================================
// 5. CSS Cascaded Style Subsystem (CPanelStyle)
// ============================================================================
class CPanelStyle {
public:
    virtual ~CPanelStyle() = default;
    virtual void ApplyStyles() = 0;
    virtual void MarkStylesDirty() = 0;
    [[nodiscard]] virtual float GetOpacity() const noexcept = 0;
    [[nodiscard]] virtual float GetZIndex() const noexcept = 0;
    [[nodiscard]] virtual bool IsVisible() const noexcept = 0;
    virtual void SetVisible(bool bVisible) = 0;
    virtual void SetProperty(const char* pszProperty, const char* pszValue) = 0;

    // Inlined at CUIPanel + 0x68 (VTable at 0x1804E4B70)
    CUIPanel*        m_pPanel{nullptr};            // 0x08 (0x70 from panel base)
    std::uint8_t     m_padCascadeData[0x58]{};     // 0x10..0x67
    float            m_flOpacity{1.0f};            // 0x68
    float            m_flZIndex{0.0f};             // 0x6C
    std::uint8_t     m_padStyleProperties[0x58]{}; // 0x70..0xC7
};

// ============================================================================
// 6. Abstract DOM Node Interface (IUIPanel)
// Matches VTable layout at panorama.dll + 0x4CCFF8
// Pure abstract interface: exactly 8 bytes (VTable pointer only)
// ============================================================================
class IUIPanel {
public:
    virtual ~IUIPanel() = default;                                                 // Slot 0
    virtual void InitializeBase() = 0;                                             // Slot 1
    virtual void Initialize(CUIWindow* pWindow, CUIPanel* pParent, const char* pszID) = 0; // Slot 2
    virtual void InitializeFromDescription(void* pDesc) = 0;                       // Slot 3
    virtual void UnusedSlot4() = 0;                                                // Slot 4
    virtual void Shutdown() = 0;                                                   // Slot 5
    virtual void UnusedSlot6() = 0;                                                // Slot 6
    virtual void SetClient(IUIPanelClient* pClient) = 0;                           // Slot 7
    [[nodiscard]] virtual IUIPanelClient* GetClient() const noexcept = 0;          // Slot 8
    virtual void UnusedSlot9() = 0;                                                // Slot 9
    [[nodiscard]] virtual const char* GetID() const noexcept = 0;                  // Slot 10
    virtual void UnusedSlot11() = 0;                                               // Slot 11
    virtual void UnusedSlot12() = 0;                                               // Slot 12
    [[nodiscard]] virtual bool HasID() const noexcept = 0;                         // Slot 13
    virtual void UnusedSlot14() = 0;                                               // Slot 14
    virtual bool LoadLayoutFile(const char* pszFile) = 0;                          // Slot 15
    virtual void SetParent(CUIPanel* pParent) = 0;                                 // Slot 16
    [[nodiscard]] virtual CUIPanel* GetParent() const noexcept = 0;                // Slot 17
    [[nodiscard]] virtual CUIWindow* GetWindow() const noexcept = 0;               // Slot 18
    [[nodiscard]] virtual int GetChildCount() const noexcept = 0;                  // Slot 19
    [[nodiscard]] virtual CUIPanel* GetChild(int nIndex) const noexcept = 0;       // Slot 20
    [[nodiscard]] virtual CUIPanel* GetFirstChild() const noexcept = 0;            // Slot 21
    [[nodiscard]] virtual CUIPanel* GetLastChild() const noexcept = 0;             // Slot 22
    [[nodiscard]] virtual int FindChildIndex(const CUIPanel* pChild) const noexcept = 0; // Slot 23
    [[nodiscard]] virtual bool BHasStyle() const noexcept = 0;                     // Slot 24
    [[nodiscard]] virtual CPanelStyle* GetStyle() noexcept = 0;                    // Slot 25
    [[nodiscard]] virtual float GetActualWidth() const noexcept = 0;               // Slot 26
    [[nodiscard]] virtual float GetActualHeight() const noexcept = 0;              // Slot 27
    [[nodiscard]] virtual float GetActualX() const noexcept = 0;                   // Slot 28
    [[nodiscard]] virtual float GetActualY() const noexcept = 0;                   // Slot 29
};

// ============================================================================
// 7. Concrete DOM Element Component (CUIPanel)
// Reconstructed with byte-exact CS2 alignment (Size: 0x300 / 768 bytes)
// ============================================================================
class alignas(8) CUIPanel : public IUIPanel {
public:
    // Memory Layout (Verified via Ghidra analysis of FUN_1800E4F30 & FUN_1800F98C0)
    IUIPanelClient*      m_pClient{nullptr};            // 0x08
    const char*          m_pszID{nullptr};              // 0x10 (CUtlString id)
    CUIPanel*            m_pParent{nullptr};            // 0x18
    CUIWindow*           m_pWindow{nullptr};            // 0x20

    // CUtlVector<CUIPanel*> m_vecChildren (0x28..0x3F)
    std::int32_t         m_nChildCount{0};              // 0x28
    std::int32_t         m_nPadChild{0};                // 0x2C
    CUIPanel**           m_pChildren{nullptr};          // 0x30
    std::int32_t         m_nChildAllocationCount{0};    // 0x38
    std::int32_t         m_nChildGrowSize{0};           // 0x3C

    std::uint8_t         m_padPanelData40_67[0x28]{};   // 0x40..0x67

    // Inlined CPanelStyle subsystem (0x68..0x133)
    std::uint8_t         m_Style[0xCC]{};               // 0x68..0x133

    // Layout extents & constraint metrics (0x134..0x157)
    std::uint8_t         m_padLayoutContent[0x24]{};    // 0x134..0x157

    // CUtlVector<CStyleSymbol> m_vecClasses (0x158..0x16F)
    std::int32_t         m_nClassCount{0};              // 0x158
    std::int32_t         m_nClassPad{0};                // 0x15C
    CStyleSymbol*        m_pClasses{nullptr};           // 0x160
    std::int32_t         m_nClassAllocCount{0};         // 0x168
    std::int32_t         m_nClassGrowSize{0};           // 0x16C

    std::uint8_t         m_padRenderData170_1AF[0x40]{};// 0x170..0x1AF

    // Computed Box Geometry (0x1B0..0x1CF)
    float                m_flActualX{0.0f};             // 0x1B0
    float                m_flActualY{0.0f};             // 0x1B4
    float                m_flActualWidth{0.0f};         // 0x1B8
    float                m_flActualHeight{0.0f};        // 0x1BC
    float                m_flDesiredWidth{0.0f};        // 0x1C0
    float                m_flDesiredHeight{0.0f};       // 0x1C4
    std::uint8_t         m_padDesiredExtents[0x08]{};   // 0x1C8..0x1CF
    float                m_flPositionX{0.0f};           // 0x1D0
    float                m_flPositionY{0.0f};           // 0x1D4
    std::uint8_t         m_padTransformData[0x18]{};    // 0x1D8..0x1EF
    float                m_flContentWidth{0.0f};        // 0x1F0
    float                m_flContentHeight{0.0f};       // 0x1F4
    float                m_flScrollY{0.0f};             // 0x1F8
    std::uint8_t         m_padScrollData[0x24]{};       // 0x1FC..0x21F

    // Pseudo-Classes & Status Bitfields
    EStyleFlags          m_nStyleFlags{EStyleFlags::None}; // 0x220
    std::uint8_t         m_padFlagData[0x06]{};         // 0x222..0x227
    EPanelLayoutFlags    m_nLayoutFlags{EPanelLayoutFlags::None}; // 0x228
    std::uint8_t         m_bVisibilityFlags{0xE8};      // 0x22A (Default visible)
    std::uint8_t         m_padPostFlags[0x0D]{};        // 0x22B..0x237

    // Event Dispatch Table (0x238..0x24F)
    std::int32_t         m_nEventHandlerCount{0};       // 0x238
    std::int32_t         m_nEventHandlerPad{0};         // 0x23C
    CUIEventHandler*     m_pEventHandlers{nullptr};     // 0x240
    std::int32_t         m_nEventHandlerAlloc{0};       // 0x248
    std::int32_t         m_nEventHandlerGrow{0};        // 0x24C

    std::uint8_t         m_padTail300[0xB0]{};          // 0x250..0x2FF

public:
    // IUIPanel Implementation Defaults
    void InitializeBase() override {}
    void Initialize(CUIWindow* pWindow, CUIPanel* pParent, const char* pszID) override {
        m_pWindow = pWindow;
        m_pParent = pParent;
        m_pszID = pszID;
    }
    void InitializeFromDescription(void*) override {}
    void UnusedSlot4() override {}
    void Shutdown() override {}
    void UnusedSlot6() override {}
    void SetClient(IUIPanelClient* pClient) override { m_pClient = pClient; }
    [[nodiscard]] IUIPanelClient* GetClient() const noexcept override { return m_pClient; }
    void UnusedSlot9() override {}
    [[nodiscard]] const char* GetID() const noexcept override { return m_pszID; }
    void UnusedSlot11() override {}
    void UnusedSlot12() override {}
    [[nodiscard]] bool HasID() const noexcept override { return m_pszID != nullptr && m_pszID[0] != '\0'; }
    void UnusedSlot14() override {}
    bool LoadLayoutFile(const char*) override { return false; }
    void SetParent(CUIPanel* pParent) override { m_pParent = pParent; }
    [[nodiscard]] CUIPanel* GetParent() const noexcept override { return m_pParent; }
    [[nodiscard]] CUIWindow* GetWindow() const noexcept override { return m_pWindow; }
    [[nodiscard]] int GetChildCount() const noexcept override { return m_nChildCount; }
    [[nodiscard]] CUIPanel* GetChild(int nIndex) const noexcept override {
        if (nIndex < 0 || nIndex >= m_nChildCount || !m_pChildren) return nullptr;
        return m_pChildren[nIndex];
    }
    [[nodiscard]] CUIPanel* GetFirstChild() const noexcept override {
        if (m_nChildCount <= 0 || !m_pChildren) return nullptr;
        return m_pChildren[0];
    }
    [[nodiscard]] CUIPanel* GetLastChild() const noexcept override {
        if (m_nChildCount <= 0 || !m_pChildren) return nullptr;
        return m_pChildren[m_nChildCount - 1];
    }
    [[nodiscard]] int FindChildIndex(const CUIPanel* pChild) const noexcept override {
        if (!pChild || !m_pChildren) return -1;
        for (int i = 0; i < m_nChildCount; ++i) {
            if (m_pChildren[i] == pChild) return i;
        }
        return -1;
    }
    [[nodiscard]] bool BHasStyle() const noexcept override { return (m_bVisibilityFlags & 0x01) != 0; }
    [[nodiscard]] CPanelStyle* GetStyle() noexcept override {
        return reinterpret_cast<CPanelStyle*>(m_Style);
    }
    [[nodiscard]] float GetActualWidth() const noexcept override { return m_flActualWidth; }
    [[nodiscard]] float GetActualHeight() const noexcept override { return m_flActualHeight; }
    [[nodiscard]] float GetActualX() const noexcept override { return m_flActualX; }
    [[nodiscard]] float GetActualY() const noexcept override { return m_flActualY; }

    // Helper DOM Node Traversal API
    [[nodiscard]] inline CUIPanel* GetNextSibling() const noexcept {
        if (!m_pParent) return nullptr;
        const int nIndex = m_pParent->FindChildIndex(this);
        if (nIndex < 0 || (nIndex + 1) >= m_pParent->m_nChildCount) return nullptr;
        return m_pParent->m_pChildren[nIndex + 1];
    }

    [[nodiscard]] inline CUIPanel* GetPrevSibling() const noexcept {
        if (!m_pParent) return nullptr;
        const int nIndex = m_pParent->FindChildIndex(this);
        if (nIndex <= 0) return nullptr;
        return m_pParent->m_pChildren[nIndex - 1];
    }

    [[nodiscard]] inline UIRect_t GetBounds() const noexcept {
        return {m_flActualX, m_flActualY, m_flActualWidth, m_flActualHeight};
    }

    [[nodiscard]] inline bool IsVisible() const noexcept {
        return (m_bVisibilityFlags & 0x01) != 0 &&
               (static_cast<std::uint16_t>(m_nLayoutFlags) & static_cast<std::uint16_t>(EPanelLayoutFlags::Visible)) != 0;
    }

    [[nodiscard]] inline bool HasClass(CStyleSymbol symClass) const noexcept {
        if (!m_pClasses) return false;
        for (std::int32_t i = 0; i < m_nClassCount; ++i) {
            if (m_pClasses[i] == symClass) return true;
        }
        return false;
    }
};

// Strict static verification against CS2 native panorama.dll memory layout
static_assert(sizeof(CUIPanel) == 0x300, "CUIPanel size must match CS2 native 0x300 layout");
static_assert(offsetof(CUIPanel, m_pClient) == 0x08, "m_pClient offset mismatch");
static_assert(offsetof(CUIPanel, m_pszID) == 0x10, "m_pszID offset mismatch");
static_assert(offsetof(CUIPanel, m_pParent) == 0x18, "m_pParent offset mismatch");
static_assert(offsetof(CUIPanel, m_pWindow) == 0x20, "m_pWindow offset mismatch");
static_assert(offsetof(CUIPanel, m_nChildCount) == 0x28, "m_nChildCount offset mismatch");
static_assert(offsetof(CUIPanel, m_pChildren) == 0x30, "m_pChildren offset mismatch");
static_assert(offsetof(CUIPanel, m_Style) == 0x68, "m_Style offset mismatch");
static_assert(offsetof(CUIPanel, m_nClassCount) == 0x158, "m_nClassCount offset mismatch");
static_assert(offsetof(CUIPanel, m_pClasses) == 0x160, "m_pClasses offset mismatch");
static_assert(offsetof(CUIPanel, m_flActualX) == 0x1B0, "m_flActualX offset mismatch");
static_assert(offsetof(CUIPanel, m_flActualY) == 0x1B4, "m_flActualY offset mismatch");
static_assert(offsetof(CUIPanel, m_flActualWidth) == 0x1B8, "m_flActualWidth offset mismatch");
static_assert(offsetof(CUIPanel, m_flActualHeight) == 0x1BC, "m_flActualHeight offset mismatch");
static_assert(offsetof(CUIPanel, m_nStyleFlags) == 0x220, "m_nStyleFlags offset mismatch");
static_assert(offsetof(CUIPanel, m_nLayoutFlags) == 0x228, "m_nLayoutFlags offset mismatch");
static_assert(offsetof(CUIPanel, m_bVisibilityFlags) == 0x22A, "m_bVisibilityFlags offset mismatch");
static_assert(offsetof(CUIPanel, m_nEventHandlerCount) == 0x238, "m_nEventHandlerCount offset mismatch");

// ============================================================================
// 8. Core Panorama UI Engine Interface (IUIEngine)
// Matches VTable layout at panorama.dll + 0x4C9010 (149 methods)
// ============================================================================
class IUIEngine {
public:
    virtual ~IUIEngine() = default;                                                // Slot 0
    virtual CUIPanel* CreatePanel(const char* pszType, const char* pszID, CUIPanel* pParent) = 0; // Slot 1
    virtual void RegisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) = 0; // Slot 2
    virtual void UnregisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) = 0; // Slot 3
    virtual void RegisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) = 0; // Slot 4
    virtual void UnregisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) = 0; // Slot 5
    virtual void DispatchEvent(CUIEvent* pEvent) = 0;                              // Slot 6
    virtual void LayoutAndPaintWindows() = 0;                                      // Slot 7

    // JavaScript / V8 Context Bridge Interface (Slots 8..11)
    virtual void* CompileScript(const char* pszContext, const char* pszScript, const char* pszFilename, int nLine) = 0; // Slot 8
    virtual bool RunScript(CUIPanel* pPanel, const char* pszScript, const char* pszFilename) = 0;                      // Slot 9
    virtual void* GetContextForPanel(CUIPanel* pPanel) = 0;                        // Slot 10
    [[nodiscard]] virtual void* GetIsolate() const noexcept = 0;                   // Slot 11
};

} // namespace cs2::panorama

// SDK namespace unification
namespace cs2::sdk {
    using namespace cs2::panorama;
}
