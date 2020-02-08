#pragma once

class Transformation;
class Widget
{
    Widget(const Widget &that) = delete;
    Widget& operator=(const Widget &that) = delete;
protected:
    static vr::IVROverlay *ms_vrOverlay;
    static vr::IVRCompositor *ms_vrCompositor;
    vr::VROverlayHandle_t m_overlayHandle;
    vr::Texture_t m_vrTexture;

    bool m_valid;
    bool m_visible;

    unsigned char m_language;
    Transformation *m_transform;

    Widget();
    virtual ~Widget();

    virtual bool Create() = 0;
    virtual void Destroy() = 0;
    virtual void Cleanup() = 0;
    virtual void Update() = 0;

    virtual bool CloseRequested() const = 0;

    virtual void OnHandActivated(unsigned char f_hand) {}
    virtual void OnHandDeactivated(unsigned char f_hand) {}
    virtual void OnButtonPress(unsigned char f_hand, uint32_t f_button) {}
    virtual void OnButtonRelease(unsigned char f_hand, uint32_t f_button) {}
    virtual void OnDashboardOpen() {}
    virtual void OnDashboardClose() {}
    virtual void OnLanguageChange(unsigned char f_lang);

    static void SetInterfaces(vr::IVROverlay *f_overlay, vr::IVRCompositor *f_compositor);

    friend class WidgetManager;
};
