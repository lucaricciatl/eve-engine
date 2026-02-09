#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class VulkanRenderer;

// ============================================================================
// UI Types
// ============================================================================

enum class Anchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class LayoutDirection {
    Horizontal,
    Vertical
};

enum class TextAlignment {
    Left,
    Center,
    Right
};

enum class VerticalAlignment {
    Top,
    Middle,
    Bottom
};

// ============================================================================
// UI Styles
// ============================================================================

struct UIColor {
    float r{1.0f}, g{1.0f}, b{1.0f}, a{1.0f};

    UIColor() = default;
    UIColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    UIColor(const glm::vec4& v) : r(v.r), g(v.g), b(v.b), a(v.a) {}
    UIColor(std::uint32_t hex);

    [[nodiscard]] glm::vec4 toVec4() const { return {r, g, b, a}; }
    [[nodiscard]] UIColor withAlpha(float alpha) const { return {r, g, b, alpha}; }

    static UIColor transparent() { return {0, 0, 0, 0}; }
    static UIColor white() { return {1, 1, 1, 1}; }
    static UIColor black() { return {0, 0, 0, 1}; }
    static UIColor red() { return {1, 0, 0, 1}; }
    static UIColor green() { return {0, 1, 0, 1}; }
    static UIColor blue() { return {0, 0, 1, 1}; }
};

struct UIMargin {
    float left{0}, top{0}, right{0}, bottom{0};

    UIMargin() = default;
    UIMargin(float all) : left(all), top(all), right(all), bottom(all) {}
    UIMargin(float horizontal, float vertical) : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    UIMargin(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

    [[nodiscard]] float horizontal() const { return left + right; }
    [[nodiscard]] float vertical() const { return top + bottom; }
};

struct UIBorder {
    float width{0};
    UIColor color{0.5f, 0.5f, 0.5f, 1.0f};
    float radius{0};  // Corner radius
};

struct UIShadow {
    bool enabled{false};
    glm::vec2 offset{2, 2};
    float blur{4};
    UIColor color{0, 0, 0, 0.5f};
};

struct UIStyle {
    UIColor backgroundColor{0.2f, 0.2f, 0.2f, 1.0f};
    UIColor textColor{1.0f, 1.0f, 1.0f, 1.0f};
    UIColor hoverColor{0.3f, 0.3f, 0.3f, 1.0f};
    UIColor activeColor{0.4f, 0.4f, 0.4f, 1.0f};
    UIColor disabledColor{0.15f, 0.15f, 0.15f, 0.5f};
    UIColor disabledTextColor{0.5f, 0.5f, 0.5f, 1.0f};
    UIBorder border;
    UIShadow shadow;
    UIMargin padding{4};
    UIMargin margin{2};
    std::string fontName;
    float fontSize{14};
    float opacity{1.0f};
};

// ============================================================================
// UI Transitions
// ============================================================================

enum class EaseType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce
};

struct UITransition {
    float duration{0.2f};
    EaseType easing{EaseType::EaseOut};
    float delay{0.0f};
};

float evaluateEasing(EaseType type, float t);

// ============================================================================
// UI Events
// ============================================================================

enum class UIEventType {
    MouseEnter,
    MouseLeave,
    MouseMove,
    MouseDown,
    MouseUp,
    Click,
    DoubleClick,
    RightClick,
    DragStart,
    Drag,
    DragEnd,
    Scroll,
    Focus,
    Blur,
    KeyDown,
    KeyUp,
    TextInput,
    ValueChanged
};

struct UIEvent {
    UIEventType type;
    glm::vec2 mousePosition{0};
    glm::vec2 mouseDelta{0};
    int mouseButton{0};
    int key{0};
    int modifiers{0};
    char character{0};
    float scrollDelta{0};
    bool consumed{false};

    void consume() { consumed = true; }
};

using UIEventHandler = std::function<void(const UIEvent&)>;

// ============================================================================
// Base UI Element
// ============================================================================

