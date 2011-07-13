/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "FileUtil.h"

#include "Print.h"

#include "translations.h"
#include "WindowInfo.h"
#include "AppTools.h"

#include "SumatraPDF.h" // TODO: SumatraPDF.h must be included before Notifications.h
#include "Notifications.h"
#include "SumatraDialogs.h"

static bool CheckPrinterStretchDibSupport(HWND hwndForMsgBox, HDC hdc)
{
#ifdef USE_GDI_FOR_PRINTING
    // assume the printer supports enough of GDI(+) for reasonable results
    return true;
#else
    // most printers can support stretchdibits,
    // whereas a lot of printers do not support bitblt
    // quit if printer doesn't support StretchDIBits
    int rasterCaps = GetDeviceCaps(hdc, RASTERCAPS);
    int supportsStretchDib = rasterCaps & RC_STRETCHDIB;
    if (supportsStretchDib)
        return true;

    MessageBox(hwndForMsgBox, _T("This printer doesn't support the StretchDIBits function"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK);
    return false;
#endif
}

struct PrintData {
    HDC hdc; // owned by PrintData

    BaseEngine *engine;
    Vec<PRINTPAGERANGE> ranges; // empty when printing a selection
    Vec<SelectionOnPage> sel;   // empty when printing a page range
    int rotation;
    PrintRangeAdv rangeAdv;
    PrintScaleAdv scaleAdv;
    short orientation;

    PrintData(BaseEngine *engine, HDC hdc, DEVMODE *devMode,
              Vec<PRINTPAGERANGE>& ranges, int rotation=0,
              PrintRangeAdv rangeAdv=PrintRangeAll,
              PrintScaleAdv scaleAdv=PrintScaleShrink,
              Vec<SelectionOnPage> *sel=NULL) :
        engine(NULL), hdc(hdc), rotation(rotation), rangeAdv(rangeAdv), scaleAdv(scaleAdv)
    {
        if (engine)
            this->engine = engine->Clone();

        if (!sel)
            this->ranges = ranges;
        else
            this->sel = *sel;

        orientation = 0;
        if (devMode && (devMode->dmFields & DM_ORIENTATION))
            orientation = devMode->dmOrientation;
    }

    ~PrintData() {
        delete engine;
        DeleteDC(hdc);
    }
};

static void PrintToDevice(PrintData& pd, ProgressUpdateUI *progressUI=NULL)
{
    assert(pd.engine);
    if (!pd.engine) return;

    HDC hdc = pd.hdc;
    BaseEngine& engine = *pd.engine;

    DOCINFO di = { 0 };
    di.cbSize = sizeof (DOCINFO);
    di.lpszDocName = engine.FileName();

    int current = 0, total = 0;
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        if (pd.ranges[i].nToPage < pd.ranges[i].nFromPage)
            total += pd.ranges[i].nFromPage - pd.ranges[i].nToPage + 1;
        else
            total += pd.ranges[i].nToPage - pd.ranges[i].nFromPage + 1;
    }
    total += (int)pd.sel.Count();
    if (progressUI)
        progressUI->ProgressUpdate(current, total);

    if (StartDoc(hdc, &di) <= 0)
        return;

    SetMapMode(hdc, MM_TEXT);

    const int paperWidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    const int paperHeight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    const int printableWidth = GetDeviceCaps(hdc, HORZRES);
    const int printableHeight = GetDeviceCaps(hdc, VERTRES);
    const int leftMargin = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    const int topMargin = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    const int rightMargin = paperWidth - printableWidth - leftMargin;
    const int bottomMargin = paperHeight - printableHeight - topMargin;
    const float dpiFactor = min(GetDeviceCaps(hdc, LOGPIXELSX) / engine.GetFileDPI(),
                                GetDeviceCaps(hdc, LOGPIXELSY) / engine.GetFileDPI());
    bool bPrintPortrait = paperWidth < paperHeight;
    if (pd.orientation)
        bPrintPortrait = DMORIENT_PORTRAIT == pd.orientation;

    if (pd.sel.Count() > 0) {
        DBG_OUT(" printing:  drawing bitmap for selection\n");

        for (size_t i = 0; i < pd.sel.Count(); i++) {
            StartPage(hdc);
            RectD *clipRegion = &pd.sel[i].rect;

            Size<float> sSize = clipRegion->Size().Convert<float>();
            // Swap width and height for rotated documents
            int rotation = engine.PageRotation(pd.sel[i].pageNo) + pd.rotation;
            if (rotation % 180 != 0)
                swap(sSize.dx, sSize.dy);

            float zoom = min((float)printableWidth / sSize.dx,
                             (float)printableHeight / sSize.dy);
            // use the correct zoom values, if the page fits otherwise
            // and the user didn't ask for anything else (default setting)
            if (PrintScaleShrink == pd.scaleAdv)
                zoom = min(dpiFactor, zoom);
            else if (PrintScaleNone == pd.scaleAdv)
                zoom = dpiFactor;

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(printableWidth - sSize.dx * zoom) / 2,
                                     (int)(printableHeight - sSize.dy * zoom) / 2,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pd.sel[i].pageNo, zoom, pd.rotation, clipRegion, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pd.sel[i].pageNo, zoom, pd.rotation, clipRegion, Target_Print);
            if (bmp) {
                PointI TL((printableWidth - bmp->Size().dx) / 2,
                          (printableHeight - bmp->Size().dy) / 2);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI && !progressUI->ProgressUpdate(current, total)) {
                AbortDoc(hdc);
                return;
            }
        }

        EndDoc(hdc);
        return;
    }

    // print all the pages the user requested
    for (size_t i = 0; i < pd.ranges.Count(); i++) {
        int dir = pd.ranges[i].nFromPage > pd.ranges[i].nToPage ? -1 : 1;
        for (DWORD pageNo = pd.ranges[i].nFromPage; pageNo != pd.ranges[i].nToPage + dir; pageNo += dir) {
            if ((PrintRangeEven == pd.rangeAdv && pageNo % 2 != 0) ||
                (PrintRangeOdd == pd.rangeAdv && pageNo % 2 == 0))
                continue;

            DBG_OUT(" printing:  drawing bitmap for page %d\n", pageNo);

            StartPage(hdc);
            // MM_TEXT: Each logical unit is mapped to one device pixel.
            // Positive x is to the right; positive y is down.

            Size<float> pSize = engine.PageMediabox(pageNo).Size().Convert<float>();
            int rotation = engine.PageRotation(pageNo);
            // Turn the document by 90 deg if it isn't in portrait mode
            if (pSize.dx > pSize.dy) {
                rotation += 90;
                swap(pSize.dx, pSize.dy);
            }
            // make sure not to print upside-down
            rotation = (rotation % 180) == 0 ? 0 : 270;
            // finally turn the page by (another) 90 deg in landscape mode
            if (!bPrintPortrait) {
                rotation = (rotation + 90) % 360;
                swap(pSize.dx, pSize.dy);
            }

            // dpiFactor means no physical zoom
            float zoom = dpiFactor;
            // offset of the top-left corner of the page from the printable area
            // (negative values move the page into the left/top margins, etc.);
            // offset adjustments are needed because the GDI coordinate system
            // starts at the corner of the printable area and we rather want to
            // center the page on the physical paper (default behavior)
            int horizOffset = (paperWidth - printableWidth) / 2 - leftMargin;
            int vertOffset = (paperHeight - printableHeight) / 2 - topMargin;

            if (pd.scaleAdv != PrintScaleNone) {
                // make sure to fit all content into the printable area when scaling
                // and the whole document page on the physical paper
                RectD rect = engine.PageContentBox(pageNo, Target_Print);
                Rect<float> cbox = engine.Transform(rect, pageNo, 1.0, rotation).Convert<float>();
                zoom = min((float)printableWidth / cbox.dx,
                       min((float)printableHeight / cbox.dy,
                       min((float)paperWidth / pSize.dx,
                           (float)paperHeight / pSize.dy)));
                // use the correct zoom values, if the page fits otherwise
                // and the user didn't ask for anything else (default setting)
                if (PrintScaleShrink == pd.scaleAdv && dpiFactor < zoom)
                    zoom = dpiFactor;
                // make sure that no content lies in the non-printable paper margins
                Rect<float> onPaper((paperWidth - pSize.dx * zoom) / 2 + cbox.x * zoom,
                                    (paperHeight - pSize.dy * zoom) / 2 + cbox.y * zoom,
                                    cbox.dx * zoom, cbox.dy * zoom);
                if (leftMargin > onPaper.x)
                    horizOffset = (int)(horizOffset + leftMargin - onPaper.x);
                else if (paperWidth - rightMargin < onPaper.BR().x)
                    horizOffset = (int)(horizOffset - onPaper.BR().x + (paperWidth - rightMargin));
                if (topMargin > onPaper.y)
                    vertOffset = (int)(vertOffset + topMargin - onPaper.y);
                else if (paperHeight - bottomMargin < onPaper.BR().y)
                    vertOffset = (int)(vertOffset - onPaper.BR().y + (paperHeight - bottomMargin));
            }

#ifdef USE_GDI_FOR_PRINTING
            RectI rc = RectI::FromXY((int)(paperWidth - pSize.dx * zoom) / 2 + horizOffset - leftMargin,
                                     (int)(paperHeight - pSize.dy * zoom) / 2 + vertOffset - topMargin,
                                     paperWidth, paperHeight);
            engine.RenderPage(hdc, rc, pageNo, zoom, rotation, NULL, Target_Print);
#else
            RenderedBitmap *bmp = engine.RenderBitmap(pageNo, zoom, rotation, NULL, Target_Print);
            if (bmp) {
                PointI TL((paperWidth - bmp->Size().dx) / 2 + horizOffset - leftMargin,
                          (paperHeight - bmp->Size().dy) / 2 + vertOffset - topMargin);
                bmp->StretchDIBits(hdc, RectI(TL, bmp->Size()));
                delete bmp;
            }
#endif
            if (EndPage(hdc) <= 0) {
                AbortDoc(hdc);
                return;
            }

            current++;
            if (progressUI && !progressUI->ProgressUpdate(current, total)) {
                AbortDoc(hdc);
                return;
            }
        }
    }

    EndDoc(hdc);
}

