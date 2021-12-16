#include "stdafx.h"

#include "Widgets/WidgetKeyboard.h"
#include "Gui/GuiSystem.h"
#include "Gui/GuiButton.h"
#include "Utils/Transformation.h"

#include "Core/VRDevicesStates.h"
#include "Managers/ConfigManager.h"
#include "Gui/GuiStructures.h"
#include "Utils/VROverlay.h"
#include "Utils/Utils.h"

// die
#include "Managers/WidgetManager.h"
#include "Utils/VRDashOverlay.h"

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

extern const float g_piHalf;
extern const glm::vec3 g_axisX;
extern const glm::vec3 g_axisZN;
extern const unsigned char g_dummyTextureData[];

// gap of 74 on X
// TODO: should be set in the keyboard xml file, smh
//const sf::Vector2u g_targetSize(1594U, 450U);
const sf::Vector2u g_targetSize(1294U, 450U);

const sf::Color g_hoverColor(142U, 205U, 246U);
const sf::Color g_toggleColor(52U, 164U, 239U);
const sf::Color g_bgColor(127U, 127U, 127U, 255U);

sf::Texture *WidgetKeyboard::ms_textureAtlas = nullptr;

WidgetKeyboard::WidgetKeyboard()
{
    //m_size.x = 0.5f;
    //m_size.y = 0.25f;

    //m_size.x = 0.625f;
    //m_size.y = 0.3125f;

    m_size.x = 0.5f * 1.65f;
    m_size.y = 0.25f * 1.65f;

    m_guiSystem = nullptr;
    m_activeMove = false;
    m_activePin = false;
    m_lastTriggerTick = 0U;
}
WidgetKeyboard::~WidgetKeyboard()
{
}


#define PITCH 0  // up / down
#define YAW 1    // left / right
#define ROLL 2   // fall over on your side and start crying about it https://cdn.discordapp.com/attachments/282679155345195018/895458645763031061/walterdeath.gif


// very cool
// https://stackoverflow.com/questions/55424746/is-there-an-analogous-function-to-vsnprintf-that-works-with-stdstring
std::string vstring( const char* format, ... )
{
    std::string result;
    va_list args, args_copy;

    va_start( args, format );
    va_copy( args_copy, args );

    int len = vsnprintf( nullptr, 0, format, args );
    if (len < 0)
    {
        va_end(args_copy);
        va_end(args);
        throw std::runtime_error("vsnprintf error");
    }

    if ( len > 0 )
    {
        result.resize( len );
        vsnprintf( &result[0], len+1, format, args_copy );
    }

    va_end( args_copy );
    va_end( args );

    return result;
}

std::string Vec2Str( const glm::vec3& in )
{
    return vstring("(%.4f, %.4f, %.4f)", in.x, in.y, in.z);
}

std::string Quat2Str( const glm::quat& in )
{
    return vstring("(%.4f, %.4f, %.4f, %.4f)", in.x, in.y, in.z, in.w);
}


inline glm::quat AngToQuat( const glm::vec3& ang )
{
    return glm::toQuat( glm::yawPitchRoll( ang[1], ang[0], ang[2] ) );
}

// degress to quat in radians
inline glm::quat AngDToQuat( const glm::vec3& ang )
{
    return glm::quat( glm::radians(ang) );
}

// quat to vec3
inline glm::vec3 QuatToAng( const glm::quat& quat )
{
    return glm::eulerAngles( quat );
}

// quat in radians to vec3 in degrees
inline glm::vec3 QuatToAngDeg( const glm::quat& quat )
{
    return glm::degrees( glm::eulerAngles( quat ) );
}

template <class T>
size_t vec_index(const std::vector<T> &vec, T item)
{
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (vec[i] == item)
            return i;
    }

    return SIZE_MAX;
}


template <class T>
void vec_remove(std::vector<T> &vec, T item)
{
    vec.erase(vec.begin() + vec_index(vec, item));
}


