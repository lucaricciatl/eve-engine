#include "engine/UISystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace vkengine {

// ============================================================================
// Easing Functions
// ============================================================================

float evaluateEasing(EaseType type, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    
    switch (type) {
        case EaseType::Linear:
            return t;
        case EaseType::EaseIn:
        case EaseType::EaseInQuad:
            return t * t;
        case EaseType::EaseOut:
        case EaseType::EaseOutQuad:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case EaseType::EaseInOut:
        case EaseType::EaseInOutQuad:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        case EaseType::EaseInCubic:
            return t * t * t;
        case EaseType::EaseOutCubic:
            return 1.0f - std::pow(1.0f - t, 3.0f);
        case EaseType::EaseInOutCubic:
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        case EaseType::EaseInElastic: {
            if (t == 0.0f || t == 1.0f) return t;
            return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * (2.0f * 3.14159f / 3.0f));
        }
        case EaseType::EaseOutElastic: {
            if (t == 0.0f || t == 1.0f) return t;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (2.0f * 3.14159f / 3.0f)) + 1.0f;
        }
        case EaseType::EaseInOutElastic: {
            if (t == 0.0f || t == 1.0f) return t;
            if (t < 0.5f) {
                return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * 3.14159f / 4.5f))) / 2.0f;
            }
            return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * 3.14159f / 4.5f))) / 2.0f + 1.0f;
        }
        case EaseType::EaseInBounce:
            return 1.0f - evaluateEasing(EaseType::EaseOutBounce, 1.0f - t);
        case EaseType::EaseOutBounce: {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
            if (t < 1.0f / d1) {
                return n1 * t * t;
            } else if (t < 2.0f / d1) {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            } else if (t < 2.5f / d1) {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            } else {
                t -= 2.625f / d1;
                return n1 * t * t + 0.984375f;
            }
        }
        case EaseType::EaseInOutBounce:
            return t < 0.5f
                ? (1.0f - evaluateEasing(EaseType::EaseOutBounce, 1.0f - 2.0f * t)) / 2.0f
                : (1.0f + evaluateEasing(EaseType::EaseOutBounce, 2.0f * t - 1.0f)) / 2.0f;
    }
    return t;
}

// ============================================================================
// UIElement Implementation
// ============================================================================

UIElement::UIElement() = default;

void UIElement::addChild(std::shared_ptr<UIElement> child) {
    child->parentElement = this;
    childElements.push_back(std::move(child));
    layoutDirty = true;
}

void UIElement::removeChild(UIElement* child) {
    childElements.erase(
        std::remove_if(childElements.begin(), childElements.end(),
                       [child](const auto& ptr) { return ptr.get() == child; }),
        childElements.end());
    layoutDirty = true;
}

void UIElement::removeAllChildren() {
    childElements.clear();
    layoutDirty = true;
}

UIElement* UIElement::findById(const std::string& id) {
    if (elementId == id) return this;
    for (auto& child : childElements) {
        if (auto* found = child->findById(id)) return found;
    }
    return nullptr;
}

UIElement* UIElement::findByName(const std::string& name) {
    if (elementName == name) return this;
    for (auto& child : childElements) {
        if (auto* found = child->findByName(name)) return found;
    }
    return nullptr;
}

void UIElement::addEventListener(UIEventType type, UIEventHandler handler) {
    eventHandlers[type] = std::move(handler);
}

void UIElement::removeEventListener(UIEventType type) {
    eventHandlers.erase(type);
}

void UIElement::dispatchEvent(UIEvent& event) {
    auto it = eventHandlers.find(event.type);
    if (it != eventHandlers.end()) {
        it->second(event);
    }
}

void UIElement::update(float deltaSeconds) {
    onUpdate(deltaSeconds);
    for (auto& child : childElements) {
        child->update(deltaSeconds);
    }
}

void UIElement::render(UIRenderer& renderer) {
    if (!visible) return;
    onRender(renderer);
    for (auto& child : childElements) {
        child->render(renderer);
    }
}