class PrintThreadUpdateWorkItem : public UIThreadWorkItem {
    MessageWnd *wnd;
    int current, total;

public:
    PrintThreadUpdateWorkItem(WindowInfo *win, MessageWnd *wnd, int current, int total)
        : UIThreadWorkItem(win), wnd(wnd), current(current), total(total) { }

    virtual void Execute() {
        if (WindowInfoStillValid(win) && win->messages->Contains(wnd))
            wnd->ProgressUpdate(current, total);
    }
};

class PrintThreadWorkItem : public ProgressUpdateUI, public UIThreadWorkItem, public MessageWndCallback {
    MessageWnd *wnd;
    bool isCanceled;

public:
    // owned and deleted by PrintThreadWorkItem
    PrintData *data;

    PrintThreadWorkItem(WindowInfo *win, PrintData *data) :
        UIThreadWorkItem(win), data(data), isCanceled(false) { 
        wnd = new MessageWnd(win->hwndCanvas, _T(""), _TR("Printing page %d of %d..."), this);
        win->messages->Add(wnd);
    }
    ~PrintThreadWorkItem() {
        delete data;
        CleanUp(wnd);
    }

    virtual bool ProgressUpdate(int current, int total) {
        QueueWorkItem(new PrintThreadUpdateWorkItem(win, wnd, current, total));
        return WindowInfoStillValid(win) && !win->printCanceled && !isCanceled;
    }

