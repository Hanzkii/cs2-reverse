// ============================================================================
// Source 2 Clean-Room Reconstructed Panorama UI Engine & DOM Subsystem
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
// Zero 3rd-party dependencies - Strictly MSVC x64 C++20 compliant.
// ============================================================================

#include "panorama_engine.h"
#include <cstring>
#include <queue>
#include <algorithm>

namespace cs2::panorama {

// ============================================================================
// DOMTreeTraversal Implementation
// ============================================================================

void DOMTreeTraversal::TraversePreOrder(CUIPanel* pRoot, const PanelVisitor& visitor) noexcept {
    if (!pRoot || !visitor) return;
    if (!visitor(pRoot)) return; // Visitor returned false: prune this branch

    for (int i = 0; i < pRoot->m_nChildCount; ++i) {
        if (pRoot->m_pChildren && pRoot->m_pChildren[i]) {
            TraversePreOrder(pRoot->m_pChildren[i], visitor);
        }
    }
}

void DOMTreeTraversal::TraversePreOrder(const CUIPanel* pRoot, const ConstPanelVisitor& visitor) noexcept {
    if (!pRoot || !visitor) return;
    if (!visitor(pRoot)) return;

    for (int i = 0; i < pRoot->m_nChildCount; ++i) {
        if (pRoot->m_pChildren && pRoot->m_pChildren[i]) {
            TraversePreOrder(static_cast<const CUIPanel*>(pRoot->m_pChildren[i]), visitor);
        }
    }
}

void DOMTreeTraversal::TraversePostOrder(CUIPanel* pRoot, const PanelVisitor& visitor) noexcept {
    if (!pRoot || !visitor) return;

    for (int i = 0; i < pRoot->m_nChildCount; ++i) {
        if (pRoot->m_pChildren && pRoot->m_pChildren[i]) {
            TraversePostOrder(pRoot->m_pChildren[i], visitor);
        }
    }

    visitor(pRoot);
}

void DOMTreeTraversal::TraverseBreadthFirst(CUIPanel* pRoot, const PanelVisitor& visitor) {
    if (!pRoot || !visitor) return;

    std::queue<CUIPanel*> queue;
    queue.push(pRoot);

    while (!queue.empty()) {
        CUIPanel* pCurrent = queue.front();
        queue.pop();

        if (!visitor(pCurrent)) continue;

        for (int i = 0; i < pCurrent->m_nChildCount; ++i) {
            if (pCurrent->m_pChildren && pCurrent->m_pChildren[i]) {
                queue.push(pCurrent->m_pChildren[i]);
            }
        }
    }
}

CUIPanel* DOMTreeTraversal::FindChildByID(CUIPanel* pRoot, std::string_view svID) noexcept {
    if (!pRoot || svID.empty()) return nullptr;

    CUIPanel* pFound = nullptr;
    TraversePreOrder(pRoot, [&pFound, svID](CUIPanel* pPanel) noexcept -> bool {
        if (pPanel->m_pszID && svID == pPanel->m_pszID) {
            pFound = pPanel;
            return false; // Found, terminate search
        }
        return true;
    });

    return pFound;
}

const CUIPanel* DOMTreeTraversal::FindChildByID(const CUIPanel* pRoot, std::string_view svID) noexcept {
    if (!pRoot || svID.empty()) return nullptr;

    const CUIPanel* pFound = nullptr;
    TraversePreOrder(pRoot, [&pFound, svID](const CUIPanel* pPanel) noexcept -> bool {
        if (pPanel->m_pszID && svID == pPanel->m_pszID) {
            pFound = pPanel;
            return false;
        }
        return true;
    });

    return pFound;
}

void DOMTreeTraversal::FindChildrenByClass(CUIPanel* pRoot, CStyleSymbol symClass, std::vector<CUIPanel*>& outPanels) {
    if (!pRoot) return;

    TraversePreOrder(pRoot, [symClass, &outPanels](CUIPanel* pPanel) -> bool {
        if (pPanel->HasClass(symClass)) {
            outPanels.push_back(pPanel);
        }
        return true;
    });
}

std::size_t DOMTreeTraversal::GetDepth(const CUIPanel* pPanel) noexcept {
    std::size_t depth = 0;
    while (pPanel && pPanel->m_pParent) {
        ++depth;
        pPanel = pPanel->m_pParent;
    }
    return depth;
}

bool DOMTreeTraversal::IsDescendantOf(const CUIPanel* pPanel, const CUIPanel* pAncestor) noexcept {
    if (!pPanel || !pAncestor) return false;

    const CUIPanel* pCur = pPanel->m_pParent;
    while (pCur) {
        if (pCur == pAncestor) return true;
        pCur = pCur->m_pParent;
    }
    return false;
}

// ============================================================================
// LayoutBoxClipper Implementation
// ============================================================================

UIRect_t LayoutBoxClipper::ComputeAbsoluteBounds(const CUIPanel* pPanel) noexcept {
    if (!pPanel) return {};

    float flAbsX = pPanel->m_flActualX + pPanel->m_flPositionX;
    float flAbsY = pPanel->m_flActualY + pPanel->m_flPositionY;

    const CUIPanel* pCur = pPanel->m_pParent;
    while (pCur) {
        flAbsX += (pCur->m_flActualX + pCur->m_flPositionX);
        flAbsY += (pCur->m_flActualY + pCur->m_flPositionY);
        pCur = pCur->m_pParent;
    }

    return {flAbsX, flAbsY, pPanel->m_flActualWidth, pPanel->m_flActualHeight};
}

UIRect_t LayoutBoxClipper::ComputeScissorRect(const CUIPanel* pPanel, const UIRect_t& viewport) noexcept {
    if (!pPanel) return viewport;

    UIRect_t currentScissor = viewport;

    // Traverse upwards from root to leaf to build intersection hierarchy
    std::vector<const CUIPanel*> ancestors;
    for (const CUIPanel* p = pPanel; p != nullptr; p = p->m_pParent) {
        ancestors.push_back(p);
    }

    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        const CUIPanel* pNode = *it;
        const auto layoutFlags = static_cast<std::uint16_t>(pNode->m_nLayoutFlags);
        if (layoutFlags & static_cast<std::uint16_t>(EPanelLayoutFlags::ClipToBounds)) {
            const UIRect_t nodeBounds = ComputeAbsoluteBounds(pNode);
            currentScissor = nodeBounds.ClipTo(currentScissor);
        }
    }