void WidgetKeyboard::UpdateTransform()
{
    // Create overlay under the dashboard
    glm::vec3 l_hmdPos;
    glm::quat l_hmdRot;

    //VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_Hmd, l_hmdPos);
    //VRDevicesStates::GetDeviceRotation(VRDeviceIndex::VDI_Hmd, l_hmdRot);
    widgetmanager->GetHmdTransformOnDashOpen( l_hmdPos, l_hmdRot );

    // glm::vec3 tmpPos(-0.2493, 1.2965, 0.4893);
    //glm::vec3 tmpPos(0, 1.2965, 0.4893);
    glm::vec3 posOffset(0, -0.8, 0);
    // glm::vec3 tmpAng(2.2816, 0.6246, -3.1404);
    //glm::vec3 tmpAng(130, 35, -180);
    // glm::vec3 tmpAng(130, 35, 0);

    const float DIST = 0.775f;

    glm::vec3 l_pos = l_hmdPos + (l_hmdRot*g_axisZN)*DIST;

    // glm::vec3 hmdAng = QuatToAngDeg(l_hmdRot);
    glm::vec3 hmdAng = {0, QuatToAngDeg(l_hmdRot).y, 0};
    //hmdAng = {0, hmdAng.y, 0};
    l_hmdRot = glm::radians(hmdAng);

    // hack cause idk what im doing
    l_pos.y = (l_hmdPos + (l_hmdRot*g_axisZN)*DIST).y;
    // glm::vec3 l_pos = l_hmdPos + (forward * 2.f);
    //glm::vec3 l_pos = l_hmdPos;

    //glm::vec3 outPos = l_pos;
    glm::vec3 outPos = l_pos + posOffset;
    //glm::vec3 outPos = l_pos + (posOffset + (l_hmdRot*g_axisZN)*0.5f);
    m_overlayMain->GetTransform()->SetPosition( outPos );

    glm::quat l_rot;
    GetRotationToPoint( l_hmdPos, outPos, l_hmdRot, l_rot );

    glm::vec3 outAng = QuatToAngDeg(l_rot);
    glm::quat outRot = glm::radians(outAng);

    m_overlayMain->GetTransform()->SetRotation( l_rot );

    printf("Keyboard Created At: Pos: %s - Ang: %s\n", Vec2Str(outPos).c_str(), Quat2Str(outRot).c_str() );
}