    void CleanUp() {
        QueueWorkItem(this);
    }

    // called when printing has been canceled
    virtual void CleanUp(MessageWnd *wnd) {
        isCanceled = true;
        this->wnd = NULL;
        if (WindowInfoStillValid(win))
            win->messages->CleanUp(wnd);
    }

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;

        HANDLE thread = win->printThread;
        win->printThread = NULL;
        CloseHandle(thread);
    }
};

static DWORD WINAPI PrintThread(LPVOID data)
{
    PrintThreadWorkItem *progressUI = (PrintThreadWorkItem *)data;
    assert(progressUI && progressUI->data);
    if (progressUI->data)
        PrintToDevice(*progressUI->data, progressUI);
    progressUI->CleanUp();

    return 0;
}

static void PrintToDeviceOnThread(WindowInfo& win, PrintData *data)
{
    PrintThreadWorkItem *progressUI = new PrintThreadWorkItem(&win, data);
    win.printThread = CreateThread(NULL, 0, PrintThread, progressUI, 0, NULL);
}

void AbortPrinting(WindowInfo *win)
{
    if (win->printThread) {
        win->printCanceled = true;
        WaitForSingleObject(win->printThread, INFINITE);
    }
    win->printCanceled = false;
}

#ifndef ID_APPLY_NOW
#define ID_APPLY_NOW 0x3021
#endif

static LRESULT CALLBACK DisableApplyBtnWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if (uiMsg == WM_ENABLE)
        EnableWindow(hWnd, FALSE);

    WNDPROC nextWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    return CallWindowProc(nextWndProc, hWnd, uiMsg, wParam, lParam);
}

