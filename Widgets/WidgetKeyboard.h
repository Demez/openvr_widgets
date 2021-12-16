#pragma once
#include "Widgets/Widget.h"

#include <unordered_map>

class GuiSystem;
class GuiElement;
class GuiButton;
class Transformation;

enum class KeyModType
{
    None = 0,
    Hold = 1,   // Shift
    Toggle = 2, // CapsLock
};

struct KeyboardKey
{
    std::string m_text;
    unsigned int m_code;

    // modifier - text when modifier enabled
    // std::unordered_map<std::string, std::string> m_modText;
    // std::unordered_map<KeyboardKey*, std::string> m_modText;
    std::unordered_map<  std::string, std::vector<KeyboardKey*> > m_modText;
    bool m_modActive = false;
    KeyModType m_modType = KeyModType::None;

    GuiButton* m_guiButton = nullptr;
};

class WidgetKeyboard : public Widget
{
public:
    // ISO keyboard (105) QWERTY UK; https://commons.wikimedia.org/wiki/File:ISO_keyboard_(105)_QWERTY_UK.svg
    GuiSystem *m_guiSystem;
    std::vector<GuiButton*> m_guiButtons;

    static sf::Texture *ms_textureAtlas;

#ifdef _WIN32
    std::vector<HKL> m_systemLayouts;
#endif

    std::vector<KeyboardKey*> m_modKeys;
    std::vector<KeyboardKey*> m_keys;

    bool m_activeMove;
    bool m_activePin;
    unsigned long long m_lastTriggerTick;

    void OnGuiElementClick_Keys(GuiElement *f_guiElement, unsigned char f_button, unsigned char f_state);
    void UpdateKeys();
    void CheckToggleKeys();

    void UpdateTransform();

    // Widget
    bool Create();
    void Destroy() override;
    void Update();
    void OnHandDeactivated(size_t f_hand) override;
    void OnButtonPress(size_t f_hand, uint32_t f_button) override;
    void OnDashboardOpen() override;
    void OnDashboardClose() override;
    WidgetKeyboard();
    ~WidgetKeyboard();

    static void InitStaticResources();
    static void RemoveStaticResources();
};