void UIElement::layout() {
    if (layoutDirty) {
        onLayout();
        layoutDirty = false;
    }
    for (auto& child : childElements) {
        child->layout();
    }
}

glm::vec4 UIElement::getBounds() const {
    return glm::vec4(position.x, position.y, size.x, size.y);
}

glm::vec4 UIElement::getGlobalBounds() const {
    glm::vec2 globalPos = position;
    if (parentElement) {
        glm::vec4 parentBounds = parentElement->getGlobalBounds();
        globalPos += glm::vec2(parentBounds.x, parentBounds.y);
    }
    return glm::vec4(globalPos.x, globalPos.y, size.x, size.y);
}

bool UIElement::containsPoint(const glm::vec2& point) const {
    glm::vec4 bounds = getGlobalBounds();
    return point.x >= bounds.x && point.x <= bounds.x + bounds.z &&
           point.y >= bounds.y && point.y <= bounds.y + bounds.w;
}

// ============================================================================
// UILabel Implementation
// ============================================================================

UILabel::UILabel() = default;

UILabel::UILabel(const std::string& t) : text(t) {}

void UILabel::setText(const std::string& t) {
    text = t;
    layoutDirty = true;
}

void UILabel::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    renderer.drawText(text, glm::vec2(bounds.x, bounds.y), style.textColor, style.fontSize);
}

// ============================================================================
// UIButton Implementation
// ============================================================================

UIButton::UIButton() = default;

UIButton::UIButton(const std::string& t) : text(t) {}

void UIButton::setText(const std::string& t) {
    text = t;
    layoutDirty = true;
}

void UIButton::setIcon(const std::string& path) {
    iconPath = path;
}

void UIButton::setOnClick(std::function<void()> callback) {
    onClick = std::move(callback);
}

void UIButton::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    UIColor bgColor = style.backgroundColor;
    if (!enabled) {
        bgColor = style.disabledColor;
    } else if (pressed) {
        bgColor = style.activeColor;
    } else if (hovered) {
        bgColor = style.hoverColor;
    }
    
    renderer.drawRect(bounds, bgColor, style.border.radius);
    
    if (!iconPath.empty()) {
        glm::vec4 iconBounds(bounds.x + 4, bounds.y + 4, bounds.w - 8, bounds.w - 8);
        renderer.drawImage(iconPath, iconBounds, style.textColor);
    }
    
    float textX = bounds.x + (iconPath.empty() ? style.padding.left : bounds.w + 4);
    float textY = bounds.y + bounds.w / 2 - style.fontSize / 2;
    UIColor textColor = enabled ? style.textColor : style.disabledTextColor;
    renderer.drawText(text, glm::vec2(textX, textY), textColor, style.fontSize);
}

void UIButton::onUpdate(float deltaSeconds) {
    // Smooth hover/press transitions
    float targetHover = hovered ? 1.0f : 0.0f;
    float targetPress = pressed ? 1.0f : 0.0f;
    
    hoverTransition += (targetHover - hoverTransition) * deltaSeconds * 10.0f;
    pressTransition += (targetPress - pressTransition) * deltaSeconds * 15.0f;
}

// ============================================================================
// UICheckbox Implementation
// ============================================================================

UICheckbox::UICheckbox() = default;

UICheckbox::UICheckbox(const std::string& l) : label(l) {}

void UICheckbox::setChecked(bool c) {
    if (checked != c) {
        checked = c;
        if (onChanged) onChanged(checked);
    }
}

void UICheckbox::setLabel(const std::string& l) {
    label = l;
    layoutDirty = true;
}

void UICheckbox::setOnChanged(std::function<void(bool)> callback) {
    onChanged = std::move(callback);
}