class UIElement {
public:
    UIElement();
    virtual ~UIElement() = default;

    // Identity
    void setId(const std::string& id) { elementId = id; }
    [[nodiscard]] const std::string& id() const { return elementId; }
    void setName(const std::string& name) { elementName = name; }
    [[nodiscard]] const std::string& name() const { return elementName; }

    // Hierarchy
    void addChild(std::shared_ptr<UIElement> child);
    void removeChild(UIElement* child);
    void removeAllChildren();
    [[nodiscard]] UIElement* parent() const { return parentElement; }
    [[nodiscard]] const std::vector<std::shared_ptr<UIElement>>& children() const { return childElements; }
    [[nodiscard]] UIElement* findById(const std::string& id);
    [[nodiscard]] UIElement* findByName(const std::string& name);

    // Transform
    void setPosition(const glm::vec2& pos) { position = pos; layoutDirty = true; }
    [[nodiscard]] const glm::vec2& getPosition() const { return position; }
    void setSize(const glm::vec2& size) { this->size = size; layoutDirty = true; }
    [[nodiscard]] const glm::vec2& getSize() const { return size; }
    void setAnchor(Anchor anchor) { this->anchor = anchor; layoutDirty = true; }
    [[nodiscard]] Anchor getAnchor() const { return anchor; }
    void setPivot(const glm::vec2& pivot) { this->pivot = pivot; layoutDirty = true; }
    [[nodiscard]] const glm::vec2& getPivot() const { return pivot; }
    void setRotation(float degrees) { rotation = degrees; }
    [[nodiscard]] float getRotation() const { return rotation; }
    void setScale(const glm::vec2& scale) { this->scale = scale; }
    [[nodiscard]] const glm::vec2& getScale() const { return scale; }

    // Layout
    void setMinSize(const glm::vec2& size) { minSize = size; layoutDirty = true; }
    void setMaxSize(const glm::vec2& size) { maxSize = size; layoutDirty = true; }
    void setPreferredSize(const glm::vec2& size) { preferredSize = size; layoutDirty = true; }
    void setFlexGrow(float value) { flexGrow = value; layoutDirty = true; }
    void setFlexShrink(float value) { flexShrink = value; layoutDirty = true; }

    // Style
    void setStyle(const UIStyle& style) { this->style = style; }
    [[nodiscard]] UIStyle& getStyle() { return style; }
    [[nodiscard]] const UIStyle& getStyle() const { return style; }

    // Visibility and interaction
    void setVisible(bool visible) { this->visible = visible; }
    [[nodiscard]] bool isVisible() const { return visible; }
    void setEnabled(bool enabled) { this->enabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return enabled; }
    void setInteractive(bool interactive) { this->interactive = interactive; }
    [[nodiscard]] bool isInteractive() const { return interactive; }

    // Events
    void addEventListener(UIEventType type, UIEventHandler handler);
    void removeEventListener(UIEventType type);
    void dispatchEvent(UIEvent& event);

    // State
    [[nodiscard]] bool isHovered() const { return hovered; }
    [[nodiscard]] bool isFocused() const { return focused; }
    [[nodiscard]] bool isPressed() const { return pressed; }

    // Update and render
    virtual void update(float deltaSeconds);
    virtual void render(class UIRenderer& renderer);
    virtual void layout();

    // Bounds
    [[nodiscard]] glm::vec4 getBounds() const;  // x, y, width, height
    [[nodiscard]] glm::vec4 getGlobalBounds() const;
    [[nodiscard]] bool containsPoint(const glm::vec2& point) const;

protected:
    virtual void onUpdate(float deltaSeconds) {}
    virtual void onRender(class UIRenderer& renderer) {}
    virtual void onLayout() {}
    virtual glm::vec2 calculatePreferredSize() const { return preferredSize; }

    std::string elementId;
    std::string elementName;
    UIElement* parentElement{nullptr};
    std::vector<std::shared_ptr<UIElement>> childElements;

