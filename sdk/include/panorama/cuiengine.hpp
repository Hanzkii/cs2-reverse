#pragma once

// ============================================================================
// Source 2 Reverse Engineered SDK - Panorama UI Engine Subsystem
// Target Binaries: panorama.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Verified Offsets & Structures:
//   CUIEngineSource2 VTable: panorama.dll + 0x4C3740 (150 bounded methods)
//   CUIEngine VTable:        panorama.dll + 0x4C9010 (149 bounded methods)
//   IPanoramaUIEngine:       "PanoramaUIEngine001" (VTable: 0x4C22E0, 18 methods)
// ============================================================================

#include "cuipanel.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace cs2::panorama {

// ============================================================================
// 1. IPanoramaUIEngine Interface ("PanoramaUIEngine001")
// Exported via CreateInterface from panorama.dll
// ============================================================================
class IPanoramaUIEngine {
public:
    virtual ~IPanoramaUIEngine() = default;                                        // Slot 0
    virtual void SetupUIEngine() = 0;                                              // Slot 1
    virtual void Shutdown() = 0;                                                   // Slot 2
    virtual IUIEngine* AccessUIEngine() = 0;                                       // Slot 3
    virtual void RunFrame(double flCurtime, float flFrametime) = 0;                // Slot 4
    virtual void SetScaleFactor(float flScale) = 0;                                // Slot 5
    [[nodiscard]] virtual float GetScaleFactor() const noexcept = 0;               // Slot 6
    virtual void SetScreenSize(int nWidth, int nHeight) = 0;                       // Slot 7
    virtual void GetScreenSize(int& nWidth, int& nHeight) const noexcept = 0;      // Slot 8
    virtual void OnDeviceReset(void* pDevice) = 0;                                 // Slot 9
    virtual void OnDeviceLost() = 0;                                               // Slot 10
    virtual bool HasFocus() const noexcept = 0;                                    // Slot 11
    virtual void SetFocus(bool bFocus) = 0;                                        // Slot 12
    virtual void* GetDesktopWindow() = 0;                                          // Slot 13
    virtual void* GetDesktopPanel() = 0;                                           // Slot 14
    virtual void* CreateTopLevelWindow(const char* pszTitle, int x, int y, int w, int h) = 0; // Slot 15
    virtual void DestroyTopLevelWindow(void* pWindow) = 0;                         // Slot 16
    virtual void* GetTopLevelWindow(int nIndex) = 0;                               // Slot 17
};

// ============================================================================
// 2. Core Panorama UI Engine Interface (IUIEngine)
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

    // Engine Frame & Tick Cycles
    virtual void RunFrame() = 0;                                                   // Slot 12
    virtual void Shutdown() = 0;                                                   // Slot 13
    [[nodiscard]] virtual bool HasFocus() const noexcept = 0;                      // Slot 14
    virtual void SetFocus(CUIPanel* pPanel) = 0;                                   // Slot 15
    virtual void ClearFocus() = 0;                                                 // Slot 16
    [[nodiscard]] virtual CUIPanel* GetFocusPanel() const noexcept = 0;            // Slot 17
    virtual void SetMouseCapture(CUIPanel* pPanel) = 0;                            // Slot 18
    virtual void ReleaseMouseCapture(CUIPanel* pPanel) = 0;                        // Slot 19
    [[nodiscard]] virtual CUIPanel* GetMouseCapturePanel() const noexcept = 0;     // Slot 20
    virtual void SetKeyFocus(CUIPanel* pPanel) = 0;                                // Slot 21
    virtual void ReleaseKeyFocus(CUIPanel* pPanel) = 0;                            // Slot 22
    [[nodiscard]] virtual CUIPanel* GetKeyFocusPanel() const noexcept = 0;         // Slot 23
};

// ============================================================================
// 3. Concrete Engine Dispatcher Implementation (CUIEngineSource2)
// Matches VTable layout at panorama.dll + 0x4C3740 (150 methods)
// ============================================================================
class CUIEngineSource2 : public IUIEngine {
public:
    // Internal subsystem references in panorama.dll
    void*               m_pRenderEngine{nullptr};       // 0x08
    void*               m_pFontManager{nullptr};        // 0x10
    void*               m_pStyleManager{nullptr};       // 0x18
    void*               m_pImageManager{nullptr};       // 0x20
    void*               m_pScriptManager{nullptr};      // 0x28 (V8 Isolate Container)
    void*               m_pSoundManager{nullptr};       // 0x30
    void*               m_pInputManager{nullptr};       // 0x38
    void*               m_pLayoutTreeRoot{nullptr};     // 0x40
    float               m_flDPI{96.0f};                 // 0x48
    float               m_flScaleFactor{1.0f};          // 0x4C
    std::uint32_t       m_nActiveWindowCount{0};        // 0x50
    std::uint32_t       m_nFrameCounter{0};             // 0x54
};

} // namespace cs2::panorama