void UICheckbox::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    float boxSize = 20.0f;
    glm::vec4 boxBounds(bounds.x, bounds.y + (bounds.w - boxSize) / 2, boxSize, boxSize);
    
    renderer.drawRect(boxBounds, style.backgroundColor, 4);
    renderer.drawRectOutline(boxBounds, style.border.color, style.border.width, 4);
    
    if (checked) {
        glm::vec4 checkBounds(boxBounds.x + 4, boxBounds.y + 4, boxSize - 8, boxSize - 8);
        renderer.drawRect(checkBounds, style.activeColor, 2);
    }
    
    if (!label.empty()) {
        float textX = bounds.x + boxSize + 8;
        float textY = bounds.y + bounds.w / 2 - style.fontSize / 2;
        renderer.drawText(label, glm::vec2(textX, textY), style.textColor, style.fontSize);
    }
}

// ============================================================================
// UISlider Implementation
// ============================================================================

UISlider::UISlider() = default;

void UISlider::setValue(float v) {
    float newValue = std::clamp(v, minValue, maxValue);
    if (step > 0) {
        newValue = std::round((newValue - minValue) / step) * step + minValue;
    }
    if (value != newValue) {
        value = newValue;
        if (onChanged) onChanged(value);
    }
}

void UISlider::setRange(float min, float max) {
    minValue = min;
    maxValue = max;
    setValue(value); // Re-clamp
}

void UISlider::setOnChanged(std::function<void(float)> callback) {
    onChanged = std::move(callback);
}

void UISlider::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    // Track
    float trackHeight = 4.0f;
    float trackY = bounds.y + bounds.w / 2 - trackHeight / 2;
    glm::vec4 trackBounds(bounds.x, trackY, bounds.z, trackHeight);
    renderer.drawRect(trackBounds, UIColor(0.3f, 0.3f, 0.3f, 1.0f), 2);
    
    // Fill
    float normalizedValue = (value - minValue) / (maxValue - minValue);
    float fillWidth = normalizedValue * bounds.z;
    if (fillWidth > 0) {
        glm::vec4 fillBounds(bounds.x, trackY, fillWidth, trackHeight);
        renderer.drawRect(fillBounds, style.activeColor, 2);
    }
    
    // Handle
    float handleRadius = 8.0f;
    float handleX = bounds.x + fillWidth;
    float handleY = bounds.y + bounds.w / 2;
    UIColor handleColor = dragging ? style.activeColor : style.hoverColor;
    renderer.drawCircle(glm::vec2(handleX, handleY), handleRadius, handleColor);
}

void UISlider::onUpdate(float /*deltaSeconds*/) {
    // Handle input would be processed here
}

// ============================================================================
// UITextField Implementation
// ============================================================================

UITextField::UITextField() = default;

void UITextField::setText(const std::string& t) {
    if (maxLength > 0 && t.length() > maxLength) {
        text = t.substr(0, maxLength);
    } else {
        text = t;
    }
    cursorPosition = text.length();
    if (onChanged) onChanged(text);
}

void UITextField::setPlaceholder(const std::string& p) {
    placeholder = p;
}

void UITextField::setOnChanged(std::function<void(const std::string&)> callback) {
    onChanged = std::move(callback);
}

void UITextField::setOnSubmit(std::function<void(const std::string&)> callback) {
    onSubmit = std::move(callback);
}

void UITextField::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    // Background
    renderer.drawRect(bounds, style.backgroundColor, style.border.radius);
    renderer.drawRectOutline(bounds, style.border.color, style.border.width, style.border.radius);
    
    // Text content
    std::string displayText;
    UIColor textColor;
    
    if (text.empty() && !focused) {
        displayText = placeholder;
        textColor = UIColor(0.5f, 0.5f, 0.5f, 1.0f);
    } else {
        displayText = isPassword ? std::string(text.length(), '*') : text;
        textColor = style.textColor;
    }
    
    float textX = bounds.x + style.padding.left;
    float textY = bounds.y + bounds.w / 2 - style.fontSize / 2;
    renderer.drawText(displayText, glm::vec2(textX, textY), textColor, style.fontSize);
    
    // Cursor (when focused and visible)
    if (focused && cursorBlink < 0.5f) {
        float cursorX = textX + renderer.measureText(displayText.substr(0, cursorPosition), style.fontSize).x;
        renderer.drawLine(
            glm::vec2(cursorX, bounds.y + 4),
            glm::vec2(cursorX, bounds.y + bounds.w - 4),
            style.textColor, 1.0f);
    }
    
    // Selection highlight
    if (selectionStart != selectionEnd) {
        std::size_t startPos = (std::min)(selectionStart, selectionEnd);
        std::size_t endPos = (std::max)(selectionStart, selectionEnd);
        float selStartX = textX + renderer.measureText(displayText.substr(0, startPos), style.fontSize).x;
        float selEndX = textX + renderer.measureText(displayText.substr(0, endPos), style.fontSize).x;
        glm::vec4 selBounds(selStartX, bounds.y + 2, selEndX - selStartX, bounds.w - 4);
        renderer.drawRect(selBounds, UIColor(0.3f, 0.5f, 0.8f, 0.5f), 0);
    }
}