    return currentScissor;
}

CUIPanel* LayoutBoxClipper::HitTest(CUIPanel* pRoot, float flScreenX, float flScreenY, const UIRect_t& viewport) noexcept {
    if (!pRoot || !pRoot->IsVisible()) return nullptr;

    const UIRect_t bounds = ComputeAbsoluteBounds(pRoot);
    const UIRect_t scissor = ComputeScissorRect(pRoot, viewport);

    if (!bounds.Contains(flScreenX, flScreenY) || !scissor.Contains(flScreenX, flScreenY)) {
        return nullptr;
    }

    // Hit-test children in reverse painting order (topmost child first)
    for (int i = pRoot->m_nChildCount - 1; i >= 0; --i) {
        if (pRoot->m_pChildren && pRoot->m_pChildren[i]) {
            CUIPanel* pHit = HitTest(pRoot->m_pChildren[i], flScreenX, flScreenY, viewport);
            if (pHit) return pHit;
        }
    }

    // Check if the current panel accepts input
    const auto layoutFlags = static_cast<std::uint16_t>(pRoot->m_nLayoutFlags);
    if (layoutFlags & static_cast<std::uint16_t>(EPanelLayoutFlags::AcceptsInput)) {
        return pRoot;
    }

    return nullptr;
}

// ============================================================================
// CConcreteUIEvent Implementation
// ============================================================================

CConcreteUIEvent::CConcreteUIEvent(CUIEventID_t nEvent, PanelHandle_t hTarget, CUIPanel* pSource, bool bCanBubble) noexcept
    : m_bCanBubble(bCanBubble) {
    m_nEventType = nEvent;
    m_hTargetPanel = hTarget;
    m_pSourcePanel = pSource;
}

// ============================================================================
// EventQueueManager Implementation
// ============================================================================

void EventQueueManager::EnqueueEvent(std::unique_ptr<CUIEvent> pEvent) {
    if (pEvent) {
        m_eventQueue.push_back(std::move(pEvent));
    }
}

EEventDispatchStatus EventQueueManager::DispatchEventImmediate(CUIEvent* pEvent, CUIPanel* pTargetPanel) {
    if (!pEvent) return EEventDispatchStatus::Continue;

    CUIPanel* pCurrent = pTargetPanel;

    while (pCurrent) {
        // 1. Dispatch to panel client if registered
        if (pCurrent->m_pClient) {
            if (pCurrent->m_pClient->OnEvent(pCurrent, pEvent)) {
                return EEventDispatchStatus::Handled;
            }
        }

        // 2. Dispatch to registered event handlers on this panel
        for (std::int32_t i = 0; i < pCurrent->m_nEventHandlerCount; ++i) {
            const CUIEventHandler& handler = pCurrent->m_pEventHandlers[i];
            if (handler.nEventType == pEvent->GetEventType()) {
                using EventHandlerFn = bool(*)(void*, CUIEvent*);
                if (handler.pFunction) {
                    auto fn = reinterpret_cast<EventHandlerFn>(handler.pFunction);
                    if (fn(handler.pContext, pEvent)) {
                        return EEventDispatchStatus::Handled;
                    }
                }
            }
        }

        // 3. Check event bubbling capability
        if (!pEvent->CanBubble()) {
            break;
        }

        pCurrent = pCurrent->m_pParent;
    }

    // 4. Dispatch to global unhandled event handlers
    for (const auto& unhandled : m_unhandledHandlers) {
        if (unhandled.nEventType == pEvent->GetEventType()) {
            using EventHandlerFn = bool(*)(void*, CUIEvent*);
            if (unhandled.pFunction) {
                auto fn = reinterpret_cast<EventHandlerFn>(unhandled.pFunction);
                if (fn(unhandled.pContext, pEvent)) {
                    return EEventDispatchStatus::Handled;
                }
            }
        }
    }

    return EEventDispatchStatus::Continue;
}