    glm::vec2 position{0};
    glm::vec2 size{100, 100};
    glm::vec2 minSize{0};
    glm::vec2 maxSize{std::numeric_limits<float>::max()};
    glm::vec2 preferredSize{0};
    glm::vec2 pivot{0.5f, 0.5f};
    glm::vec2 scale{1.0f};
    float rotation{0};
    Anchor anchor{Anchor::TopLeft};

    float flexGrow{0};
    float flexShrink{1};

    UIStyle style;
    std::unordered_map<UIEventType, UIEventHandler> eventHandlers;

    bool visible{true};
    bool enabled{true};
    bool interactive{true};
    bool hovered{false};
    bool focused{false};
    bool pressed{false};
    bool layoutDirty{true};

    friend class UICanvas;  // Allow UICanvas to access protected members
};

// ============================================================================
// UI Widgets
// ============================================================================

class UILabel : public UIElement {
public:
    UILabel();
    explicit UILabel(const std::string& text);

    void setText(const std::string& text);
    [[nodiscard]] const std::string& getText() const { return text; }
    void setTextAlignment(TextAlignment align) { alignment = align; }
    void setVerticalAlignment(VerticalAlignment align) { verticalAlign = align; }
    void setWordWrap(bool wrap) { wordWrap = wrap; layoutDirty = true; }

protected:
    void onRender(UIRenderer& renderer) override;
    glm::vec2 calculatePreferredSize() const override;

private:
    std::string text;
    TextAlignment alignment{TextAlignment::Left};
    VerticalAlignment verticalAlign{VerticalAlignment::Middle};
    bool wordWrap{false};
};

class UIButton : public UIElement {
public:
    UIButton();
    explicit UIButton(const std::string& text);

    void setText(const std::string& text);
    [[nodiscard]] const std::string& getText() const { return text; }
    void setIcon(const std::string& iconPath);
    void setOnClick(std::function<void()> callback);

protected:
    void onRender(UIRenderer& renderer) override;
    void onUpdate(float deltaSeconds) override;

private:
    std::string text;
    std::string iconPath;
    std::function<void()> onClick;
    float hoverTransition{0};
    float pressTransition{0};
};

class UICheckbox : public UIElement {
public:
    UICheckbox();
    explicit UICheckbox(const std::string& label);

    void setChecked(bool checked);
    [[nodiscard]] bool isChecked() const { return checked; }
    void setLabel(const std::string& label);
    void setOnChanged(std::function<void(bool)> callback);

protected:
    void onRender(UIRenderer& renderer) override;

private:
    std::string label;
    bool checked{false};
    std::function<void(bool)> onChanged;
};

class UISlider : public UIElement {
public:
    UISlider();

    void setValue(float value);
    [[nodiscard]] float getValue() const { return value; }
    void setRange(float min, float max);
    void setStep(float step) { this->step = step; }
    void setOnChanged(std::function<void(float)> callback);

protected:
    void onRender(UIRenderer& renderer) override;
    void onUpdate(float deltaSeconds) override;

private:
    float value{0};
    float minValue{0};
    float maxValue{1};
    float step{0};
    bool dragging{false};
    std::function<void(float)> onChanged;
};

class UITextField : public UIElement {
public:
    UITextField();

    void setText(const std::string& text);
    [[nodiscard]] const std::string& getText() const { return text; }
    void setPlaceholder(const std::string& placeholder);
    void setPassword(bool password) { isPassword = password; }
    void setMaxLength(std::size_t length) { maxLength = length; }
    void setOnChanged(std::function<void(const std::string&)> callback);
    void setOnSubmit(std::function<void(const std::string&)> callback);

protected:
    void onRender(UIRenderer& renderer) override;
    void onUpdate(float deltaSeconds) override;

private:
    std::string text;
    std::string placeholder;
    bool isPassword{false};
    std::size_t maxLength{0};
    std::size_t cursorPosition{0};
    std::size_t selectionStart{0};
    std::size_t selectionEnd{0};
    float cursorBlink{0};
    std::function<void(const std::string&)> onChanged;
    std::function<void(const std::string&)> onSubmit;
};