void UITextField::onUpdate(float deltaSeconds) {
    cursorBlink += deltaSeconds;
    if (cursorBlink >= 1.0f) {
        cursorBlink = 0.0f;
    }
}

// ============================================================================
// UIProgressBar Implementation
// ============================================================================

UIProgressBar::UIProgressBar() = default;

void UIProgressBar::setValue(float v) {
    value = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    // Background track
    renderer.drawRect(bounds, style.backgroundColor, style.border.radius);
    
    // Fill
    float fillWidth = bounds.z * value;
    if (fillWidth > 0) {
        glm::vec4 fillBounds(bounds.x, bounds.y, fillWidth, bounds.w);
        renderer.drawRect(fillBounds, fillColor, style.border.radius);
    }
    
    // Text
    if (showText) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), textFormat.c_str(), static_cast<int>(value * 100));
        std::string progressText = buffer;
        glm::vec2 textSize = renderer.measureText(progressText, style.fontSize);
        float textX = bounds.x + (bounds.z - textSize.x) / 2;
        float textY = bounds.y + (bounds.w - textSize.y) / 2;
        renderer.drawText(progressText, glm::vec2(textX, textY), style.textColor, style.fontSize);
    }
}

// ============================================================================
// UIImage Implementation
// ============================================================================

UIImage::UIImage() = default;

UIImage::UIImage(const std::string& path) : imagePath(path) {}

void UIImage::setImage(const std::string& path) {
    imagePath = path;
}

void UIImage::onRender(UIRenderer& renderer) {
    if (imagePath.empty()) return;
    
    glm::vec4 bounds = getGlobalBounds();
    
    if (preserveAspect) {
        // Would calculate aspect-correct bounds here
    }
    
    renderer.drawImage(imagePath, bounds, tint);
}

// ============================================================================
// UIPanel Implementation
// ============================================================================

UIPanel::UIPanel() = default;

void UIPanel::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    renderer.drawRect(bounds, style.backgroundColor, style.border.radius);
    if (style.border.width > 0) {
        renderer.drawRectOutline(bounds, style.border.color, style.border.width, style.border.radius);
    }
}

// ============================================================================
// UIHorizontalLayout Implementation
// ============================================================================

UIHorizontalLayout::UIHorizontalLayout() = default;

void UIHorizontalLayout::onLayout() {
    float x = style.padding.left;
    for (auto& child : childElements) {
        child->setPosition(glm::vec2(x, style.padding.top));
        x += child->getSize().x + spacing;
    }
}

// ============================================================================
// UIVerticalLayout Implementation
// ============================================================================

UIVerticalLayout::UIVerticalLayout() = default;

void UIVerticalLayout::onLayout() {
    float y = style.padding.top;
    for (auto& child : childElements) {
        child->setPosition(glm::vec2(style.padding.left, y));
        y += child->getSize().y + spacing;
    }
}

// ============================================================================
// UIGridLayout Implementation
// ============================================================================

UIGridLayout::UIGridLayout() = default;