bool WidgetKeyboard::Create()
{
    if(!m_valid)
    {
        std::string l_overlayKeyPart("ovrw.keyboard_");
        l_overlayKeyPart.append(std::to_string(reinterpret_cast<size_t>(this)));

        std::string l_overlayKeyFull(l_overlayKeyPart);
        l_overlayKeyFull.append(".main");

        if(m_overlayMain->Create(l_overlayKeyFull.c_str(), "OpenVR Widgets - Keyboard - Main"))
        {
            UpdateTransform();

            m_overlayMain->SetInputMethod(vr::VROverlayInputMethod_Mouse);
            m_overlayMain->SetMouseScale(static_cast<float>(g_targetSize.x), static_cast<float>(g_targetSize.y));
            m_overlayMain->SetWidth(m_size.x);
        }

        m_guiSystem = new GuiSystem(g_targetSize);
        if(m_guiSystem->IsValid())
        {
            m_guiSystem->SetFont(ConfigManager::GetGuiFont());

            struct ModKeyThing
            {
                KeyboardKey* key;
                std::string modText;
                std::string modValue;
            };

            std::vector<ModKeyThing> tmpKeyList;

            const auto l_clickCallback = std::bind(&WidgetKeyboard::OnGuiElementClick_Keys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

            pugi::xml_document *l_layout = new pugi::xml_document();
            if(l_layout->load_file(ConfigManager::GetKeyboardLayout().c_str()))
            {
                pugi::xml_node l_root = l_layout->child("layout");
                if(l_root)
                {
                    for(pugi::xml_node l_keyNode = l_root.child("key"); l_keyNode; l_keyNode = l_keyNode.next_sibling("key"))
                    {
                        pugi::xml_attribute l_attribText = l_keyNode.attribute("text");
                        //pugi::xml_attribute l_attribAltText = l_keyNode.attribute("altText");
                        pugi::xml_attribute l_attribCode = l_keyNode.attribute("code");
                        pugi::xml_attribute l_attribTransform = l_keyNode.attribute("transform");

                        pugi::xml_attribute l_attribMod = l_keyNode.attribute("mod");  // is modifier key?

                        // text for when certain modifiers are active
                        //// format for lshift and A: modText="LShift A;RShift A"
                        // format for lshift or rshift and A: modText="LShift|RShift A"
                        //// format for lshift or rshift and A: modText="160|161 A|65"
                        pugi::xml_attribute l_attribModText = l_keyNode.attribute("modText");  

                        if(l_attribText && l_attribCode && l_attribTransform)
                        {
                            KeyboardKey *l_keyboardKey = new KeyboardKey();
                            m_keys.push_back( l_keyboardKey );

                            l_keyboardKey->m_text.assign(l_attribText.as_string("?"));
                            l_keyboardKey->m_code = l_attribCode.as_uint(0U);

                            if ( l_attribMod )
                            {
                                unsigned int modValue = l_attribMod.as_uint(); 

                                if ( modValue == 1 )
                                    l_keyboardKey->m_modType = KeyModType::Hold;

                                else if ( modValue == 2 )
                                    l_keyboardKey->m_modType = KeyModType::Toggle;

                                m_modKeys.push_back( l_keyboardKey );
                            }

                            if ( l_attribModText )
                            {
                                std::string rawText = l_attribModText.as_string();
                                std::vector<std::string> keys;
                                std::string value;

                                std::string cur;
                                bool inValue = false;
                                for (char const &c: rawText)
                                {
                                    if ( c == '|' && !inValue )
                                    {
                                        keys.push_back(cur);
                                        cur = "";
                                    }

                                    else if ( c == ' ' && !inValue )
                                    {
                                        keys.push_back(cur);
                                        cur = "";
                                        inValue = true;
                                    }
                                    else
                                    {
                                        if ( inValue )
                                            value += c;
                                        else
                                            cur += c;
                                    }
                                }

                                // HACK:
                                if ( value == "&quot" )
                                    value = "\"";

                                for (auto& modKey : keys)
                                    tmpKeyList.push_back( {l_keyboardKey, modKey, value} );
                            }

                            GuiButton *l_guiButton = m_guiSystem->CreateButton();
                            l_guiButton->SetUserPointer(l_keyboardKey);
                            l_keyboardKey->m_guiButton = l_guiButton;

                            std::stringstream l_transformStream(l_attribTransform.as_string("0 0 0 0"));
                            glm::uvec4 l_transform(0U);
                            l_transformStream >> l_transform[0] >> l_transform[1] >> l_transform[2] >> l_transform[3];
                            l_guiButton->SetPosition(sf::Vector2f(static_cast<float>(l_transform[0]),static_cast<float>(l_transform[1])));
                            l_guiButton->SetSize(sf::Vector2f(static_cast<float>(l_transform[2]), static_cast<float>(l_transform[3])));

                            l_guiButton->SetText(l_keyboardKey->m_text);
                            l_guiButton->SetTextSize(24U);
                            l_guiButton->SetSelectionColor(g_hoverColor);
                            l_guiButton->SetVisibility(true);
                            l_guiButton->SetClickCallback(l_clickCallback);

                            m_guiButtons.push_back(l_guiButton);
                        }
                    }
                }
            }
            delete l_layout;

            for ( auto& tmpKey : tmpKeyList )
            {
                if ( tmpKey.key->m_modText.find( tmpKey.modValue ) == tmpKey.key->m_modText.end() )
                    tmpKey.key->m_modText[tmpKey.modValue] = {};

                for ( auto& modKey : m_modKeys )
                {
                    if ( modKey->m_text == tmpKey.modText )
                    {
                        tmpKey.key->m_modText[tmpKey.modValue].push_back(modKey);
                        break;
                    }
                }
            }

#ifdef _WIN32
            int l_layoutsCount = GetKeyboardLayoutList(0,nullptr);
            glm::clamp(l_layoutsCount, 0, 2);
            if(l_layoutsCount > 0)
            {
                m_systemLayouts.assign(static_cast<size_t>(l_layoutsCount), nullptr);
                GetKeyboardLayoutList(l_layoutsCount, m_systemLayouts.data());
            }
#endif

            m_overlayMain->SetTexture(m_guiSystem->GetRenderTextureHandle());
        }

        m_valid = (m_overlayMain->IsValid() && m_guiSystem->IsValid());
        if(m_valid) m_overlayMain->Show();

        m_visible = m_valid;
    }

    return m_valid;
}

void WidgetKeyboard::Destroy()
{
    if(m_guiSystem)
    {
        for(auto l_button : m_guiButtons)
        {
            delete reinterpret_cast<KeyboardKey*>(l_button->GetUserPointer());
            m_guiSystem->Remove(l_button);
        }
        m_guiButtons.clear();

        delete m_guiSystem;
        m_guiSystem = nullptr;
    }

    m_activeMove = false;
    m_activePin = false;
    m_lastTriggerTick = 0U;

    Widget::Destroy();
}

void WidgetKeyboard::Update()
{
    if(m_valid && m_visible)
    {
        CheckToggleKeys();

        while(m_overlayMain->Poll(m_event))
        {
            switch(m_event.eventType)
            {
                case vr::VREvent_MouseButtonDown: case vr::VREvent_MouseButtonUp:
                {
                    unsigned char l_button = 0U;
                    switch(m_event.data.mouse.button)
                    {
                        case vr::VRMouseButton_Left:
                            l_button = GuiClick::GC_Left;
                            break;
                        case vr::VRMouseButton_Right:
                            l_button = GuiClick::GC_Right;
                            break;
                        case vr::VRMouseButton_Middle:
                            l_button = GuiClick::GC_Middle;
                            break;
                    }
                    const unsigned char l_state = ((m_event.eventType == vr::VREvent_MouseButtonDown) ? GuiClickState::GCS_Press : GuiClickState::GCS_Release);
                    m_guiSystem->ProcessClick(l_button, l_state, static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(m_event.data.mouse.y));
                } break;
                case vr::VREvent_MouseMove:
                    m_guiSystem->ProcessMove(static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(m_event.data.mouse.y));
                    break;
            }
        }

        if(m_activePin)
        {
            glm::quat l_handRot;
            VRDevicesStates::GetDeviceRotation(VRDeviceIndex::VDI_LeftController, l_handRot);
            const glm::quat l_rot = glm::rotate(l_handRot, -g_piHalf, g_axisX);
            m_overlayMain->GetTransform()->SetRotation(l_rot);

            glm::vec3 l_handPos;
            VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_LeftController, l_handPos);
            m_overlayMain->GetTransform()->SetPosition(l_handPos);
        }

        m_guiSystem->Update();
        m_overlayMain->Update();

        if(m_activeDashboard)
        {
            //UpdateTransform();
        }
    }
}