class UIProgressBar : public UIElement {
public:
    UIProgressBar();

    void setValue(float value);
    [[nodiscard]] float getValue() const { return value; }
    void setShowText(bool show) { showText = show; }
    void setTextFormat(const std::string& format) { textFormat = format; }

protected:
    void onRender(UIRenderer& renderer) override;

private:
    float value{0};
    bool showText{true};
    std::string textFormat{"%d%%"};
    UIColor fillColor{0.2f, 0.6f, 1.0f, 1.0f};
};

class UIImage : public UIElement {
public:
    UIImage();
    explicit UIImage(const std::string& path);

    void setImage(const std::string& path);
    void setTint(const UIColor& color) { tint = color; }
    void setPreserveAspect(bool preserve) { preserveAspect = preserve; }

protected:
    void onRender(UIRenderer& renderer) override;

private:
    std::string imagePath;
    UIColor tint{1, 1, 1, 1};
    bool preserveAspect{true};
};

// ============================================================================
// Layout Containers
// ============================================================================

class UIPanel : public UIElement {
public:
    UIPanel();

protected:
    void onRender(UIRenderer& renderer) override;
};

class UIHorizontalLayout : public UIElement {
public:
    UIHorizontalLayout();

    void setSpacing(float spacing) { this->spacing = spacing; layoutDirty = true; }
    void setAlignment(VerticalAlignment align) { alignment = align; layoutDirty = true; }

protected:
    void onLayout() override;

private:
    float spacing{4};
    VerticalAlignment alignment{VerticalAlignment::Middle};
};

class UIVerticalLayout : public UIElement {
public:
    UIVerticalLayout();

    void setSpacing(float spacing) { this->spacing = spacing; layoutDirty = true; }
    void setAlignment(TextAlignment align) { alignment = align; layoutDirty = true; }

protected:
    void onLayout() override;

private:
    float spacing{4};
    TextAlignment alignment{TextAlignment::Left};
};

class UIGridLayout : public UIElement {
public:
    UIGridLayout();

    void setColumns(int cols) { columns = cols; layoutDirty = true; }
    void setRows(int rows) { this->rows = rows; layoutDirty = true; }
    void setSpacing(const glm::vec2& spacing) { this->spacing = spacing; layoutDirty = true; }

protected:
    void onLayout() override;

private:
    int columns{2};
    int rows{0};  // 0 = auto
    glm::vec2 spacing{4, 4};
};

class UIScrollView : public UIElement {
public:
    UIScrollView();

    void setContent(std::shared_ptr<UIElement> content);
    void setScrollPosition(const glm::vec2& pos);
    [[nodiscard]] const glm::vec2& getScrollPosition() const { return scrollPosition; }
    void setShowHorizontalScrollbar(bool show) { showHScrollbar = show; }
    void setShowVerticalScrollbar(bool show) { showVScrollbar = show; }

protected:
    void onRender(UIRenderer& renderer) override;
    void onUpdate(float deltaSeconds) override;
    void onLayout() override;

private:
    std::shared_ptr<UIElement> content;
    glm::vec2 scrollPosition{0};
    glm::vec2 scrollVelocity{0};
    bool showHScrollbar{false};
    bool showVScrollbar{true};
    float scrollbarWidth{10};
};

// ============================================================================
// UI Canvas (root element)
// ============================================================================

class UICanvas : public UIElement {
public:
    UICanvas();

    void setScreenSize(const glm::vec2& size);
    [[nodiscard]] const glm::vec2& getScreenSize() const { return screenSize; }

    void processEvent(UIEvent& event);
    void updateAll(float deltaSeconds);
    void renderAll(UIRenderer& renderer);