std::size_t EventQueueManager::ProcessEventQueue(const std::function<CUIPanel*(PanelHandle_t)>& panelResolver) {
    std::size_t processed = 0;

    while (!m_eventQueue.empty()) {
        std::unique_ptr<CUIEvent> pEvent = std::move(m_eventQueue.front());
        m_eventQueue.pop_front();

        CUIPanel* pTarget = nullptr;
        if (panelResolver) {
            pTarget = panelResolver(pEvent->GetTargetPanel());
        }

        DispatchEventImmediate(pEvent.get(), pTarget);
        ++processed;
    }

    return processed;
}

void EventQueueManager::RegisterHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) {
    if (!pTarget || !pHandler) return;

    // Check if re-allocation needed for m_pEventHandlers
    if (pTarget->m_nEventHandlerCount >= pTarget->m_nEventHandlerAlloc) {
        const std::int32_t nNewAlloc = pTarget->m_nEventHandlerAlloc ? (pTarget->m_nEventHandlerAlloc * 2) : 4;
        auto* pNewMemory = new CUIEventHandler[static_cast<std::size_t>(nNewAlloc)];
        if (pTarget->m_pEventHandlers) {
            std::memcpy(pNewMemory, pTarget->m_pEventHandlers, sizeof(CUIEventHandler) * static_cast<std::size_t>(pTarget->m_nEventHandlerCount));
            delete[] pTarget->m_pEventHandlers;
        }
        pTarget->m_pEventHandlers = pNewMemory;
        pTarget->m_nEventHandlerAlloc = nNewAlloc;
    }

    pTarget->m_pEventHandlers[pTarget->m_nEventHandlerCount++] = {
        pHandler,
        pTarget,
        nEvent,
        0
    };
}

void EventQueueManager::UnregisterHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) {
    if (!pTarget || !pTarget->m_pEventHandlers) return;

    for (std::int32_t i = 0; i < pTarget->m_nEventHandlerCount; ++i) {
        if (pTarget->m_pEventHandlers[i].nEventType == nEvent &&
            pTarget->m_pEventHandlers[i].pFunction == pHandler) {
            // Shift elements left
            for (std::int32_t j = i; j < (pTarget->m_nEventHandlerCount - 1); ++j) {
                pTarget->m_pEventHandlers[j] = pTarget->m_pEventHandlers[j + 1];
            }
            --pTarget->m_nEventHandlerCount;
            break;
        }
    }
}

void EventQueueManager::RegisterUnhandled(CUIEventID_t nEvent, void* pHandler) {
    if (!pHandler) return;
    m_unhandledHandlers.push_back({pHandler, nullptr, nEvent, 0});
}

void EventQueueManager::UnregisterUnhandled(CUIEventID_t nEvent, void* pHandler) {
    std::erase_if(m_unhandledHandlers, [nEvent, pHandler](const CUIEventHandler& h) {
        return h.nEventType == nEvent && h.pFunction == pHandler;
    });
}

// ============================================================================
// CReconstructedUIEngine Implementation
// ============================================================================

CReconstructedUIEngine::CReconstructedUIEngine() {
    // Zero-initialize members
}

CReconstructedUIEngine::~CReconstructedUIEngine() {
    m_allocatedPanels.clear();
}