void WidgetKeyboard::OnHandDeactivated(size_t f_hand)
{
    Widget::OnHandDeactivated(f_hand);

    if(m_valid)
    {
        if((f_hand == VRDeviceIndex::VDI_LeftController) && m_activeMove) m_activeMove = false;
    }
}

void WidgetKeyboard::OnButtonPress(size_t f_hand, uint32_t f_button)
{
    Widget::OnButtonPress(f_hand, f_button);
}

void WidgetKeyboard::OnDashboardOpen()
{
    Widget::OnDashboardOpen();

    if(m_valid)
    {
        UpdateTransform();

        if(!m_activePin) m_overlayMain->Show();
    }
}
void WidgetKeyboard::OnDashboardClose()
{
    Widget::OnDashboardClose();

    if(m_valid)
    {
        if(!m_activePin) m_overlayMain->Hide();
    }
}


void WidgetKeyboard::OnGuiElementClick_Keys(GuiElement *f_guiElement, unsigned char f_button, unsigned char f_state)
{
    if((f_button == GuiClick::GC_Left) && (f_state == GuiClickState::GCS_Press))
    {
        KeyboardKey *key = reinterpret_cast<KeyboardKey*>(f_guiElement->GetUserPointer());
        if(!key)
            return;

        // don't actually press these modifier keys
        if ( key->m_modType == KeyModType::Hold )
        {
            key->m_modActive = !key->m_modActive;
            UpdateKeys();
            return;
        }

        INPUT input;
        INPUT inputArray[4];
        input.ki.dwExtraInfo = 0;
        input.ki.time = 0;
        input.type = INPUT_KEYBOARD;

        bool hasMods = false;
        // releases hold modifier keys?
        if ( key->m_modType == KeyModType::None )
        {
            for ( auto& modKey: m_modKeys )
            {
                if ( modKey->m_modType != KeyModType::Hold || !modKey->m_modActive )
                    continue;

                hasMods = true;

                input.ki.wScan = 0;
                input.ki.wVk = modKey->m_code;
                input.ki.dwFlags = 0;
                inputArray[0] = input;

                input.ki.wScan = 0;
                input.ki.wVk = modKey->m_code;
                input.ki.dwFlags = KEYEVENTF_KEYUP;
                inputArray[3] = input;

                // only one mod for now lol
                break;
            }
        }

        input.ki.wScan = 0;
        input.ki.wVk = key->m_code;
        input.ki.dwFlags = 0;
        inputArray[hasMods ? 1 : 0] = input;

        input.ki.wScan = 0;
        input.ki.wVk = key->m_code;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        inputArray[hasMods ? 2 : 1] = input;

        SendInput(hasMods ? 4U : 2U, inputArray, sizeof(INPUT));

        UpdateKeys();
    }
}

