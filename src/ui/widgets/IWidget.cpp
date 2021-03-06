#include "Vorb/stdafx.h"
#include "Vorb/ui/widgets/IWidget.h"
#include "Vorb/ui/InputDispatcher.h"
#include "Vorb/ui/widgets/Widget.h"
#include "Vorb/ui/UIRenderer.h"
#include "Vorb/ui/widgets/Viewport.h"
#include "Vorb/utils.h"

vui::IWidget::IWidget() :
    m_viewport(nullptr),
    m_parent(nullptr),
    m_widgets(IWidgets()),
    m_position(f32v2(0.0f)),
    m_size(f32v2(0.0f)),
    m_padding(f32v4(0.0f)),
    m_clipping(DEFAULT_CLIPPING),
    m_clipRect(DEFAULT_CLIP_RECT),
    m_zIndex(0),
    m_dock({ DockState::NONE, 0.0f }),
    m_name(""),
    m_childOffset(f32v2(0.0f)),
    m_flags({ false, false, false, false, false, false, false, false, false }) {
    // Empty
}

vui::IWidget::IWidget(const IWidget& widget) :
    m_viewport(nullptr),
    m_parent(nullptr),
    m_widgets(IWidgets()),
    m_position(widget.m_position),
    m_size(widget.m_size),
    m_padding(widget.m_padding),
    m_clipping(widget.m_clipping),
    m_clipRect(widget.m_clipRect),
    m_zIndex(widget.m_zIndex),
    m_dock(widget.m_dock),
    m_name(widget.m_name),
    m_childOffset(widget.m_childOffset),
    m_flags({ false, false, false, false, false, false, false, false, false }) {
    // Empty
}

vui::IWidget::~IWidget() {
    // Empty
}

void vui::IWidget::init(const nString& name, const f32v4& dimensions /*= f32v4(0.0f)*/, ui16 zIndex /*= 0*/) {
    m_name     = name;
    m_position = f32v2(dimensions.x, dimensions.y);
    m_size     = f32v2(dimensions.z, dimensions.w);
    m_zIndex   = zIndex;

    MouseClick.setSender(this);
    MouseDown.setSender(this);
    MouseUp.setSender(this);
    MouseEnter.setSender(this);
    MouseLeave.setSender(this);
    MouseMove.setSender(this);

    initBase();
}

void vui::IWidget::init(IWidget* parent, const nString& name, const f32v4& dimensions /*= f32v4(0.0f)*/, ui16 zIndex /*= 0*/) {
    IWidget::init(name, dimensions, zIndex);

    parent->addWidget(this);
}

void vui::IWidget::dispose() {
    for (auto& w : m_widgets) {
        w->dispose();
    }
    IWidgets().swap(m_widgets);

//cant call this as it will invalidate the m_widgets vector of parent and crash
//    if (m_parent) {
//        m_parent->removeWidget(this);
//    }

    disable();
}

void vui::IWidget::update(f32 dt /*= 0.0f*/) {
    if (m_flags.needsZIndexReorder) {
        m_flags.needsZIndexReorder = false;
        reorderWidgets();
    }

    if (m_flags.needsDimensionUpdate) {
        m_flags.needsDimensionUpdate = false;
        updateDimensions(dt);
        if (!m_flags.ignoreOffset) {
            applyOffset();
        }
    }

    if (m_flags.needsDockRecalculation) {
        m_flags.needsDockRecalculation = false;
        calculateDockedWidgets();
    }

    if (m_flags.needsClipRectRecalculation) {
        m_flags.needsClipRectRecalculation = false;
        calculateClipRect();
    }

    if (m_flags.needsDrawableRecalculation) {
        m_flags.needsDrawableRecalculation = false;
        calculateDrawables();
    }
}

void vui::IWidget::enable() {
    if (!m_flags.isEnabled) {
        m_flags.isEnabled = true;
        vui::InputDispatcher::mouse.onButtonDown += makeDelegate(this, &IWidget::onMouseDown);
        vui::InputDispatcher::mouse.onButtonUp   += makeDelegate(this, &IWidget::onMouseUp);
        vui::InputDispatcher::mouse.onMotion     += makeDelegate(this, &IWidget::onMouseMove);
        vui::InputDispatcher::mouse.onFocusLost  += makeDelegate(this, &IWidget::onMouseFocusLost);
    }

    // Enable all children
    for (auto& w : m_widgets) w->enable();
}