CUIPanel* CReconstructedUIEngine::CreatePanel(const char* pszType, const char* pszID, CUIPanel* pParent) {
    (void)pszType;

    auto panel = std::make_unique<CUIPanel>();
    CUIPanel* rawPtr = panel.get();

    // Default initialization
    rawPtr->m_pszID = pszID;
    rawPtr->m_pParent = pParent;
    rawPtr->m_nLayoutFlags = EPanelLayoutFlags::Visible | EPanelLayoutFlags::AcceptsInput;
    rawPtr->m_bVisibilityFlags = 0x01; // Visible
    rawPtr->m_flActualWidth = 100.0f;
    rawPtr->m_flActualHeight = 100.0f;

    // Attach to parent DOM list if valid
    if (pParent) {
        if (pParent->m_nChildCount >= pParent->m_nChildAllocationCount) {
            const std::int32_t nNewAlloc = pParent->m_nChildAllocationCount ? (pParent->m_nChildAllocationCount * 2) : 4;
            auto** ppNewMemory = new CUIPanel*[static_cast<std::size_t>(nNewAlloc)];
            if (pParent->m_pChildren) {
                std::memcpy(ppNewMemory, pParent->m_pChildren, sizeof(CUIPanel*) * static_cast<std::size_t>(pParent->m_nChildCount));
                delete[] pParent->m_pChildren;
            }
            pParent->m_pChildren = ppNewMemory;
            pParent->m_nChildAllocationCount = nNewAlloc;
        }
        pParent->m_pChildren[pParent->m_nChildCount++] = rawPtr;
    }

    m_allocatedPanels.push_back(std::move(panel));
    return rawPtr;
}

void CReconstructedUIEngine::DestroyPanel(CUIPanel* pPanel) {
    if (!pPanel) return;

    // Detach from parent
    if (pPanel->m_pParent) {
        CUIPanel* pParent = pPanel->m_pParent;
        for (int i = 0; i < pParent->m_nChildCount; ++i) {
            if (pParent->m_pChildren[i] == pPanel) {
                for (int j = i; j < (pParent->m_nChildCount - 1); ++j) {
                    pParent->m_pChildren[j] = pParent->m_pChildren[j + 1];
                }
                --pParent->m_nChildCount;
                break;
            }
        }
        pPanel->m_pParent = nullptr;
    }

    // Clean up allocated children pointers
    if (pPanel->m_pChildren) {
        delete[] pPanel->m_pChildren;
        pPanel->m_pChildren = nullptr;
        pPanel->m_nChildCount = 0;
        pPanel->m_nChildAllocationCount = 0;
    }

    // Clean up event handlers
    if (pPanel->m_pEventHandlers) {
        delete[] pPanel->m_pEventHandlers;
        pPanel->m_pEventHandlers = nullptr;
        pPanel->m_nEventHandlerCount = 0;
        pPanel->m_nEventHandlerAlloc = 0;
    }

    // Remove from ownership list
    std::erase_if(m_allocatedPanels, [pPanel](const std::unique_ptr<CUIPanel>& ptr) {
        return ptr.get() == pPanel;
    });
}

void CReconstructedUIEngine::RegisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) {
    m_eventMgr.RegisterHandler(nEvent, pTarget, pHandler);
}

void CReconstructedUIEngine::UnregisterEventHandler(CUIEventID_t nEvent, CUIPanel* pTarget, void* pHandler) {
    m_eventMgr.UnregisterHandler(nEvent, pTarget, pHandler);
}

void CReconstructedUIEngine::RegisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) {
    m_eventMgr.RegisterUnhandled(nEvent, pHandler);
}

void CReconstructedUIEngine::UnregisterForUnhandledEvent(CUIEventID_t nEvent, void* pHandler) {
    m_eventMgr.UnregisterUnhandled(nEvent, pHandler);
}

void CReconstructedUIEngine::DispatchEvent(CUIEvent* pEvent) {
    if (!pEvent) return;
    CUIPanel* pTarget = pEvent->m_pSourcePanel;
    m_eventMgr.DispatchEventImmediate(pEvent, pTarget);
}

void CReconstructedUIEngine::LayoutAndPaintWindows() {
    // Process queued deferred events
    m_eventMgr.ProcessEventQueue([this](PanelHandle_t hTarget) -> CUIPanel* {
        for (const auto& pPanel : m_allocatedPanels) {
            if (reinterpret_cast<PanelHandle_t>(pPanel.get()) == hTarget) {
                return pPanel.get();
            }
        }
        return nullptr;
    });
}

void* CReconstructedUIEngine::CompileScript(const char* pszContext, const char* pszScript, const char* pszFilename, int nLine) {
    (void)pszContext;
    (void)pszScript;
    (void)pszFilename;
    (void)nLine;
    return nullptr;
}

bool CReconstructedUIEngine::RunScript(CUIPanel* pPanel, const char* pszScript, const char* pszFilename) {
    (void)pPanel;
    (void)pszScript;
    (void)pszFilename;
    return true;
}

void* CReconstructedUIEngine::GetContextForPanel(CUIPanel* pPanel) {
    (void)pPanel;
    return nullptr;
}

void* CReconstructedUIEngine::GetIsolate() const noexcept {
    return m_pIsolate;
}

} // namespace cs2::panorama
