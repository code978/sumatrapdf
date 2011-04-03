/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include "ComicBook.h"
#include "SumatraPDF.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "WindowInfo.h"
#include "AppPrefs.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

using namespace Gdiplus;

class ComicBookPage {
public:
    ComicBookPage(Bitmap *bmp) : bmp(bmp), w(bmp->GetWidth()), h(bmp->GetHeight()) { }

    int         w, h;
    Bitmap *    bmp;
};

bool IsComicBook(const TCHAR *fileName)
{
    if (Str::EndsWithI(fileName, _T(".cbz")))
        return true;
#if 0 // not yet
    if (Str::EndsWithI(fileName, _T(".cbr")))
        return true;
#endif
    return false;
}

static bool IsImageFile(char *fileName)
{
    if (Str::EndsWithI(fileName, ".png"))
        return true;
    if (Str::EndsWithI(fileName, ".jpg") || Str::EndsWithI(fileName, ".jpeg"))
        return true;
    return false;
}

// HGLOBAL must be allocated with GlobalAlloc(GMEM_MOVEABLE, ...)
Bitmap *BitmapFromHGlobal(HGLOBAL mem)
{
    IStream *stream = NULL;
    Bitmap *bmp = NULL;

    GlobalLock(mem); // not sure if needed

    if (CreateStreamOnHGlobal(mem, FALSE, &stream) != S_OK)
        goto Exit;

    bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp)
        goto Exit;

    if (bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        bmp = NULL;
    }
Exit:
    GlobalUnlock(mem);
    return bmp;
}

ComicBookPage *LoadCurrentComicBookPage(unzFile& uf)
{
    char fileName[MAX_PATH];
    unz_file_info64 finfo;
    ComicBookPage *page = NULL;
    HGLOBAL data = NULL;
    int readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    if (!IsImageFile(fileName))
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    data = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!data)
        goto Exit;

    void *buf = GlobalLock(data);
    readBytes = unzReadCurrentFile(uf, buf, len);
    GlobalUnlock(data);

    if ((unsigned)readBytes != len)
        goto Exit;

    Bitmap *bmp = BitmapFromHGlobal(data);
    if (!bmp)
        goto Exit;

    page = new ComicBookPage(bmp);

Exit:
    err = unzCloseCurrentFile(uf);
    // ignoring the error

    GlobalFree(data);
    return page;
}

// TODO: similar to WindowInfo_CreateEmpty(), extract common parts
WindowInfo* CreateEmptyComicBookWindow()
{
    RectI windowPos;
    if (gGlobalPrefs.m_windowPos.IsEmpty()) {
        // center the window on the primary monitor
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        windowPos.y = workArea.top;
        windowPos.dy = RectDy(&workArea);
        windowPos.dx = (int)min(windowPos.dy * DEF_PAGE_RATIO, RectDx(&workArea));
        windowPos.x = (RectDx(&workArea) - windowPos.dx) / 2;
    }
    else {
        windowPos = gGlobalPrefs.m_windowPos;
        EnsureWindowVisibility(&windowPos);
    }

    HWND hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    assert(NULL == FindWindowInfoByHwnd(hwndFrame));
    WindowInfo *win = new WindowInfo(hwndFrame);

    HWND hwndCanvas = CreateWindowEx(
            WS_EX_STATICEDGE, 
            CANVAS_CLASS_NAME, NULL,
            WS_CHILD | WS_HSCROLL | WS_VSCROLL,
            0, 0, 0, 0, /* position and size determined in OnSize */
            hwndFrame, NULL,
            ghinst, NULL);
    if (!hwndCanvas)
        return NULL;
    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(hwndCanvas, SB_BOTH, FALSE);
    win->menu = BuildMenu(win->hwndFrame);

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    CreateToolbar(win, ghinst);
    DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    return win;
}

// Load *.cbz / *.cbr file
// TODO: far from being done
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin)
{    
    zlib_filefunc64_def ffunc;
    unzFile uf;
    fill_win32_filefunc64(&ffunc);

    ScopedGdiPlus gdiPlus;

    uf = unzOpen2_64(fileName, &ffunc);
    if (!uf) {
        goto Error;
    }
    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        goto Error;
    }

    // extract all contained files one by one
    Vec<ComicBookPage*> *pages = new Vec<ComicBookPage *>(256);

    unzGoToFirstFile(uf);

    // Note: maybe lazy loading would be beneficial (but at least I would
    // need to parse image headers to extract width/height information)
    for (int n = 0; n < ginfo.number_entry; n++) {
        ComicBookPage *page = LoadCurrentComicBookPage(uf);
        if (page)
            pages->Append(page);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

    unzClose(uf);
    if (0 == pages->Count())
    {
        delete pages;
        return NULL;
    }

    // TODO: open a window etc.
    DeleteVecMembers(*pages);
    delete pages; // for now
    return NULL;

Error:
    if (uf)
        unzClose(uf);
    return NULL;
}
