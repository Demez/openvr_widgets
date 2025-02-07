#include "stdafx.h"

#include "Widgets/WidgetWindowCapture.h"
#include "Gui/GuiSystem.h"
#include "Gui/GuiImage.h"
#include "Gui/GuiText.h"
#include "Utils/Transformation.h"
#include "Utils/VROverlay.h"
#include "Utils/WindowCapturer.h"

#include "Core/VRDevicesStates.h"
#include "Managers/ConfigManager.h"
#include "Gui/GuiStructures.h"
#include "Utils/Utils.h"

extern const float g_piHalf;
extern const glm::ivec2 g_emptyIVector2;
extern const glm::vec3 g_axisX;
extern const glm::vec3 g_axisZN;
extern const sf::Color g_clearColor;
extern const unsigned char g_dummyTextureData[];

const size_t g_captureDelays[3U]
{
    66U, 33U, 16U
};
const sf::Vector2u g_guiSystemDefaultSize(256U, 448U);
const sf::Vector2i g_guiButtonsDefaultSize(128, 128);
const sf::Vector2f g_guiButtonsInitialPositions[6U]
{
    { 0.f, 64.f },
    { 128.f, 64.f },
    { 0.f, 192.f },
    { 128.f, 192.f },
    { 0.f, 320.f },
    { 128.f, 320.f }
};
const sf::Vector2i g_guiButtonsInitialUV[6U]
{
    { 0, 0 },
    { 256, 0 },
    { 384, 0 },
    { 0, 128 },
    { 128, 128 },
    { 256, 128 },
};
const sf::Vector2i g_guiButtonPinUV[2U]
{
    { 0, 0 },
    { 128, 0 }
};
const sf::Vector2i g_guiButtonFpsUV[3U]
{
    {256, 128 },
    { 384, 128 },
    { 0, 256 }
};

const glm::vec3 g_tintColorFar(0.f);
const glm::vec3 g_tintColorNear(0.f, 0.658823f, 0.917647f);
const float g_backgroundScale = 1.025f;

sf::Texture *WidgetWindowCapture::ms_textureAtlas = nullptr;
#ifdef __linux__
Display *WidgetWindowCapture::ms_display = nullptr;
#endif

WidgetWindowCapture::WidgetWindowCapture()
{
    m_overlayBackground = new VROverlay();
    m_overlayControls = new VROverlay();

    m_windowCapturer = nullptr;
    m_windowIndex = std::numeric_limits<size_t>::max();

    m_guiSystem = nullptr;
    for(size_t i = 0U; i < CEI_Count; i++) m_guiImages[i] = nullptr;
    m_guiTextWindow = nullptr;

    m_lastLeftTriggerTick = 0U;
    m_lastRightTriggerTick = 0U;

    m_activeMove = false;
    m_activeResize = false;
    m_activePin = false;

    m_windowRatio = 0.f;
    m_mousePosition = g_emptyIVector2;

    m_fpsMode = FM_15;
}
WidgetWindowCapture::~WidgetWindowCapture()
{
    delete m_overlayBackground;
    delete m_overlayControls;
}