void vui::IWidget::disable() {
    if (m_flags.isEnabled) {
        m_flags.isEnabled = false;
        vui::InputDispatcher::mouse.onButtonDown -= makeDelegate(this, &IWidget::onMouseDown);
        vui::InputDispatcher::mouse.onButtonUp   -= makeDelegate(this, &IWidget::onMouseUp);
        vui::InputDispatcher::mouse.onMotion     -= makeDelegate(this, &IWidget::onMouseMove);
        vui::InputDispatcher::mouse.onFocusLost  -= makeDelegate(this, &IWidget::onMouseFocusLost);
    }
    m_flags.isClicking = false;

    // Disable all children
    for (auto& w : m_widgets) w->disable();
}

bool vui::IWidget::addWidget(IWidget* child) {
    if (child->m_parent) return false;

    m_widgets.push_back(child);

    child->m_parent   = this;
    child->m_viewport = m_viewport;

    child->updateDescendantViewports();

    // Update child appropriately - no more, no less, than we must.
    if (child->isEnabled()) {
        WidgetFlags oldFlags = child->getFlags();
        child->setFlags({
            oldFlags.isClicking,
            oldFlags.isEnabled,
            oldFlags.isMouseIn,
            oldFlags.ignoreOffset,
            true,  // needsDimensionUpdate
            false, // needsZIndexReorder
            true,  // needsDockRecalculation
            true,  // needsClipRectRecalculation
            false  // needsDrawableRecalculation
        });

        child->update(0.0f);

        WidgetFlags newFlags = child->getFlags();
        child->setFlags({
            newFlags.isClicking,
            newFlags.isEnabled,
            newFlags.isMouseIn,
            newFlags.ignoreOffset,
            oldFlags.needsDimensionUpdate       || newFlags.needsDimensionUpdate,
            oldFlags.needsZIndexReorder         || newFlags.needsZIndexReorder,
            oldFlags.needsDockRecalculation     || newFlags.needsDockRecalculation,
            oldFlags.needsClipRectRecalculation || newFlags.needsClipRectRecalculation,
            oldFlags.needsDrawableRecalculation || newFlags.needsDrawableRecalculation
        });
    }

    m_flags.needsZIndexReorder     = true;
    m_flags.needsDockRecalculation = true;

    return true;
}

bool vui::IWidget::removeWidget(IWidget* child) {
    for (auto it = m_widgets.begin(); it != m_widgets.end(); it++) {
        if (*it == child) {
            m_widgets.erase(it);

            child->m_parent   = nullptr;
            child->m_viewport = nullptr;

            child->updateDescendantViewports();

            m_clipRect = DEFAULT_CLIP_RECT;

            child->m_flags.needsDimensionUpdate       = true;
            child->m_flags.needsDockRecalculation     = true;
            child->m_flags.needsClipRectRecalculation = true;

            m_flags.needsDockRecalculation = true;

            return true;
        }
    }
    return false;
}

bool vui::IWidget::isInBounds(f32 x, f32 y) const {
    f32v2 pos  = getPaddedPosition();
    f32v2 size = getPaddedSize();

    return (x >= pos.x && x < pos.x + size.x &&
            y >= pos.y && y < pos.y + size.y);
}

vui::ClippingState vui::IWidget::getClippingLeft() const {
    return (m_clipping.left == ClippingState::INHERIT ? m_parent->getClippingLeft() : m_clipping.left);
}

vui::ClippingState vui::IWidget::getClippingTop() const {
    return (m_clipping.top == ClippingState::INHERIT ? m_parent->getClippingTop() : m_clipping.top);
}

vui::ClippingState vui::IWidget::getClippingRight() const {
    return (m_clipping.right == ClippingState::INHERIT ? m_parent->getClippingRight() : m_clipping.right);
}

vui::ClippingState vui::IWidget::getClippingBottom() const {
    return (m_clipping.bottom == ClippingState::INHERIT ? m_parent->getClippingBottom() : m_clipping.bottom);
}