void UIGridLayout::onLayout() {
    if (columns == 0 || childElements.empty()) return;
    
    float cellWidth = (size.x - style.padding.left - style.padding.right - (columns - 1) * spacing.x) / columns;
    float cellHeight = (size.y - style.padding.top - style.padding.bottom - (rows - 1) * spacing.y) / rows;
    
    for (std::size_t i = 0; i < childElements.size(); ++i) {
        int col = static_cast<int>(i % columns);
        int row = static_cast<int>(i / columns);
        float x = style.padding.left + col * (cellWidth + spacing.x);
        float y = style.padding.top + row * (cellHeight + spacing.y);
        childElements[i]->setPosition(glm::vec2(x, y));
        childElements[i]->setSize(glm::vec2(cellWidth, cellHeight));
    }
}

// ============================================================================
// UIScrollView Implementation
// ============================================================================

UIScrollView::UIScrollView() = default;

void UIScrollView::setContent(std::shared_ptr<UIElement> c) {
    content = std::move(c);
    // Note: content->parentElement assignment would be handled by addChild
    // if we use the parent class mechanism
    if (content) {
        addChild(content);  // Use proper parent class method
    }
    layoutDirty = true;
}

void UIScrollView::setScrollPosition(const glm::vec2& pos) {
    scrollPosition = pos;
}

void UIScrollView::onRender(UIRenderer& renderer) {
    glm::vec4 bounds = getGlobalBounds();
    
    // Draw background
    renderer.drawRect(bounds, style.backgroundColor, style.border.radius);
    
    // Push scissor for content clipping
    renderer.pushScissor(bounds);
    
    // Transform content by scroll position
    glm::mat3 scrollTransform(1.0f);
    scrollTransform[2][0] = -scrollPosition.x;
    scrollTransform[2][1] = -scrollPosition.y;
    renderer.pushTransform(scrollTransform);
    
    // Render content
    if (content) {
        content->render(renderer);
    }
    
    renderer.popTransform();
    renderer.popScissor();
    
    // Get content size for scrollbars
    glm::vec2 contentSize = content ? content->getSize() : glm::vec2(0);
    
    // Draw scrollbars if needed
    if (showHScrollbar && contentSize.x > bounds.z) {
        float scrollRatio = bounds.z / contentSize.x;
        float thumbWidth = bounds.z * scrollRatio;
        float thumbX = bounds.x + (scrollPosition.x / contentSize.x) * bounds.z;
        glm::vec4 hBar(thumbX, bounds.y + bounds.w - scrollbarWidth, thumbWidth, scrollbarWidth);
        renderer.drawRect(hBar, UIColor(0.5f, 0.5f, 0.5f, 0.5f), 2);
    }
    
    if (showVScrollbar && contentSize.y > bounds.w) {
        float scrollRatio = bounds.w / contentSize.y;
        float thumbHeight = bounds.w * scrollRatio;
        float thumbY = bounds.y + (scrollPosition.y / contentSize.y) * bounds.w;
        glm::vec4 vBar(bounds.x + bounds.z - scrollbarWidth, thumbY, scrollbarWidth, thumbHeight);
        renderer.drawRect(vBar, UIColor(0.5f, 0.5f, 0.5f, 0.5f), 2);
    }
}

void UIScrollView::onUpdate(float deltaSeconds) {
    // Smooth scrolling
    if (scrollVelocity.x != 0 || scrollVelocity.y != 0) {
        scrollPosition += scrollVelocity * deltaSeconds;
        scrollVelocity *= 0.9f; // Deceleration
        
        // Get content size
        glm::vec2 contentSize = content ? content->getSize() : glm::vec2(0);
        
        // Clamp scroll position
        scrollPosition.x = std::clamp(scrollPosition.x, 0.0f, (std::max)(0.0f, contentSize.x - size.x));
        scrollPosition.y = std::clamp(scrollPosition.y, 0.0f, (std::max)(0.0f, contentSize.y - size.y));
        
        // Stop when velocity is very small
        if (std::abs(scrollVelocity.x) < 0.1f) scrollVelocity.x = 0;
        if (std::abs(scrollVelocity.y) < 0.1f) scrollVelocity.y = 0;
    }
}

void UIScrollView::onLayout() {
    if (content) {
        content->layout();
    }
}

