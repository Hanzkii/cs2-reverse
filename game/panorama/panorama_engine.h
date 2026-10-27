#pragma once

// ============================================================================
// Source 2 Clean-Room Reconstructed Panorama UI Engine & DOM Subsystem
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
// Zero 3rd-party dependencies - Pure C++20 standard library compliant.
// ============================================================================

#include "../../sdk/panorama.hpp"
#include <vector>
#include <deque>
#include <string_view>
#include <functional>
#include <memory>
#include <string>

namespace cs2::panorama {

/**
 * @brief DOM Node Hierarchy Traversal utilities for Source 2 Panorama UI.
 */
class DOMTreeTraversal {
public:
    // Functional traversal predicates
    using PanelVisitor = std::function<bool(CUIPanel* pPanel)>;
    using ConstPanelVisitor = std::function<bool(const CUIPanel* pPanel)>;

    // Pre-order depth-first traversal (Visitor returns false to prune branch or stop)
    static void TraversePreOrder(CUIPanel* pRoot, const PanelVisitor& visitor) noexcept;
    static void TraversePreOrder(const CUIPanel* pRoot, const ConstPanelVisitor& visitor) noexcept;

    // Post-order depth-first traversal
    static void TraversePostOrder(CUIPanel* pRoot, const PanelVisitor& visitor) noexcept;

    // Breadth-first level-order traversal
    static void TraverseBreadthFirst(CUIPanel* pRoot, const PanelVisitor& visitor);

    // Iterative DOM Search Algorithms
    [[nodiscard]] static CUIPanel* FindChildByID(CUIPanel* pRoot, std::string_view svID) noexcept;
    [[nodiscard]] static const CUIPanel* FindChildByID(const CUIPanel* pRoot, std::string_view svID) noexcept;

    static void FindChildrenByClass(CUIPanel* pRoot, CStyleSymbol symClass, std::vector<CUIPanel*>& outPanels);

    [[nodiscard]] static std::size_t GetDepth(const CUIPanel* pPanel) noexcept;
    [[nodiscard]] static bool IsDescendantOf(const CUIPanel* pPanel, const CUIPanel* pAncestor) noexcept;
};

/**
 * @brief Layout Box Clipping and Screen Coordinate Computation.
 */
class LayoutBoxClipper {
public:
    /**
     * @brief Recursively accumulates absolute screen coordinates for a given panel.
     */
    [[nodiscard]] static UIRect_t ComputeAbsoluteBounds(const CUIPanel* pPanel) noexcept;

    /**
     * @brief Computes effective scissor/clip rectangle taking parent hierarchy into account.
     */
    [[nodiscard]] static UIRect_t ComputeScissorRect(const CUIPanel* pPanel, const UIRect_t& viewport) noexcept;

    /**
     * @brief Performs precise reverse-depth hit-testing against the DOM tree.
     */
    [[nodiscard]] static CUIPanel* HitTest(CUIPanel* pRoot, float flScreenX, float flScreenY, const UIRect_t& viewport) noexcept;
};

/**
 * @brief Reconstructed event object for synthetic event dispatching.
 */
class CConcreteUIEvent final : public CUIEvent {
public:
    CConcreteUIEvent(CUIEventID_t nEvent, PanelHandle_t hTarget, CUIPanel* pSource, bool bCanBubble = true) noexcept;

    [[nodiscard]] CUIEventID_t GetEventType() const noexcept override { return m_nEventType; }
    [[nodiscard]] PanelHandle_t GetTargetPanel() const noexcept override { return m_hTargetPanel; }
    [[nodiscard]] bool CanBubble() const noexcept override { return m_bCanBubble; }

private:
    bool m_bCanBubble{true};
};

/**
 * @brief Thread-safe Event Dispatch Queue with bubbling and capture support.
 */
class EventQueueManager {
public:
    EventQueueManager() = default;
    ~EventQueueManager() = default;

    // Queue an event for deferred dispatching during frame sync
    void EnqueueEvent(std::unique_ptr<CUIEvent> pEvent);

    // Direct synchronous dispatch with bubbling
    EEventDispatchStatus DispatchEventImmediate(CUIEvent* pEvent, CUIPanel* pTargetPanel);

    // Process queued events
    std::size_t ProcessEventQueue(const std::function<CUIPanel*(PanelHandle_t)>& panelResolver);

    // Event Handler Registration
    void RegisterHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler);
    void UnregisterHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler);
    void RegisterUnhandled(CUIEventID_t nEvent, void* pHandler);
    void UnregisterUnhandled(CUIEventID_t nEvent, void* pHandler);

    [[nodiscard]] std::size_t GetPendingEventCount() const noexcept { return m_eventQueue.size(); }

private:
    std::deque<std::unique_ptr<CUIEvent>> m_eventQueue;
    std::vector<CUIEventHandler> m_unhandledHandlers;
};

/**
 * @brief Clean-room C++20 runtime implementation of IUIEngine.
 */
class CReconstructedUIEngine final : public IUIEngine {
public:
    CReconstructedUIEngine();
    ~CReconstructedUIEngine() override;

    // IUIEngine Implementation
    CUIPanel* CreatePanel(const char* pszType, const char* pszID, CUIPanel* pParent) override;
    void RegisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) override;
    void UnregisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) override;
    void RegisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) override;
    void UnregisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) override;
    void DispatchEvent(CUIEvent* pEvent) override;
    void LayoutAndPaintWindows() override;

    void* CompileScript(const char* pszContext, const char* pszScript, const char* pszFilename, int nLine) override;
    bool RunScript(CUIPanel* pPanel, const char* pszScript, const char* pszFilename) override;
    void* GetContextForPanel(CUIPanel* pPanel) override;
    [[nodiscard]] void* GetIsolate() const noexcept override;

    // High-level accessors
    [[nodiscard]] EventQueueManager& GetEventManager() noexcept { return m_eventMgr; }
    [[nodiscard]] const EventQueueManager& GetEventManager() const noexcept { return m_eventMgr; }
    void DestroyPanel(CUIPanel* pPanel);

private:
    EventQueueManager           m_eventMgr;
    std::vector<std::unique_ptr<CUIPanel>> m_allocatedPanels;
    void*                       m_pIsolate{nullptr};
};

} // namespace cs2::panorama