void WidgetKeyboard::UpdateKeys()
{
    // for (auto key: m_keys)
    for ( auto& guiBtn: m_guiButtons )
    {
        KeyboardKey *key = reinterpret_cast<KeyboardKey*>(guiBtn->GetUserPointer());
        if(!key)
            continue;

        bool modActive = false;

        // GOD my code here sucks
        bool shiftEnabled = false;
        bool capsEnabled = false;

        // i hate that this is vs2013, can't even use c++17 features, god
        for (auto const& kv : key->m_modText)
        {
            auto& value = kv.first;
            auto& modKeys = kv.second;

            for (auto const& modKey: modKeys)
            {
                if (modKey->m_modActive)
                {
                    modActive = true;

                    if ( modKey->m_code == VK_CAPITAL )
                        capsEnabled = true;

                    if ( modKey->m_code == VK_LSHIFT || modKey->m_code == VK_RSHIFT )
                        shiftEnabled = true;
                }
            }

            if ( modActive && !(shiftEnabled && capsEnabled) )
            {
                guiBtn->SetText( value );
            }
            else
            {
                guiBtn->SetText( key->m_text );
            }
        }

        if ( key->m_modType == KeyModType::Hold )
        {
            if ( key->m_modActive )
            {
                guiBtn->SetBackgroundColor( g_toggleColor );
            }
            else
            {
                guiBtn->SetBackgroundColor( g_bgColor );
            }
        }
    }
}


void WidgetKeyboard::CheckToggleKeys()
{
    for ( auto& key: m_modKeys )
    {
        if ( key->m_modType == KeyModType::Toggle )
        {
            bool prevState = key->m_modActive;

            // key->m_modActive = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);
            key->m_modActive = ((GetKeyState( key->m_code ) & 0x0001) != 0);

            if ( key->m_modActive )
            {
                key->m_guiButton->SetBackgroundColor( g_toggleColor );
            }
            else
            {
                key->m_guiButton->SetBackgroundColor( g_bgColor );
            }

            if ( prevState != key->m_modActive )
                UpdateKeys();
        }
    }
}


void WidgetKeyboard::InitStaticResources()
{
    if(!ms_textureAtlas)
    {
        ms_textureAtlas = new sf::Texture();
        if(!ms_textureAtlas->loadFromFile("icons/atlas_keyboard.png")) ms_textureAtlas->loadFromMemory(g_dummyTextureData, 16U);
    }
}

void WidgetKeyboard::RemoveStaticResources()
{
    if(ms_textureAtlas)
    {
        delete ms_textureAtlas;
        ms_textureAtlas = nullptr;
    }
}