    void setFocusedElement(UIElement* element);
    [[nodiscard]] UIElement* getFocusedElement() const { return focusedElement; }

private:
    UIElement* findElementAtPoint(const glm::vec2& point);
    void updateHover(const glm::vec2& mousePos);

    glm::vec2 screenSize{1280, 720};
    UIElement* focusedElement{nullptr};
    UIElement* hoveredElement{nullptr};
    UIElement* pressedElement{nullptr};
    glm::vec2 lastMousePosition{0};
};

// ============================================================================
// UI Renderer
// ============================================================================

class UIRenderer {
public:
    UIRenderer();
    virtual ~UIRenderer() = default;

    virtual void begin(const glm::vec2& screenSize);
    virtual void end();

    // Primitives
    virtual void drawRect(const glm::vec4& bounds, const UIColor& color, float cornerRadius = 0);
    virtual void drawRectOutline(const glm::vec4& bounds, const UIColor& color, float thickness = 1, float cornerRadius = 0);
    virtual void drawText(const std::string& text, const glm::vec2& position, const UIColor& color, float fontSize = 14, const std::string& font = "");
    virtual void drawImage(const std::string& path, const glm::vec4& bounds, const UIColor& tint = UIColor::white());
    virtual void drawLine(const glm::vec2& start, const glm::vec2& end, const UIColor& color, float thickness = 1);
    virtual void drawCircle(const glm::vec2& center, float radius, const UIColor& color, int segments = 32);
    virtual void drawCircleOutline(const glm::vec2& center, float radius, const UIColor& color, float thickness = 1, int segments = 32);

    // Scissor (clipping)
    virtual void pushScissor(const glm::vec4& bounds);
    virtual void popScissor();

    // Transform stack
    virtual void pushTransform(const glm::mat3& transform);
    virtual void popTransform();

    // Text measurement
    virtual glm::vec2 measureText(const std::string& text, float fontSize = 14, const std::string& font = "");

protected:
    glm::vec2 currentScreenSize{0};
    std::vector<glm::vec4> scissorStack;
    std::vector<glm::mat3> transformStack;
};

// ============================================================================
// Theme System
// ============================================================================

struct UITheme {
    std::string name;

    UIColor primaryColor{0.2f, 0.6f, 1.0f, 1.0f};
    UIColor secondaryColor{0.4f, 0.4f, 0.4f, 1.0f};
    UIColor backgroundColor{0.15f, 0.15f, 0.15f, 1.0f};
    UIColor surfaceColor{0.2f, 0.2f, 0.2f, 1.0f};
    UIColor errorColor{1.0f, 0.3f, 0.3f, 1.0f};
    UIColor successColor{0.3f, 1.0f, 0.3f, 1.0f};
    UIColor warningColor{1.0f, 0.8f, 0.3f, 1.0f};

    UIColor textPrimary{1.0f, 1.0f, 1.0f, 1.0f};
    UIColor textSecondary{0.7f, 0.7f, 0.7f, 1.0f};
    UIColor textDisabled{0.4f, 0.4f, 0.4f, 1.0f};

    float borderRadius{4};
    float borderWidth{1};
    UIColor borderColor{0.3f, 0.3f, 0.3f, 1.0f};

    std::string defaultFont;
    float defaultFontSize{14};

    UIStyle getButtonStyle() const;
    UIStyle getLabelStyle() const;
    UIStyle getTextFieldStyle() const;
    UIStyle getPanelStyle() const;

    static UITheme dark();
    static UITheme light();
};

class UIThemeManager {
public:
    static UIThemeManager& instance();

    void setTheme(const UITheme& theme);
    [[nodiscard]] const UITheme& currentTheme() const { return activeTheme; }

    void registerTheme(const std::string& name, const UITheme& theme);
    void applyTheme(const std::string& name);

private:
    UITheme activeTheme;
    std::unordered_map<std::string, UITheme> themes;
};

} // namespace vkengine