bool WidgetWindowCapture::Create()
{
    if(!m_valid)
    {
        m_size.x = 0.5f;

        std::string l_overlayKeyPart("ovrw.capture_");
        l_overlayKeyPart.append(std::to_string(reinterpret_cast<size_t>(this)));
        std::string l_overlayKeyFull(l_overlayKeyPart);

        l_overlayKeyFull.append(".main");
        if(m_overlayMain->Create(l_overlayKeyFull, "OpenVR Widgets - Capture - Main"))
        {
            // Create overlay in front of user
            glm::vec3 l_hmdPos;
            glm::quat l_hmdRot;
            VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_Hmd, l_hmdPos);
            VRDevicesStates::GetDeviceRotation(VRDeviceIndex::VDI_Hmd, l_hmdRot);

            glm::vec3 l_pos = l_hmdPos + (l_hmdRot*g_axisZN)*0.5f;
            m_overlayMain->GetTransform()->SetPosition(l_pos);

            glm::quat l_rot;
            GetRotationToPoint(l_hmdPos, l_pos, l_hmdRot, l_rot);
            m_overlayMain->GetTransform()->SetRotation(l_rot);

            m_overlayMain->SetWidth(m_size.x);
            m_overlayMain->SetFlag(vr::VROverlayFlags_SortWithNonSceneOverlays, true);
            m_overlayMain->SetFlag(vr::VROverlayFlags_ProtectedContent, true);
            m_overlayMain->SetInputMethod(vr::VROverlayInputMethod_Mouse);
            m_overlayMain->SetFlag(vr::VROverlayFlags_ShowTouchPadScrollWheel, true);
            m_overlayMain->SetFlag(vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
        }

        l_overlayKeyFull.assign(l_overlayKeyPart);
        l_overlayKeyFull.append(".background");
        if(m_overlayBackground->Create(l_overlayKeyFull, "OpenVR Widgets - Capture - Background"))
        {
            m_overlayBackground->GetTransform()->SetPosition(glm::vec3(0.f, 0.f, -0.005f));
            m_overlayBackground->SetWidth(m_size.x*g_backgroundScale);
        }

        l_overlayKeyFull.assign(l_overlayKeyPart);
        l_overlayKeyFull.append(".control");
        if(m_overlayControls->Create(l_overlayKeyFull, "OpenVR Widgets - Capture - Control"))
        {
            m_guiSystem = new GuiSystem(g_guiSystemDefaultSize);
            m_guiSystem->SetFont(ConfigManager::GetGuiFont());

            const auto l_clickCallback = std::bind(&WidgetWindowCapture::OnGuiElementMouseClick, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            for(size_t i = 0U; i < CEI_Count; i++)
            {
                m_guiImages[i] = m_guiSystem->CreateImage(ms_textureAtlas);
                m_guiImages[i]->SetPosition(g_guiButtonsInitialPositions[i]);
                m_guiImages[i]->SetSize(sf::Vector2f(g_guiButtonsDefaultSize));
                m_guiImages[i]->SetUV(g_guiButtonsInitialUV[i], g_guiButtonsDefaultSize);
                m_guiImages[i]->SetUserPointer(reinterpret_cast<void*>(i));
                m_guiImages[i]->SetClickCallback(l_clickCallback);
            }
            m_guiTextWindow = m_guiSystem->CreateText();
            m_guiTextWindow->Set("<>");
            m_guiTextWindow->SetCharactersSize(16U);
            m_guiTextWindow->SetAlignment(GuiText::GTA_Center);
            m_guiTextWindow->SetPosition(sf::Vector2f(128.f, 32.f));

            m_overlayControls->SetTexture(m_guiSystem->GetRenderTextureHandle());
            m_overlayControls->GetTransform()->SetPosition(glm::vec3(m_size.x * 0.5f + 0.072f, 0.f, 0.f));

            m_overlayControls->SetMouseScale(static_cast<float>(g_guiSystemDefaultSize.x), static_cast<float>(g_guiSystemDefaultSize.y));
            m_overlayControls->SetFlag(vr::VROverlayFlags_SortWithNonSceneOverlays, true);
            m_overlayControls->SetInputMethod(vr::VROverlayInputMethod_Mouse);
            m_overlayControls->SetWidth(0.128f);
        }

        m_valid = (m_overlayMain->IsValid() && m_overlayBackground->IsValid() && m_overlayControls->IsValid() && m_guiSystem->IsValid());
        if(m_valid)
        {
            // Create window grabber and start capture
            m_windowCapturer = new WindowCapturer();
            m_windowCapturer->UpdateWindows();
            m_windowIndex = 0U;
            StartCapture();

            m_overlayMain->Show();
            m_overlayBackground->Show();
            m_overlayControls->Show();

            m_visible = true;
        }
    }

    return m_valid;
}

void WidgetWindowCapture::Destroy()
{
    m_overlayBackground->Destroy();
    m_overlayControls->Destroy();

    delete m_guiSystem;
    m_guiSystem = nullptr;
    for(size_t i = 0U; i < CEI_Count; i++) m_guiImages[i] = nullptr;
    m_guiTextWindow = nullptr;

    delete m_windowCapturer;
    m_windowCapturer = nullptr;

    Widget::Destroy();
}

void WidgetWindowCapture::Update()
{
    if(m_valid && m_visible)
    {
        // Poll overlays interaction
        while(m_overlayMain->Poll(m_event))
        {
            switch(m_event.eventType)
            {
                case vr::VREvent_MouseMove:
                {
                    m_mousePosition.x = static_cast<int>(m_event.data.mouse.x);
                    m_mousePosition.y = static_cast<int>(m_event.data.mouse.y);
                } break;

                case vr::VREvent_MouseButtonDown:
                {
                    const auto *l_window = m_windowCapturer->GetWindowInfo(m_windowIndex);
                    if(l_window)
                    {
#ifdef _WIN32
                        if(IsWindow(reinterpret_cast<HWND>(l_window->Handle)))
                        {
                            DWORD l_buttonData[3] = { 0 };
                            switch(m_event.data.mouse.button)
                            {
                                case vr::VRMouseButton_Left:
                                {
                                    l_buttonData[0] = MK_LBUTTON;
                                    l_buttonData[1] = WM_LBUTTONDOWN;
                                    l_buttonData[2] = WM_LBUTTONUP;
                                } break;
                                case vr::VRMouseButton_Right:
                                {
                                    l_buttonData[0] = MK_RBUTTON;
                                    l_buttonData[1] = WM_RBUTTONDOWN;
                                    l_buttonData[2] = WM_RBUTTONUP;
                                } break;
                                case vr::VRMouseButton_Middle:
                                {
                                    l_buttonData[0] = MK_MBUTTON;
                                    l_buttonData[1] = WM_MBUTTONDOWN;
                                    l_buttonData[2] = WM_MBUTTONUP;
                                } break;
                            }

                            SendMessage(reinterpret_cast<HWND>(l_window->Handle), WM_MOUSEMOVE, NULL, MAKELPARAM(m_mousePosition.x, m_mousePosition.y));
                            SendMessage(reinterpret_cast<HWND>(l_window->Handle), l_buttonData[1], l_buttonData[0], MAKELPARAM(m_mousePosition.x, m_mousePosition.y));
                            SendMessage(reinterpret_cast<HWND>(l_window->Handle), l_buttonData[2], NULL, MAKELPARAM(m_mousePosition.x, m_mousePosition.y));
                        }
#elif __linux__
                        if(ms_display)
                        {
                            XEvent l_event = { 0 };

                            switch(m_event.data.mouse.button)
                            {
                                case vr::VRMouseButton_Left:
                                    l_event.xbutton.button = Button1;
                                    break;
                                case vr::VRMouseButton_Right:
                                    l_event.xbutton.button = Button3;
                                    break;
                                case vr::VRMouseButton_Middle:
                                    l_event.xbutton.button = Button2;
                                    break;
                            }
                            l_event.xbutton.same_screen = true;
                            l_event.xbutton.window = l_window->Handle;
                            l_event.xbutton.x = m_mousePosition.x;
                            l_event.xbutton.y = m_mousePosition.y;

                            l_event.type = ButtonPress;
                            XSendEvent(ms_display, l_window->Handle, true, ButtonPressMask, &l_event);
                            XFlush(ms_display);

                            l_event.type = ButtonRelease;
                            XSendEvent(ms_display, l_window->Handle, true, ButtonReleaseMask, &l_event);
                            XFlush(ms_display);
                        }
#endif
                    }
                } break;

                case vr::VREvent_ScrollDiscrete:
                {
                    const auto *l_window = m_windowCapturer->GetWindowInfo(m_windowIndex);
                    if(l_window)
                    {
#ifdef _WIN32
                        if(IsWindow(reinterpret_cast<HWND>(l_window->Handle)))
                        {
                            SendMessage(reinterpret_cast<HWND>(l_window->Handle), WM_MOUSEMOVE, NULL, MAKELPARAM(m_mousePosition.x, m_mousePosition.y));
                            SendMessage(reinterpret_cast<HWND>(l_window->Handle), WM_MOUSEWHEEL, MAKEWPARAM(NULL, m_event.data.scroll.ydelta * WHEEL_DELTA), MAKELPARAM(m_mousePosition.x, m_mousePosition.y));
                        }
#elif __linux__
                        if(ms_display)
                        {
                            XEvent l_event = { 0 };

                            l_event.xbutton.button = (m_event.data.scroll.ydelta < 0.f) ? Button5 : Button4;
                            l_event.xbutton.same_screen = true;
                            l_event.xbutton.window = l_window->Handle;
                            l_event.xbutton.x = m_mousePosition.x;
                            l_event.xbutton.y = m_mousePosition.y;

                            l_event.type = ButtonPress;
                            XSendEvent(ms_display, l_window->Handle, true, ButtonPressMask, &l_event);
                            XFlush(ms_display);

                            l_event.type = ButtonRelease;
                            XSendEvent(ms_display, l_window->Handle, true, ButtonReleaseMask, &l_event);
                            XFlush(ms_display);
                        }
#endif
                    }
                } break;
            }
        }

        while(m_overlayControls->Poll(m_event))
        {
            switch(m_event.eventType)
            {
                case vr::VREvent_MouseMove:
#ifdef _WIN32
                    m_guiSystem->ProcessMove(static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(m_event.data.mouse.y));
#elif __linux__
                    m_guiSystem->ProcessMove(static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(g_guiSystemDefaultSize.y - m_event.data.mouse.y));
#endif
                    break;

                case vr::VREvent_MouseButtonDown:
                {
                    if(m_event.data.mouse.button == vr::VRMouseButton_Left)
                    {
#ifdef _WIN32
                        m_guiSystem->ProcessClick(GuiClick::GC_Left, GuiClickState::GCS_Press, static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(m_event.data.mouse.y));
#elif __linux__
                        m_guiSystem->ProcessClick(GuiClick::GC_Left, GuiClickState::GCS_Press, static_cast<unsigned int>(m_event.data.mouse.x), static_cast<unsigned int>(g_guiSystemDefaultSize.y - m_event.data.mouse.y));
#endif
                    }
                } break;
            }
        }

        if(m_activeMove)
        {
            glm::quat l_handRot;
            VRDevicesStates::GetDeviceRotation(VRDeviceIndex::VDI_LeftController, l_handRot);
            const glm::quat l_rot = glm::rotate(l_handRot, -g_piHalf, g_axisX);
            m_overlayMain->GetTransform()->SetRotation(l_rot);

            glm::vec3 l_handPos;
            VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_LeftController, l_handPos);
            m_overlayMain->GetTransform()->SetPosition(l_handPos);
        }

        if(m_activeResize)
        {
            glm::vec3 l_handPos;
            VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_RightController, l_handPos);
            m_size.x = (glm::distance(l_handPos, m_overlayMain->GetTransform()->GetPosition()) * 2.f);
            m_size.y = m_size.x / m_windowRatio;
            m_overlayControls->GetTransform()->SetPosition(glm::vec3(m_size.x * 0.5f + 0.072f, 0.f, 0.f));
            m_overlayMain->SetWidth(m_size.x);
            m_overlayBackground->SetWidth(m_size.x*g_backgroundScale);
        }

        // Set background based on distance to left hand if not pinned
        if(!m_activePin)
        {
            glm::vec3 l_handPos;
            VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_LeftController, l_handPos);
            float l_distance = glm::distance(l_handPos, m_overlayMain->GetTransform()->GetPosition());
            const float l_limit = m_size.x * 0.2f;
            l_distance = glm::clamp(l_distance, l_limit, l_limit + 0.1f);
            const glm::vec3 l_newTint = glm::mix(g_tintColorNear, g_tintColorFar, (l_distance - l_limit)*10.f);
            m_overlayBackground->SetColor(l_newTint.r, l_newTint.g, l_newTint.b);
            m_overlayBackground->SetAlpha(1.f - (l_distance - l_limit)*2.5f);
        }

        if(m_windowCapturer->IsStale())
        {
            // Window was resized or destroyed
            m_windowCapturer->StopCapture();
            m_windowCapturer->UpdateWindows();
            m_windowIndex = 0U;
            StartCapture();
        }
        m_windowCapturer->Update();

        m_overlayMain->Update();
        m_overlayBackground->Update(m_overlayMain);
        if(m_activeDashboard) m_guiSystem->Update();
        m_overlayControls->Update(m_overlayMain);
    }
}

