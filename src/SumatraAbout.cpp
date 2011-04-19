/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "SumatraAbout.h"
#include "translations.h"
#include "Version.h"
#include "WinUtil.h"

#define ABOUT_LINE_OUTER_SIZE       2
#define ABOUT_LINE_SEP_SIZE         1
#define ABOUT_LEFT_RIGHT_SPACE_DX   8
#define ABOUT_MARGIN_DX            10
#define ABOUT_BOX_MARGIN_DY         6
#define ABOUT_BORDER_COL            RGB(0,0,0)
#define ABOUT_TXT_DY                6
#define ABOUT_RECT_PADDING          8
#define ABOUT_INNER_PADDING         6

#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

#define SUMATRA_TXT_FONT        _T("Arial Black")
#define SUMATRA_TXT_FONT_SIZE   24

#define VERSION_TXT_FONT        _T("Arial Black")
#define VERSION_TXT_FONT_SIZE   12

#define VERSION_TXT             _T("v") CURR_VERSION_STR
#ifdef SVN_PRE_RELEASE_VER
 #define VERSION_SUB_TXT        _T("Pre-release")
#else
 #define VERSION_SUB_TXT        _T("")
#endif

static HWND gHwndAbout;
static HWND gHwndAboutTooltip = NULL;
static const TCHAR *gClickedURL = NULL;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;
    const TCHAR *   url;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    { _T("website"),        _T("SumatraPDF website"),   _T("http://blog.kowalczyk.info/software/sumatrapdf") },
    { _T("forums"),         _T("SumatraPDF forums"),    _T("http://blog.kowalczyk.info/forum_sumatra") },
    { _T("programming"),    _T("Krzysztof Kowalczyk"),  _T("http://blog.kowalczyk.info") },
    { _T("programming"),    _T("Simon B\xFCnzli"),      _T("http://www.zeniko.ch/#SumatraPDF") },
    { _T("programming"),    _T("William Blum"),         _T("http://william.famille-blum.org/") },
#ifdef SVN_PRE_RELEASE_VER
    { _T("a note"),         _T("Pre-release version, for testing only!"), NULL },
#endif
#ifdef DEBUG
    { _T("a note"),         _T("Debug version, for testing only!"), NULL },