// ============================================================================
// UICanvas Implementation
// ============================================================================

UICanvas::UICanvas() = default;

void UICanvas::setScreenSize(const glm::vec2& sz) {
    screenSize = sz;
    size = sz;
}

void UICanvas::processEvent(UIEvent& event) {
    if (event.type == UIEventType::MouseMove) {
        updateHover(event.mousePosition);
    }
    
    // Dispatch to focused/hovered element
    if (focusedElement) {
        focusedElement->dispatchEvent(event);
    } else if (hoveredElement) {
        hoveredElement->dispatchEvent(event);
    }
    
    dispatchEvent(event);
}

void UICanvas::updateAll(float deltaSeconds) {
    update(deltaSeconds);
}

void UICanvas::renderAll(UIRenderer& renderer) {
    renderer.begin(screenSize);
    render(renderer);
    renderer.end();
}

void UICanvas::setFocusedElement(UIElement* element) {
    if (focusedElement) {
        focusedElement->focused = false;
    }
    focusedElement = element;
    if (focusedElement) {
        focusedElement->focused = true;
    }
}

UIElement* UICanvas::findElementAtPoint(const glm::vec2& point) {
    // Search children in reverse (top-most first)
    for (auto it = childElements.rbegin(); it != childElements.rend(); ++it) {
        if ((*it)->isVisible() && (*it)->containsPoint(point)) {
            return it->get();
        }
    }
    return nullptr;
}

void UICanvas::updateHover(const glm::vec2& mousePos) {
    UIElement* newHovered = findElementAtPoint(mousePos);
    
    if (hoveredElement != newHovered) {
        if (hoveredElement) {
            hoveredElement->hovered = false;
        }
        hoveredElement = newHovered;
        if (hoveredElement) {
            hoveredElement->hovered = true;
        }
    }
    
    lastMousePosition = mousePos;
}

// ============================================================================
// UIRenderer Implementation
// ============================================================================

UIRenderer::UIRenderer() = default;

void UIRenderer::begin(const glm::vec2& /*screenSize*/) {
    // Setup rendering state
}

void UIRenderer::end() {
    // Flush batched draws
}

void UIRenderer::drawRect(const glm::vec4& /*bounds*/, const UIColor& /*color*/, float /*cornerRadius*/) {
    // Implementation would use ImGui or custom GPU rendering
}

void UIRenderer::drawRectOutline(const glm::vec4& /*bounds*/, const UIColor& /*color*/, float /*thickness*/, float /*cornerRadius*/) {
    // Implementation
}

void UIRenderer::drawText(const std::string& /*text*/, const glm::vec2& /*position*/, const UIColor& /*color*/, float /*fontSize*/, const std::string& /*font*/) {
    // Implementation
}

void UIRenderer::drawImage(const std::string& /*path*/, const glm::vec4& /*bounds*/, const UIColor& /*tint*/) {
    // Implementation
}

void UIRenderer::drawLine(const glm::vec2& /*start*/, const glm::vec2& /*end*/, const UIColor& /*color*/, float /*thickness*/) {
    // Implementation
}

void UIRenderer::drawCircle(const glm::vec2& /*center*/, float /*radius*/, const UIColor& /*color*/, int /*segments*/) {
    // Implementation
}

void UIRenderer::drawCircleOutline(const glm::vec2& /*center*/, float /*radius*/, const UIColor& /*color*/, float /*thickness*/, int /*segments*/) {
    // Implementation
}

void UIRenderer::pushScissor(const glm::vec4& /*bounds*/) {
    // Implementation
}

void UIRenderer::popScissor() {
    // Implementation
}

void UIRenderer::pushTransform(const glm::mat3& /*transform*/) {
    // Implementation
}

void UIRenderer::popTransform() {
    // Implementation
}

glm::vec2 UIRenderer::measureText(const std::string& /*text*/, float /*fontSize*/, const std::string& /*font*/) {
    return glm::vec2(100.0f, 14.0f); // Stub
}

} // namespace vkengine