void WidgetWindowCapture::OnButtonPress(size_t f_hand, uint32_t f_button)
{
    Widget::OnButtonPress(f_hand, f_button);

    if(m_valid)
    {
        switch(f_hand)
        {
            case VRDeviceIndex::VDI_LeftController:
            {
                switch(f_button)
                {
                    case vr::k_EButton_SteamVR_Trigger:
                    {
                        if(m_visible && !m_activeDashboard && !m_activePin)
                        {
                            const unsigned long long l_tick = GetTickCount64();
                            if((l_tick - m_lastLeftTriggerTick) < 500U)
                            {
                                if(!m_activeMove)
                                {
                                    glm::vec3 l_pos;
                                    VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_LeftController, l_pos);
                                    m_activeMove = (glm::distance(l_pos, m_overlayMain->GetTransform()->GetPosition()) < (m_size.x * 0.2f));
                                }
                                else m_activeMove = false;
                            }
                            m_lastLeftTriggerTick = l_tick;
                        }
                    } break;
                }
            } break;

            case VRDeviceIndex::VDI_RightController:
            {
                if(m_activeMove && (f_button == vr::k_EButton_SteamVR_Trigger))
                {
                    const unsigned long long l_tick = GetTickCount64();
                    if((l_tick - m_lastRightTriggerTick) < 500U)
                    {
                        glm::vec3 l_pos;
                        VRDevicesStates::GetDevicePosition(VRDeviceIndex::VDI_RightController, l_pos);
                        m_activeResize = (glm::distance(l_pos, m_overlayMain->GetTransform()->GetPosition()) <= (m_size.x * 0.5f));
                    }
                    m_lastRightTriggerTick = l_tick;
                }
            } break;
        }
    }
}