#endif
    { _T("pdf rendering"),  _T("MuPDF"),                _T("http://mupdf.com") },
    { _T("program icon"),   _T("Zenon"),                _T("http://www.flashvidz.tk/") },
    { _T("toolbar icons"),  _T("Yusuke Kamiyamane"),    _T("http://p.yusukekamiyamane.com/") },
    { _T("translators"),    _T("The Translators"),      _T("http://blog.kowalczyk.info/software/sumatrapdf/translators.html") },
    { _T("translations"),   _T("Contribute translation"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html") },
    // Note: Must be on the last line, as it's dynamically hidden based on m_enableTeXEnhancements
    { _T("synctex"),        _T("J\xE9rome Laurens"),    _T("http://itexmac.sourceforge.net/SyncTeX.html") },
    { NULL, NULL, NULL }
};

static Vec<StaticLinkInfo> gLinkInfo;

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93,  160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

static void DrawSumatraPDF(HDC hdc, PointI pt)
{
    const TCHAR *txt = APP_NAME_STR;
#ifdef BLACK_ON_YELLOW
    // simple black version
    SetTextColor(hdc, ABOUT_BORDER_COL);
    TextOut(hdc, pt.x, pt.y, txt, Str::Len(txt));
#else
    // colorful version
    COLORREF cols[] = { COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1 };
    for (size_t i = 0; i < Str::Len(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        TextOut(hdc, pt.x, pt.y, txt + i, 1);

        SIZE txtSize;
        GetTextExtentPoint32(hdc, txt + i, 1, &txtSize);
        pt.x += txtSize.cx;
    }
#endif
}

static SizeI CalcSumatraVersionSize(HDC hdc)
{
    SizeI result;

    Win::Font::ScopedFont fontSumatraTxt(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontVersionTxt(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SIZE txtSize;
    /* calculate minimal top box size */
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    result.dy = txtSize.cy + ABOUT_BOX_MARGIN_DY * 2;
    result.dx = txtSize.cx;

    /* consider version and version-sub strings */
    SelectObject(hdc, fontVersionTxt);
    txt = VERSION_TXT;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    int minWidth = txtSize.cx;
    txt = VERSION_SUB_TXT;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    txtSize.cx = max(txtSize.cx, minWidth);
    result.dx += 2 * (txtSize.cx + ABOUT_INNER_PADDING);

    SelectObject(hdc, oldFont);

    return result;
}

static void DrawSumatraVersion(HDC hdc, RectI rect)
{
    Win::Font::ScopedFont fontSumatraTxt(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontVersionTxt(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SetBkMode(hdc, TRANSPARENT);

    SIZE txtSize;
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    RectI mainRect(rect.x + (rect.dx - txtSize.cx) / 2,
                   rect.y + (rect.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    DrawSumatraPDF(hdc, mainRect.TL());

    SetTextColor(hdc, WIN_COL_BLACK);
    SelectObject(hdc, fontVersionTxt);
    PointI pt(mainRect.x + mainRect.dx + ABOUT_INNER_PADDING, mainRect.y);
    txt = VERSION_TXT;
    TextOut(hdc, pt.x, pt.y, txt, Str::Len(txt));
    txt = VERSION_SUB_TXT;
    TextOut(hdc, pt.x, pt.y + 16, txt, Str::Len(txt));

    SelectObject(hdc, oldFont);
}

static RectI DrawBottomRightLink(HWND hwnd, HDC hdc, const TCHAR *txt)
{
    Win::Font::ScopedFont fontLeftTxt(hdc, _T("MS Shell Dlg"), 14);
    HPEN penLinkLine = CreatePen(PS_SOLID, 1, COL_BLUE_LINK);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetTextColor(hdc, COL_BLUE_LINK);
    SetBkMode(hdc, TRANSPARENT);
    ClientRect rc(hwnd);

    SIZE txtSize;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    RectI rect(rc.dx - txtSize.cx - ABOUT_INNER_PADDING,
               rc.y + rc.dy - txtSize.cy - ABOUT_INNER_PADDING, txtSize.cx, txtSize.cy);
    DrawText(hdc, txt, -1, &rect.ToRECT(), DT_LEFT);

    SelectObject(hdc, penLinkLine);
    PaintLine(hdc, RectI(rect.x, rect.y + rect.dy, rect.dx, 0));

    SelectObject(hdc, origFont);
    DeleteObject(penLinkLine);

    // make the click target larger
    rect.Inflate(ABOUT_INNER_PADDING, ABOUT_INNER_PADDING);
    return rect;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, RectI rect, Vec<StaticLinkInfo>& linkInfo)
{
    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);

    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    Win::Font::ScopedFont fontLeftTxt(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontRightTxt(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    ClientRect rc(hwnd);
    FillRect(hdc, &rc.ToRECT(), brushBg);

    /* render title */
    RectI titleRect(rect.TL(), CalcSumatraVersionSize(hdc));

    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx, rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);

    titleRect.Offset((rect.dx - titleRect.dx) / 2, 0);
    DrawSumatraVersion(hdc, titleRect);

    /* render attribution box */
    SetTextColor(hdc, ABOUT_BORDER_COL);
    SetBkMode(hdc, TRANSPARENT);

    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++)
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, Str::Len(el->leftTxt));

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    linkInfo.Reset();
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = !gRestrictedUse && el->url;
        SetTextColor(hdc, hasUrl ? COL_BLUE_LINK : ABOUT_BORDER_COL);
        TextOut(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, Str::Len(el->rightTxt));

        if (hasUrl) {
            int underlineY = el->rightPos.y + el->rightPos.dy - 3;
            PaintLine(hdc, RectI(el->rightPos.x, underlineY, el->rightPos.dx, 0));
            linkInfo.Append(StaticLinkInfo(el->rightPos, el->url, el->url));
        }
    }

    SelectObject(hdc, penDivideLine);
    RectI divideLine(gAboutLayoutInfo[0].rightPos.x - ABOUT_LEFT_RIGHT_SPACE_DX,
                     rect.y + titleRect.dy + 4, 0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    PaintLine(hdc, divideLine);

    SelectObject(hdc, origFont);

    DeleteObject(brushBg);
    DeleteObject(penBorder);
    DeleteObject(penDivideLine);
    DeleteObject(penLinkLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RectI *rect)
{
    HFONT fontLeftTxt = Win::Font::GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win::Font::GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* show/hide the SyncTeX attribution line */
    assert(!gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt || Str::Eq(gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt, _T("synctex")));
    if (gGlobalPrefs.m_enableTeXEnhancements)
        gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt = _T("synctex");
    else
        gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt = NULL;

    /* calculate minimal top box size */
    SizeI headerSize = CalcSumatraVersionSize(hdc);

    /* calculate left text dimensions */
    SelectObject(hdc, fontLeftTxt);
    int leftLargestDx = 0;
    int leftDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        SIZE txtSize;
        GetTextExtentPoint32(hdc, el->leftTxt, Str::Len(el->leftTxt), &txtSize);
        el->leftPos.dx = txtSize.cx;
        el->leftPos.dy = txtSize.cy;

        if (el == &gAboutLayoutInfo[0])
            leftDy = el->leftPos.dy;
        else
            assert(leftDy == el->leftPos.dy);
        if (leftLargestDx < el->leftPos.dx)
            leftLargestDx = el->leftPos.dx;
    }

    /* calculate right text dimensions */
    SelectObject(hdc, fontRightTxt);
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        SIZE txtSize;
        GetTextExtentPoint32(hdc, el->rightTxt, Str::Len(el->rightTxt), &txtSize);
        el->rightPos.dx = txtSize.cx;
        el->rightPos.dy = txtSize.cy;

        if (el == &gAboutLayoutInfo[0])
            rightDy = el->rightPos.dy;
        else
            assert(rightDy == el->rightPos.dy);
        if (rightLargestDx < el->rightPos.dx)
            rightLargestDx = el->rightPos.dx;
    }

    /* calculate total dimension and position */
    RectI minRect;
    minRect.dx = ABOUT_LEFT_RIGHT_SPACE_DX + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    if (minRect.dx < headerSize.dx)
        minRect.dx = headerSize.dx;
    minRect.dx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * ABOUT_MARGIN_DX;

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++)
        minRect.dy += rightDy + ABOUT_TXT_DY;
    minRect.dy += 2 * ABOUT_LINE_OUTER_SIZE + 4;

    ClientRect rc(hwnd);
    minRect.x = (rc.dx - minRect.dx) / 2;
    minRect.y = (rc.dy - minRect.dy) / 2;

    if (rect)
        *rect = minRect;

    /* calculate text positions */
    int linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    int currY = minRect.y + headerSize.dy + 4;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = minRect.x + linePosX - ABOUT_LEFT_RIGHT_SPACE_DX - el->leftPos.dx;
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
        el->rightPos.x = minRect.x + linePosX + ABOUT_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = currY;
        currY += rightDy + ABOUT_TXT_DY;
    }

    SelectObject(hdc, origFont);
    Win::Font::Delete(fontLeftTxt);
    Win::Font::Delete(fontRightTxt);
}

static void OnPaintAbout(HWND hwnd)
{
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc, gLinkInfo);
    EndPaint(hwnd, &ps);
}

const TCHAR *GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo *info)
{
    if (gRestrictedUse)
        return NULL;

    PointI pt(x, y);
    for (size_t i = 0; i < linkInfo.Count(); i++) {
        if (linkInfo[i].rect.Inside(pt)) {
            if (info)
                *info = linkInfo[i];
            return linkInfo[i].target;
        }
    }

    return NULL;
}

void OnMenuAbout() {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    gHwndAbout = CreateWindow(
            ABOUT_CLASS_NAME, ABOUT_WIN_TITLE,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout)
        return;

    // get the dimensions required for the about box's content
    RectI rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    rc.Inflate(ABOUT_RECT_PADDING, ABOUT_RECT_PADDING);

    // resize the new window to just match these dimensions
    WindowRect wRc(gHwndAbout);
    ClientRect cRc(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    ShowWindow(gHwndAbout, SW_SHOW);
}

static void CreateInfotipForLink(StaticLinkInfo& linkInfo)
{
    if (gHwndAboutTooltip)
        return;

    gHwndAboutTooltip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        gHwndAbout, NULL, ghinst, NULL);

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = gHwndAbout;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (TCHAR *)linkInfo.infotip;
    ti.rect = linkInfo.rect.ToRECT();

    SendMessage(gHwndAboutTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

static void ClearInfotip()
{
    if (!gHwndAboutTooltip)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = gHwndAbout;

    SendMessage(gHwndAboutTooltip, TTM_DELTOOL, 0, (LPARAM)&ti);
    DestroyWindow(gHwndAboutTooltip);
    gHwndAboutTooltip = NULL;
}

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const TCHAR * url;
    POINT pt;

    switch (message)
    {
        case WM_CREATE:
            assert(!gHwndAbout);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                StaticLinkInfo linkInfo;
                if (GetStaticLink(gLinkInfo, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursor(gCursorHand);
                    return TRUE;
                }
            }
            ClearInfotip();
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_LBUTTONDOWN:
            gClickedURL = GetStaticLink(gLinkInfo, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;

        case WM_LBUTTONUP:
            url = GetStaticLink(gLinkInfo, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (url && url == gClickedURL)
                LaunchBrowser(url);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            ClearInfotip();
            assert(gHwndAbout);
            gHwndAbout = NULL;
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

void DrawAboutPage(WindowInfo *win, HDC hdc)
{
    ClientRect rc(win->hwndCanvas);
    UpdateAboutLayoutInfo(win->hwndCanvas, hdc, &rc);
    DrawAbout(win->hwndCanvas, hdc, rc, win->staticLinks);
#ifdef NEW_START_PAGE
    if (!gRestrictedUse && gGlobalPrefs.m_rememberOpenedFiles) {
        RectI rect = DrawBottomRightLink(win->hwndCanvas, hdc, _TR("Show frequently read"));
        win->staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_SHOW));
    }
#endif
}

#ifdef NEW_START_PAGE

/* alternate static page to display when no document is loaded */

#include "FileUtil.h"
#include "AppTools.h"
#include "FileHistory.h"

#define DOCLIST_SEPARATOR_DY        2
#define DOCLIST_THUMBNAIL_BORDER_W  2
#define DOCLIST_MARGIN_LEFT        40
#define DOCLIST_MARGIN_BETWEEN_X   30
#define DOCLIST_MARGIN_RIGHT       40
#define DOCLIST_MARGIN_TOP         60
#define DOCLIST_MARGIN_BETWEEN_Y   50
#define DOCLIST_MARGIN_BOTTOM      40
#define DOCLIST_MAX_THUMBNAILS_X    5
#define DOCLIST_BOTTOM_BOX_DY      50

void DrawStartPage(WindowInfo *win, HDC hdc, FileHistory& fileHistory)
{
    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);

    HPEN penBorder = CreatePen(PS_SOLID, DOCLIST_SEPARATOR_DY, WIN_COL_BLACK);
    HPEN penThumbBorder = CreatePen(PS_SOLID, DOCLIST_THUMBNAIL_BORDER_W, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, 1, COL_BLUE_LINK);

    Win::Font::ScopedFont fontSumatraTxt(hdc, _T("MS Shell Dlg"), 24);
    Win::Font::ScopedFont fontLeftTxt(hdc, _T("MS Shell Dlg"), 14);

    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt); /* Just to remember the orig font */

    ClientRect rc(win->hwndCanvas);
    FillRect(hdc, &rc.ToRECT(), brushBg);

    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);

    /* render title */
    RectI titleBox = RectI(PointI(0, 0), CalcSumatraVersionSize(hdc));
    titleBox.x = rc.dx - titleBox.dx - 3;
    DrawSumatraVersion(hdc, titleBox);
    PaintLine(hdc, RectI(0, titleBox.dy, rc.dx, 0));

    /* render recent files list */
    SelectObject(hdc, gBrushNoDocBg);
    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN_COL_BLACK);

    rc.y += titleBox.dy;
    rc.dy -= titleBox.dy;
    FillRect(hdc, &rc.ToRECT(), gBrushNoDocBg);
    rc.dy -= DOCLIST_BOTTOM_BOX_DY;

    Vec<DisplayState *> *list = fileHistory.GetFrequencyOrder();
    // don't show documents for which we don't have any statistics
    // or which don't exist (anymore)
    for (size_t i = list->Count(); i > 0; i--) {
        DisplayState *state = list->At(i - 1);
        if (!state->openCount || !state->thumbnail && !File::Exists(state->filePath))
            list->RemoveAt(i - 1);
    }

    int width = limitValue((rc.dx - DOCLIST_MARGIN_LEFT - DOCLIST_MARGIN_RIGHT + DOCLIST_MARGIN_BETWEEN_X) / (THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X), 1, DOCLIST_MAX_THUMBNAILS_X);
    int height = min((rc.dy - DOCLIST_MARGIN_TOP - DOCLIST_MARGIN_BOTTOM + DOCLIST_MARGIN_BETWEEN_Y) / (THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y), FILE_HISTORY_MAX_FREQUENT / width);
    PointI offset(rc.x + DOCLIST_MARGIN_LEFT + (rc.dx - width * THUMBNAIL_DX - (width - 1) * DOCLIST_MARGIN_BETWEEN_X - DOCLIST_MARGIN_LEFT - DOCLIST_MARGIN_RIGHT) / 2, rc.y + DOCLIST_MARGIN_TOP);
    if (offset.x < ABOUT_INNER_PADDING)
        offset.x = ABOUT_INNER_PADDING;
    else if (list->Count() == 0)
        offset.x = DOCLIST_MARGIN_LEFT;

    SelectObject(hdc, fontSumatraTxt);
    SIZE txtSize;
    const TCHAR *txt = _TR("Frequently Read");
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    TextOut(hdc, offset.x, rc.y + (DOCLIST_MARGIN_TOP - txtSize.cy) / 2, txt, Str::Len(txt));

    SelectObject(hdc, fontLeftTxt);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));

    win->staticLinks.Reset();
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            if (h * width + w >= (int)list->Count()) {
                // display the "Open a document" link right below the last row
                height = w > 0 ? h + 1 : h;
                break;
            }
            DisplayState *state = list->At(h * width + w);

            RectI page(offset.x + w * (THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X),
                       offset.y + h * (THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y),
                       THUMBNAIL_DX, THUMBNAIL_DY);
            if (state->thumbnail) {
                HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
                SelectClipRgn(hdc, clip);
                state->thumbnail->StretchDIBits(hdc, page);
                SelectClipRgn(hdc, NULL);
                DeleteObject(clip);
            }
            RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

            RectI rect(page.x, page.y + THUMBNAIL_DY + 3, THUMBNAIL_DX, 20);
            DrawText(hdc, Path::GetBaseName(state->filePath), -1, &rect.ToRECT(), DT_LEFT | DT_END_ELLIPSIS);

            win->staticLinks.Append(StaticLinkInfo(rect.Union(page), state->filePath, state->filePath));
        }
    }
    delete list;

    /* render bottom links */
    rc.y += DOCLIST_MARGIN_TOP + height * THUMBNAIL_DY + (height - 1) * DOCLIST_MARGIN_BETWEEN_Y + DOCLIST_MARGIN_BOTTOM;
    rc.dy = DOCLIST_BOTTOM_BOX_DY;

    SetTextColor(hdc, COL_BLUE_LINK);
    SelectObject(hdc, penLinkLine);

    HIMAGELIST himl = (HIMAGELIST)SendMessage(win->hwndToolbar, TB_GETIMAGELIST, 0, 0);
    RectI rectIcon(offset.x, rc.y, 0, 0);
    ImageList_GetIconSize(himl, &rectIcon.dx, &rectIcon.dy);
    rectIcon.y += (rc.dy - rectIcon.dy) / 2;
    ImageList_Draw(himl, 0, hdc, rectIcon.x, rectIcon.y, ILD_NORMAL);

    txt = _TR("Open a document...");
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    RectI rect(offset.x + rectIcon.dx + 3, rc.y + (rc.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    DrawText(hdc, txt, -1, &rect.ToRECT(), DT_LEFT);
    PaintLine(hdc, RectI(rect.x, rect.y + rect.dy, rect.dx, 0));
    // make the click target larger
    rect = rect.Union(rectIcon);
    rect.Inflate(10, 10);
    win->staticLinks.Append(StaticLinkInfo(rect, SLINK_OPEN_FILE));

    rect = DrawBottomRightLink(win->hwndCanvas, hdc, _TR("Hide frequently read"));
    win->staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_HIDE));

    SelectObject(hdc, origFont);

    DeleteObject(brushBg);
    DeleteObject(penBorder);
    DeleteObject(penThumbBorder);
    DeleteObject(penLinkLine);
}