/* minimal IPrintDialogCallback implementation for hiding the useless Apply button */
class ApplyButtonDiablingCallback : public IPrintDialogCallback
{
public:
    ApplyButtonDiablingCallback() : m_cRef(0) { };
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        if (riid == IID_IUnknown || riid == IID_IPrintDialogCallback) {
            *ppv = this;
            this->AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    };
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); };
    STDMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&m_cRef); };
    STDMETHODIMP HandleMessage(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult) {
        if (uiMsg == WM_INITDIALOG) {
            HWND hPropSheetContainer = GetParent(GetParent(hDlg));
            HWND hApplyButton = GetDlgItem(hPropSheetContainer, ID_APPLY_NOW);
            WNDPROC nextWndProc = (WNDPROC)SetWindowLongPtr(hApplyButton, GWLP_WNDPROC, (LONG_PTR)DisableApplyBtnWndProc);
            SetWindowLongPtr(hApplyButton, GWLP_USERDATA, (LONG_PTR)nextWndProc);
        }
        return S_FALSE;
    };
    STDMETHODIMP InitDone() { return S_FALSE; };
    STDMETHODIMP SelectionChange() { return S_FALSE; };
protected:
    LONG m_cRef;
    WNDPROC m_wndProc;
};

/* Show Print Dialog box to allow user to select the printer
and the pages to print.

Note: The following doesn't apply for USE_GDI_FOR_PRINTING

Creates a new dummy page for each page with a large zoom factor,
and then uses StretchDIBits to copy this to the printer's dc.

So far have tested printing from XP to
 - Acrobat Professional 6 (note that acrobat is usually set to
   downgrade the resolution of its bitmaps to 150dpi)
 - HP Laserjet 2300d
 - HP Deskjet D4160
 - Lexmark Z515 inkjet, which should cover most bases.
*/
#define MAXPAGERANGES 10
void OnMenuPrint(WindowInfo *win)
{
    // In order to print with Adobe Reader instead:
    // ViewWithAcrobat(win, _T("/P"));

    if (!HasPermission(Perm_PrinterAccess)) return;

    DisplayModel *dm = win->dm;
    assert(dm);
    if (!dm) return;

    if (win->printThread) {
        int res = MessageBox(win->hwndFrame, _TR("Printing is still in progress. Abort and start over?"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_YESNO | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (res == IDNO)
            return;
    }
    AbortPrinting(win);

    PRINTDLGEX pd;
    ZeroMemory(&pd, sizeof(PRINTDLGEX));
    pd.lStructSize = sizeof(PRINTDLGEX);
    pd.hwndOwner   = win->hwndFrame;
    pd.hDevMode    = NULL;
    pd.hDevNames   = NULL;
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;
    if (!win->selectionOnPage)
        pd.Flags |= PD_NOSELECTION;
    pd.nCopies     = 1;
    /* by default print all pages */
    pd.nPageRanges =1;
    pd.nMaxPageRanges = MAXPAGERANGES;
    PRINTPAGERANGE *ppr = SAZA(PRINTPAGERANGE, MAXPAGERANGES);
    pd.lpPageRanges = ppr;
    ppr->nFromPage = 1;
    ppr->nToPage = dm->pageCount();
    pd.nMinPage = 1;
    pd.nMaxPage = dm->pageCount();
    pd.nStartPage = START_PAGE_GENERAL;
    pd.lpCallback = new ApplyButtonDiablingCallback();

    // TODO: remember these (and maybe all of PRINTDLGEX) at least for this document/WindowInfo?
    Print_Advanced_Data advanced = { PrintRangeAll, PrintScaleShrink };
    ScopedMem<DLGTEMPLATE> dlgTemplate; // needed for RTL languages
    HPROPSHEETPAGE hPsp = CreatePrintAdvancedPropSheet(&advanced, dlgTemplate);
    pd.lphPropertyPages = &hPsp;
    pd.nPropertyPages = 1;

    if (PrintDlgEx(&pd) == S_OK) {
        if (pd.dwResultAction == PD_RESULT_PRINT) {
            if (CheckPrinterStretchDibSupport(win->hwndFrame, pd.hDC)) {
                bool printSelection = false;
                Vec<PRINTPAGERANGE> ranges;
                if (pd.Flags & PD_CURRENTPAGE) {
                    PRINTPAGERANGE pr = { dm->currentPageNo(), dm->currentPageNo() };
                    ranges.Append(pr);
                } else if (win->selectionOnPage && (pd.Flags & PD_SELECTION)) {
                    printSelection = true;
                } else if (!(pd.Flags & PD_PAGENUMS)) {
                    PRINTPAGERANGE pr = { 1, dm->pageCount() };
                    ranges.Append(pr);
                } else {
                    assert(pd.nPageRanges > 0);
                    for (DWORD i = 0; i < pd.nPageRanges; i++)
                        ranges.Append(pd.lpPageRanges[i]);
                }

                LPDEVMODE devMode = (LPDEVMODE)GlobalLock(pd.hDevMode);
                PrintData *data = new PrintData(dm->engine, pd.hDC, devMode, ranges,
                                                dm->rotation(), advanced.range, advanced.scale,
                                                printSelection ? win->selectionOnPage : NULL);
                pd.hDC = NULL; // deleted by PrintData
                if (devMode)
                    GlobalUnlock(pd.hDevMode);

                PrintToDeviceOnThread(*win, data);
            }
        }
    }
    else if (CommDlgExtendedError() != 0) { 
        /* if PrintDlg was cancelled then
           CommDlgExtendedError is zero, otherwise it returns the
           error code, which we could look at here if we wanted.
           for now just warn the user that printing has stopped
           becasue of an error */
        MessageBox(win->hwndFrame, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
    }

    free(ppr);
    free(pd.lpCallback);
    DeleteDC(pd.hDC);
    GlobalFree(pd.hDevNames);
    GlobalFree(pd.hDevMode);
}

bool PrintFile(const TCHAR *fileName, const TCHAR *printerName, bool displayErrors)
{
    if (!HasPermission(Perm_PrinterAccess))
        return false;

    ScopedMem<TCHAR> fileName2(path::Normalize(fileName));
    BaseEngine *engine = EngineManager::CreateEngine(fileName2);
    if (!engine || !engine->IsPrintingAllowed()) {
        if (displayErrors)
            MessageBox(NULL, _TR("Cannot print this file"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    // Retrieve the printer, printer driver, and output-port names from WIN.INI. 
    TCHAR devstring[256];
    GetProfileString(_T("Devices"), printerName, _T(""), devstring, dimof(devstring));

    // Parse the string of names
    // If the string contains the required names, use them to
    // create a device context.
    ScopedMem<TCHAR> driver, port;
    if (!str::Parse(devstring, _T("%S,%S,"), &driver, &port) &&
        !str::Parse(devstring, _T("%S,%S"), &driver, &port) || !driver || !port) {
        if (displayErrors)
            MessageBox(NULL, _TR("Printer with given name doesn't exist"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    HANDLE printer;
    bool ok = OpenPrinter((LPTSTR)printerName, &printer, NULL);
    if (!ok) {
        if (displayErrors)
            MessageBox(NULL, _TR("Could not open Printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        return false;
    }

    DWORD structSize = DocumentProperties(NULL,
        printer,                /* Handle to our printer. */ 
        (LPTSTR)printerName,    /* Name of the printer. */ 
        NULL,                   /* Asking for size, so */ 
        NULL,                   /* these are not used. */ 
        0);                     /* Zero returns buffer size. */
    LPDEVMODE devMode = (LPDEVMODE)malloc(structSize);
    HDC hdcPrint = NULL;
    if (!devMode) goto Exit;

    // Get the default DevMode for the printer and modify it for your needs.
    DWORD returnCode = DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* The address of the buffer to fill. */ 
        NULL,           /* Not using the input buffer. */ 
        DM_OUT_BUFFER); /* Have the output buffer filled. */ 
    if (IDOK != returnCode) {
        // If failure, inform the user, cleanup and return failure.
        if (displayErrors)
            MessageBox(NULL, _TR("Could not obtain Printer properties"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }

    /*
     * Merge the new settings with the old.
     * This gives the driver an opportunity to update any private
     * portions of the DevMode structure.
     */ 
    DocumentProperties(NULL,
        printer,
        (LPTSTR)printerName,
        devMode,        /* Reuse our buffer for output. */ 
        devMode,        /* Pass the driver our changes. */ 
        DM_IN_BUFFER |  /* Commands to Merge our changes and */ 
        DM_OUT_BUFFER); /* write the result. */ 

    ClosePrinter(printer);

    hdcPrint = CreateDC(driver, printerName, port, devMode); 
    if (!hdcPrint) {
        if (displayErrors)
            MessageBox(NULL, _TR("Couldn't initialize printer"), _TR("Printing problem."), MB_ICONEXCLAMATION | MB_OK | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        goto Exit;
    }
    if (CheckPrinterStretchDibSupport(NULL, hdcPrint)) {
        PRINTPAGERANGE pr = { 1, engine->PageCount() };
        Vec<PRINTPAGERANGE> ranges;
        ranges.Append(pr);
        PrintData pd(engine, hdcPrint, devMode, ranges);
        hdcPrint = NULL; // deleted by PrintData

        PrintToDevice(pd);
        ok = true;
    }
Exit:
    free(devMode);
    DeleteDC(hdcPrint);
    return ok;
}