void WidgetWindowCapture::OnButtonRelease(size_t f_hand, uint32_t f_button)
{
    Widget::OnButtonRelease(f_hand, f_button);

    if(m_valid && m_visible)
    {
        if((f_hand == VRDeviceIndex::VDI_RightController) && (f_button == vr::k_EButton_SteamVR_Trigger))
        {
            if(m_activeResize) m_activeResize = false;
        }
    }
}

void WidgetWindowCapture::OnDashboardOpen()
{
    Widget::OnDashboardOpen();

    if(m_valid && m_visible)
    {
        m_activeMove = false;
        m_activeResize = false;

        m_overlayControls->Show();
    }
}
void WidgetWindowCapture::OnDashboardClose()
{
    Widget::OnDashboardClose();

    if(m_valid && m_visible) m_overlayControls->Hide();
}

void WidgetWindowCapture::StartCapture()
{
    if(m_windowCapturer->StartCapture(m_windowIndex))
    {
        m_overlayMain->SetTexture(m_windowCapturer->GetTextureHandle());
        const auto *l_window = m_windowCapturer->GetWindowInfo(m_windowIndex);
        if(l_window)
        {
            m_overlayBackground->SetTexture(l_window->Size.x, l_window->Size.y);
            m_overlayMain->SetMouseScale(static_cast<float>(l_window->Size.x), static_cast<float>(l_window->Size.y));
            m_windowRatio = static_cast<float>(l_window->Size.x) / static_cast<float>(l_window->Size.y);
            m_size.y = m_size.x / m_windowRatio;
#ifdef _WIN32
            wchar_t l_windowName[256U];
            if(GetWindowTextW(reinterpret_cast<HWND>(l_window->Handle), l_windowName, 256) != 0) m_guiTextWindow->Set(l_windowName);
            else m_guiTextWindow->Set("<>");
#elif __linux__
            m_guiTextWindow->Set(l_window->Name);
#endif
        }
    }
    else m_overlayMain->SetTexture(0U);
}