static char *PathFingerprint(const TCHAR *filePath);

// TODO: create in TEMP directory instead?
static TCHAR *GetThumbnailPath(const TCHAR *filePath)
{
    ScopedMem<TCHAR> thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    ScopedMem<char> fingerPrint(PathFingerprint(filePath));
    ScopedMem<TCHAR> fname(Str::Conv::FromAnsi(fingerPrint));

    return Str::Format(_T("%s\\%s.bmp"), thumbsPath, fname);
}

// removes thumbnails that don't belong to any frequently used item in file history
static void CleanUpCache(FileHistory& fileHistory)
{
    ScopedMem<TCHAR> thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    ScopedMem<TCHAR> pattern(Path::Join(thumbsPath, _T("*.bmp")));

    StrVec files;
    WIN32_FIND_DATA fdata;

    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return;
    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.Append(Str::Dup(fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    Vec<DisplayState *> *list = fileHistory.GetFrequencyOrder();
    for (size_t i = 0; i < list->Count() && i < FILE_HISTORY_MAX_FREQUENT; i++) {
        ScopedMem<TCHAR> bmpPath(GetThumbnailPath(list->At(i)->filePath));
        int idx = files.Find(Path::GetBaseName(bmpPath));
        if (idx != -1) {
            free(files[idx]);
            files.RemoveAt(idx);
        }
    }
    delete list;

    for (size_t i = 0; i < files.Count(); i++) {
        ScopedMem<TCHAR> bmpPath(Path::Join(thumbsPath, files[i]));
        File::Delete(bmpPath);
    }
}

void LoadThumbnails(FileHistory& fileHistory)
{
    DisplayState *state;
    for (int i = 0; (state = fileHistory.Get(i)); i++) {
        if (state->thumbnail)
            delete state->thumbnail;
        state->thumbnail = NULL;

        ScopedMem<TCHAR> bmpPath(GetThumbnailPath(state->filePath));
        HBITMAP hbmp = (HBITMAP)LoadImage(NULL, bmpPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (!hbmp)
            continue;

        BITMAP bmp;
        GetObject(hbmp, sizeof(BITMAP), &bmp);

        state->thumbnail = new RenderedBitmap(hbmp, bmp.bmWidth, bmp.bmHeight);
    }

    CleanUpCache(fileHistory);
}

void SaveThumbnail(DisplayState *state)
{
    if (!state->thumbnail)
        return;

    size_t dataLen;
    unsigned char *data = state->thumbnail->Serialize(&dataLen);
    if (data) {
        ScopedMem<TCHAR> bmpPath(GetThumbnailPath(state->filePath));
        ScopedMem<TCHAR> thumbsPath(Path::GetDir(bmpPath));
        if (Dir::Create(thumbsPath))
            File::WriteAll(bmpPath, data, dataLen);
        free(data);
    }
}

extern "C" {
__pragma(warning(push))
#include <fitz.h>
__pragma(warning(pop))
}

// creates a fingerprint of a (normalized) path and the
// file's last modification time - much quicker than hashing
// a file's entire content
// caller needs to free() the result
static char *PathFingerprint(const TCHAR *filePath)
{
    unsigned char digest[16];
    ScopedMem<TCHAR> pathN(Path::Normalize(filePath));
    ScopedMem<char> pathU(Str::Conv::ToUtf8(pathN));
    FILETIME lastMod = File::GetModificationTime(pathN);

    fz_md5 md5;
    fz_md5_init(&md5);
    fz_md5_update(&md5, (unsigned char *)pathU.Get(), Str::Len(pathU));
    fz_md5_update(&md5, (unsigned char *)&lastMod, sizeof(lastMod));
    fz_md5_final(&md5, digest);

    return Str::MemToHex(digest, 16);
}

#endif
