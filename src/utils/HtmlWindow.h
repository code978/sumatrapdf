/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */
#ifndef HtmlWindow_h
#define HtmlWindow_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include <exdisp.h>

class FrameSite;

class HtmlWindowCallback
{
public:
    virtual bool OnBeforeNavigate(const TCHAR *url) = 0;
};

class HtmlWindow
{
protected:
    friend class FrameSite;

    HWND hwnd;

    IWebBrowser2 *      webBrowser;
    IOleObject *        oleObject;
    IOleInPlaceObject * oleInPlaceObject;
    IViewObject *       viewObject;
    IConnectionPoint *  connectionPoint;
    HWND                oleObjectHwnd;

    DWORD               adviseCookie;

    bool                aboutBlankShown;

    bool                documentLoaded;

    HtmlWindowCallback *htmlWinCb;

    void EnsureAboutBlankShown();
    void CreateBrowser();

public:
    HtmlWindow(HWND hwnd, HtmlWindowCallback *cb);
    ~HtmlWindow();

    void OnSize(int dx, int dy);
    void SetVisible(bool visible);
    void NavigateToUrl(const TCHAR *url);
    void DisplayHtml(const TCHAR *html);
    void DisplayChmPage(const TCHAR *chmFilePath, const TCHAR *chmPage);
    void GoBack();
    void GoForward();
    bool WaitUntilLoaded(DWORD maxWaitMs);

    HBITMAP TakeScreenshot(RectI area);
    bool OnBeforeNavigate(const TCHAR *url);
    void OnDocumentComplete(const TCHAR *url);
};
#endif