void WidgetWindowCapture::OnGuiElementMouseClick(GuiElement *f_guiElement, unsigned char f_button, unsigned char f_state)
{
    if((f_button == GuiClick::GC_Left) && (f_state == GuiClickState::GCS_Press))
    {
        switch(reinterpret_cast<size_t>(f_guiElement->GetUserPointer()))
        {
            case CEI_Pin:
            {
                m_activePin = !m_activePin;
                if(m_activePin)
                {
                    m_overlayBackground->SetColor(g_tintColorFar.r, g_tintColorFar.g, g_tintColorFar.b);
                    m_overlayBackground->SetAlpha(0.8f);
                }
                m_guiImages[CEI_Pin]->SetUV(g_guiButtonPinUV[m_activePin ? 1U : 0U], g_guiButtonsDefaultSize);
            } break;

            case CEI_Close:
                m_closed = true;
                break;

            case CEI_Previous:
            {
                const size_t l_windowsCount = m_windowCapturer->GetWindowsCount();
                if(l_windowsCount > 0U)
                {
                    m_windowIndex += (l_windowsCount - 1U);
                    m_windowIndex %= l_windowsCount;

                    m_windowCapturer->StopCapture();
                    StartCapture();
                }
            } break;

            case CEI_Next:
            {
                const size_t l_windowsCount = m_windowCapturer->GetWindowsCount();
                if(l_windowsCount > 0U)
                {
                    ++m_windowIndex %= l_windowsCount; // Don't be afraid

                    m_windowCapturer->StopCapture();
                    StartCapture();
                }
            } break;

            case CEI_Update:
            {
                m_windowCapturer->StopCapture();
                m_windowCapturer->UpdateWindows();
                m_windowIndex = 0U;
                StartCapture();
            } break;

            case CEI_FPS:
            {
                ++m_fpsMode %= FM_Count; // Don't be afraid

                m_guiImages[CEI_FPS]->SetUV(g_guiButtonFpsUV[m_fpsMode], g_guiButtonsDefaultSize);
                m_windowCapturer->SetDelay(g_captureDelays[m_fpsMode]);
            } break;
        }
    }
}

// Static
void WidgetWindowCapture::InitStaticResources()
{
#ifdef __linux__
    if(!ms_display) ms_display = XOpenDisplay(nullptr);
#endif

    if(!ms_textureAtlas)
    {
        ms_textureAtlas = new sf::Texture();
        if(!ms_textureAtlas->loadFromFile("icons/atlas_capture.png")) ms_textureAtlas->loadFromMemory(g_dummyTextureData, 16U);
    }

    WindowCapturer::InitStaticResources();
}

void WidgetWindowCapture::RemoveStaticResources()
{
    if(ms_textureAtlas)
    {
        delete ms_textureAtlas;
        ms_textureAtlas = nullptr;
    }

#ifdef __linux__
    if(ms_display)
    {
        XCloseDisplay(ms_display);
        ms_display = nullptr;
    }
#endif

    WindowCapturer::RemoveStaticResources();
}