void vui::IWidget::setPosition(f32v2 position) {
    f32v2 tmp = m_position;

    m_position = position;

    if (tmp != m_position) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setX(f32 x) {
    f32 tmp = m_position.x;

    m_position.x = x;

    if (tmp != m_position.x) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setY(f32 y) {
    f32 tmp = m_position.y;

    m_position.y = y;

    if (tmp != m_position.y) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setRelativePosition(f32v2 relativePosition) {
    f32v2 tmp = m_position;

    m_position = relativePosition + m_parent->getPosition();

    if (tmp != m_position) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setRelativeX(f32 relativeX) {
    f32 tmp = m_position.x;

    m_position.x = relativeX + m_parent->getPosition().x;

    if (tmp != m_position.x) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setRelativeY(f32 relativeY) {
    f32 tmp = m_position.y;

    m_position.y = relativeY + m_parent->getPosition().y;

    if (tmp != m_position.y) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setSize(f32v2 size) {
    f32v2 tmp = m_size;

    m_size = size;

    if (tmp != m_position) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setWidth(f32 width) {
    f32 tmp = m_size.x;

    m_size.x = width;

    if (tmp != m_size.x) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setHeight(f32 height) {
    f32 tmp = m_size.y;

    m_size.y = height;

    if (tmp != m_size.y) {
        markChildrenToUpdateDimensions();

        m_flags.needsDockRecalculation     = true;
        m_flags.needsClipRectRecalculation = true;
        m_flags.needsDrawableRecalculation = true;
    }
}

void vui::IWidget::setPadding(const f32v4& padding) {
    f32v4 tmp = m_padding;

    m_padding = padding;

    if (tmp != m_padding) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setPaddingLeft(f32 left) {
    f32 tmp = m_padding.x;

    m_padding.x = left;

    if (tmp != m_padding.x) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setPaddingTop(f32 top) {
    f32 tmp = m_padding.y;

    m_padding.y = top;

    if (tmp != m_padding.y) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setPaddingRight(f32 right) {
    f32 tmp = m_padding.z;

    m_padding.z = right;

    if (tmp != m_padding.z) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setPaddingBottom(f32 bottom) {
    f32 tmp = m_padding.w;

    m_padding.w = bottom;

    if (tmp != m_padding.w) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setClipping(Clipping clipping) {
    Clipping tmp = m_clipping;

    m_clipping = clipping;

    if (tmp != m_clipping) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setClippingLeft(ClippingState state) {
    ClippingState tmp = m_clipping.left;

    m_clipping.left = state;

    if (tmp != m_clipping.left) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setClippingTop(ClippingState state) {
    ClippingState tmp = m_clipping.top;

    m_clipping.top = state;

    if (tmp != m_clipping.top) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setClippingRight(ClippingState state) {
    ClippingState tmp = m_clipping.right;

    m_clipping.right = state;

    if (tmp != m_clipping.right) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setClippingBottom(ClippingState state) {
    ClippingState tmp = m_clipping.bottom;

    m_clipping.bottom = state;

    if (tmp != m_clipping.bottom) {
        m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::setZIndex(ZIndex zIndex) {
    ZIndex tmp = m_zIndex;

    m_zIndex = zIndex;

    if (tmp != m_zIndex) {
        if (m_parent) m_parent->setNeedsZIndexReorder(true);
    }
}

void vui::IWidget::setDock(Dock dock) {
    m_dock = dock;

    if (m_parent) {
        m_parent->m_flags.needsDockRecalculation   = true;
    } else if (m_viewport && m_viewport != this) {
        m_viewport->m_flags.needsDockRecalculation = true;
    }
}

void vui::IWidget::setDockState(DockState state) {
    m_dock.state = state;

    if (m_parent) {
        m_parent->m_flags.needsDockRecalculation   = true;
    } else if (m_viewport && m_viewport != this) {
        m_viewport->m_flags.needsDockRecalculation = true;
    }
}

void vui::IWidget::setDockSize(f32 size) {
    m_dock.size = size;

    if (m_parent) {
        m_parent->m_flags.needsDockRecalculation   = true;
    } else if (m_viewport && m_viewport != this) {
        m_viewport->m_flags.needsDockRecalculation = true;
    }
}

void vui::IWidget::setChildOffset(const f32v2& offset) {
    m_childOffset = offset;

    for (auto& child : m_widgets) {
        child->m_flags.needsDimensionUpdate = true;
    }
}

void vui::IWidget::setChildOffsetX(f32 offset) {
    setChildOffset(f32v2(offset, getChildOffset().y));
}

void vui::IWidget::setChildOffsetY(f32 offset) {
    setChildOffset(f32v2(getChildOffset().x, offset));
}

void vui::IWidget::setIgnoreOffset(bool ignoreOffset) {
    m_flags.ignoreOffset = ignoreOffset;

    m_flags.needsDimensionUpdate = true;
}

void vui::IWidget::updateDescendants(f32 dt) {
    for (auto& child : m_widgets) {
        if (!child->isEnabled()) continue;
        child->update(dt);
        child->updateDescendants(dt);
    }
}

void vui::IWidget::addDescendantDrawables(UIRenderer& renderer) {
    for (auto& child : m_widgets) {
        if (!child->isEnabled()) continue;
        child->addDrawables(renderer);
        child->addDescendantDrawables(renderer);
    }
}

void vui::IWidget::updateDescendantViewports() {
    for (auto& child : m_widgets) {
        child->m_viewport = m_viewport;
        child->updateDescendantViewports();
    }
}

void vui::IWidget::markChildrenToUpdateDimensions() {
    for (auto& child : m_widgets) {
        child->m_flags.needsDimensionUpdate = true;
    }
}

void vui::IWidget::markDescendantsToUpdateDimensions() {
    for (auto& child : m_widgets) {
        child->m_flags.needsDimensionUpdate = true;
        child->markDescendantsToUpdateDimensions();
    }
}

void vui::IWidget::calculateDockedWidgets() {
    f32 surplusWidth  = m_size.x;
    f32 surplusHeight = m_size.y;
    f32 leftFill      = 0.0f;
    f32 topFill       = 0.0f;

    f32 oldLeftFill, oldTopFill;

    for (auto& child : m_widgets) {
        if (child->getDockState() == DockState::NONE) {
            continue;
        } else if (surplusWidth == 0.0f && surplusHeight == 0.0f) {
            child->IWidget::setSize(f32v2(0.0f, 0.0f));
            continue;
        }

        DockState state = child->getDockState();
        f32       size  = child->getDockSize();
        switch (state) {
            case DockState::LEFT:
                oldLeftFill = leftFill;
                if (surplusWidth > size) {
                    child->IWidget::setWidth(size);
                    leftFill     += size;
                    surplusWidth -= size;
                } else {
                    child->IWidget::setWidth(surplusWidth);
                    leftFill    += surplusWidth;
                    surplusWidth = 0.0f;
                }

                child->IWidget::setPosition(f32v2(oldLeftFill, topFill));

                child->IWidget::setHeight(surplusHeight);
                break;
            case DockState::TOP:
                oldTopFill = topFill;
                if (surplusHeight > size) {
                    child->IWidget::setHeight(size);
                    topFill       += size;
                    surplusHeight -= size;
                } else {
                    child->IWidget::setHeight(surplusHeight);
                    topFill      += surplusHeight;
                    surplusHeight = 0.0f;
                }

                child->IWidget::setPosition(f32v2(leftFill, oldTopFill));

                child->IWidget::setWidth(surplusWidth);
                break;
            case DockState::RIGHT:
                if (surplusWidth > size) {
                    child->IWidget::setWidth(size);
                    surplusWidth -= size;

                    child->IWidget::setPosition(f32v2(leftFill + surplusWidth, topFill));
                } else {
                    child->IWidget::setWidth(surplusWidth);
                    surplusWidth = 0.0f;

                    child->IWidget::setPosition(f32v2(leftFill, topFill));
                }

                child->IWidget::setHeight(surplusHeight);
                break;
            case DockState::BOTTOM:
                if (surplusHeight > size) {
                    child->IWidget::setHeight(size);
                    surplusHeight -= size;

                    child->IWidget::setPosition(f32v2(leftFill, topFill + surplusHeight));
                } else {
                    child->IWidget::setHeight(surplusHeight);
                    surplusHeight = 0.0f;

                    child->IWidget::setPosition(f32v2(leftFill, topFill));
                }

                child->IWidget::setWidth(surplusWidth);
                break;
            case DockState::FILL:
                child->IWidget::setSize(f32v2(surplusWidth, surplusHeight));
                child->IWidget::setPosition(f32v2(leftFill, topFill));
                surplusWidth  = 0.0f;
                surplusHeight = 0.0f;
                break;
            default:
                // Shouldn't get here.
                assert(false);
                break;
        }
    }
}

void vui::IWidget::calculateClipRect() {
    f32v4 oldClipRect = m_clipRect;

    f32v4 parentClipRect = DEFAULT_CLIP_RECT;
    if (m_parent) {
        parentClipRect = m_parent->getClipRect();
    } else if (m_viewport && m_viewport != this) {
        parentClipRect = m_viewport->getClipRect();
    }

    f32 leftMax = m_position.x - m_padding.x;
    ClippingState left = getClippingLeft();
    if (left == ClippingState::VISIBLE
            || leftMax < parentClipRect.x) {
        // This widget's clip rect is the same as its parent's if overflow is enabled or if the widget overflows the parent's clip rect.
        m_clipRect.x = parentClipRect.x;
    } else if (left == ClippingState::HIDDEN) {
        // Otherwise, the clip rect is the same as the dimensions.
        m_clipRect.x = leftMax;
    } else {
        // Shouldn't get here as getClipping{Left|Top|Right|Bottom} should only return HIDDEN or VISIBLE.
        assert(false);
    }

    f32 topMax = m_position.y - m_padding.y;
    ClippingState top = getClippingTop();
    if (top == ClippingState::VISIBLE
            || topMax < parentClipRect.y) {
        m_clipRect.y = parentClipRect.y;
    } else if (top == ClippingState::HIDDEN) {
        m_clipRect.y = topMax;
    } else {
        assert(false);
    }

    f32 rightMax = m_position.x + m_size.x + m_padding.z;
    ClippingState right = getClippingRight();
    if (right == ClippingState::VISIBLE
            || rightMax > parentClipRect.x + parentClipRect.z) {
        m_clipRect.z = parentClipRect.x + parentClipRect.z - m_clipRect.x;
    } else if (right == ClippingState::HIDDEN) {
        m_clipRect.z = rightMax - m_clipRect.x;
    } else {
        assert(false);
    }

    f32 bottomMax = m_position.y + m_size.y + m_padding.w;
    ClippingState bottom = getClippingBottom();
    if (bottom == ClippingState::VISIBLE
            || bottomMax > parentClipRect.y + parentClipRect.w) {
        m_clipRect.w = parentClipRect.y + parentClipRect.w - m_clipRect.y;
    } else if (bottom == ClippingState::HIDDEN) {
        m_clipRect.w = bottomMax - m_clipRect.y;
    } else {
        assert(false);
    }

    if (oldClipRect != m_clipRect) {
        markChildrenToCalculateClipRect();
    }
}

void vui::IWidget::markChildrenToCalculateClipRect() {
    for (auto& child : m_widgets) {
        child->m_flags.needsClipRectRecalculation = true;
    }
}

void vui::IWidget::reorderWidgets() {
    bool change = false;

    std::sort(m_widgets.begin(), m_widgets.end(), [&change](const IWidget* lhs, const IWidget* rhs) {
        bool res = lhs->getZIndex() < rhs->getZIndex();

        // Mark change as true if we swap any entries.
        change |= !res;

        return res;
    });

    // If a change occurs, we need to recalculate dockings of children.
    if (change) {
        m_flags.needsDockRecalculation = true;
    }
}

void vui::IWidget::applyOffset() {
    if (m_parent) {
        IWidget::setPosition(getPosition() + m_parent->getChildOffset());
    }
}

void vui::IWidget::onMouseDown(Sender, const MouseButtonEvent& e) {
    if (!m_flags.isEnabled) return;
    if (m_flags.isMouseIn) {
        MouseDown(e);
        m_flags.isClicking = true;
    }
}

void vui::IWidget::onMouseUp(Sender, const MouseButtonEvent& e) {
    if (!m_flags.isEnabled) return;
    if (m_flags.isMouseIn) {
        MouseUp(e);
        if (m_flags.isClicking) MouseClick(e);
    }
    m_flags.isClicking = false;
}

void vui::IWidget::onMouseMove(Sender, const MouseMotionEvent& e) {
    if (!m_flags.isEnabled) return;
    if (isInBounds((f32)e.x, (f32)e.y)) {
        if (!m_flags.isMouseIn) {
            m_flags.isMouseIn = true;
            MouseEnter(e);
        }
        MouseMove(e);
    } else if (m_flags.isMouseIn) {
        m_flags.isMouseIn = false;
        MouseLeave(e);
    }
}

void vui::IWidget::onMouseFocusLost(Sender, const MouseEvent& e) {
    if (!m_flags.isEnabled) return;
    if (m_flags.isMouseIn) {
        m_flags.isMouseIn = false;
        MouseMotionEvent ev;
        ev.x = e.x;
        ev.y = e.y;
        MouseLeave(ev);
    }
}
