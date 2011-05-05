/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "ParseCommandLine.h"
#include "CmdLineParser.h"
#include "StressTesting.h"

#ifdef DEBUG
static void EnumeratePrinters()
{
    PRINTER_INFO_5 *info5Arr = NULL;
    DWORD bufSize = 0, printersCount;
    bool fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
        5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    if (!fOk) {
        info5Arr = (PRINTER_INFO_5 *)malloc(bufSize);
        fOk = EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 
            5, (LPBYTE)info5Arr, bufSize, &bufSize, &printersCount);
    }
    if (!info5Arr)
        return;
    assert(fOk);
    if (!fOk) return;
    printf("Printers: %ld\n", printersCount);
    for (DWORD i=0; i < printersCount; i++) {
        const TCHAR *printerName = info5Arr[i].pPrinterName;
        const TCHAR *printerPort = info5Arr[i].pPortName;
        bool fDefault = false;
        if (info5Arr[i].Attributes & PRINTER_ATTRIBUTE_DEFAULT)
            fDefault = true;
        _tprintf(_T("Name: %s, port: %s, default: %d\n"), printerName, printerPort, (int)fDefault);
    }
    TCHAR buf[512];
    bufSize = dimof(buf);
    fOk = GetDefaultPrinter(buf, &bufSize);
    if (!fOk) {
        if (ERROR_FILE_NOT_FOUND == GetLastError())
            printf("No default printer\n");
    }
    free(info5Arr);
}
#endif

/* Parse 'txt' as hex color and return the result in 'destColor' */
static void ParseColor(int *destColor, const TCHAR* txt)
{
    if (!destColor)
        return;
    if (str::StartsWith(txt, _T("0x")))
        txt += 2;
    else if (str::StartsWith(txt, _T("#")))
        txt += 1;

    unsigned int r, g, b;
    if (str::Parse(txt, _T("%2x%2x%2x"), &r, &g, &b))
        *destColor = RGB(r, g, b);
}

namespace str {

// compares two strings ignoring case and whitespace
static bool EqIS(const TCHAR *s1, const TCHAR *s2)
{
    while (*s1 && *s2) {
        // skip whitespace
        for (; _istspace(*s1); s1++);
        for (; _istspace(*s2); s2++);

        if (_totlower(*s1) != _totlower(*s2))
            return false;
        if (*s1) { s1++; s2++; }
    }

    return !*s1 && !*s2;
}

}

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(txt, _T(enumName##_STR))) { \
        *mode = enumName; \
        return; \
    }

// -view [continuous][singlepage|facing|bookview]
static void ParseViewMode(DisplayMode *mode, const TCHAR *txt)
{
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    if (str::EqIS(txt, _T("continuous single page"))) {
        *mode = DM_CONTINUOUS;
        return;
    }
}

// -zoom [fitwidth|fitpage|fitcontent|100%] (with 100% meaning actual size)
static void ParseZoomValue(float *zoom, const TCHAR *txt)
{
    if (str::EqIS(txt, _T("fit page")))
        *zoom = ZOOM_FIT_PAGE;
    else if (str::EqIS(txt, _T("fit width")))
        *zoom = ZOOM_FIT_WIDTH;
    else if (str::EqIS(txt, _T("fit content")))
        *zoom = ZOOM_FIT_CONTENT;
    else
        str::Parse(txt, _T("%f"), zoom);
}

// -scroll x,y
static void ParseScrollValue(PointI *scroll, const TCHAR *txt)
{
    int x, y;
    if (str::Parse(txt, _T("%d,%d"), &x, &y))
        *scroll = PointI(x, y);
}

/* parse argument list. we assume that all unrecognized arguments are file names. */
void CommandLineInfo::ParseCommandLine(TCHAR *cmdLine)
{
    CmdLineParser argList(cmdLine);
    size_t argCount = argList.Count();

#define is_arg(txt) str::EqI(_T(txt), argument)
#define is_arg_with_param(txt) (is_arg(txt) && param != NULL)

    for (size_t n = 1; n < argCount; n++) {
        TCHAR *argument = argList[n], *param = NULL;
        if (n < argCount - 1)
            param = argList[n + 1];

        if (is_arg("-register-for-pdf")) {
            makeDefault = true;
            exitImmediately = true;
            return;
        }
        else if (is_arg("-silent")) {
            // silences errors happening during -print-to and -print-to-default
            silent = true;
        }
        else if (is_arg("-print-to-default")) {
            TCHAR *printerName = GetDefaultPrinterName();
            if (printerName) {
                SetPrinterName(printerName);
                free(printerName);
            }
        }
        else if (is_arg_with_param("-print-to")) {
            SetPrinterName(argList[++n]);
        }
        else if (is_arg("-print-dialog")) {
            printDialog = true;
        }
        else if (is_arg("-exit-on-print")) {
            // only affects -print-dialog (-print-to and -print-to-default
            // always exit on print)
            exitOnPrint = true;
        }
        else if (is_arg_with_param("-bgcolor") || is_arg_with_param("-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consitency
            ParseColor(&bgColor, argList[++n]);
        }
        else if (is_arg_with_param("-inverse-search")) {
            SetInverseSearchCmdLine(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-offset")) {
            fwdsearchOffset = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-width")) {
            fwdsearchWidth = _ttoi(argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-color")) {
            ParseColor(&fwdsearchColor, argList[++n]);
        }
        else if (is_arg_with_param("-fwdsearch-permanent")) {
            fwdsearchPermanent = _ttoi(argList[++n]);
        }
        else if (is_arg("-esc-to-exit")) {
            escToExit = true;
        }
        else if (is_arg("-reuse-instance")) {
            // find the window handle of a running instance of SumatraPDF
            // TODO: there should be a mutex here to reduce possibility of
            // race condition and having more than one copy launch because
            // FindWindow() in one process is called before a window is created
            // in another process
            reuseInstance = (FindWindow(FRAME_CLASS_NAME, 0) != NULL);
        }
        else if (is_arg_with_param("-lang")) {
            SetLang(argList[++n]);
        }
        else if (is_arg_with_param("-nameddest") || is_arg_with_param("-named-dest")) {
            // -nameddest is for backwards compat (was used pre-1.3)
            // -named-dest is for consitency
            SetDestName(argList[++n]);
        }
        else if (is_arg_with_param("-page")) {
            pageNumber = _ttoi(argList[++n]);
        }
        else if (is_arg("-restrict")) {
            restrictedUse = true;
        }
        else if (is_arg("-invertcolors") || is_arg("-invert-colors")) {
            // -invertcolors is for backwards compat (was used pre-1.3)
            // -invert-colors is for consitency
            invertColors = TRUE;
        }
        else if (is_arg("-presentation")) {
            enterPresentation = true;
        }
        else if (is_arg("-fullscreen")) {
            enterFullscreen = true;
        }
        else if (is_arg_with_param("-view")) {
            ParseViewMode(&startView, argList[++n]);
        }
        else if (is_arg_with_param("-zoom")) {
            ParseZoomValue(&startZoom, argList[++n]);
        }
        else if (is_arg_with_param("-scroll")) {
            ParseScrollValue(&startScroll, argList[++n]);
        }
        else if (is_arg("-console")) {
            showConsole = true;
        }
        else if (is_arg_with_param("-plugin")) {
            // the argument is a (nummeric) window handle to
            // become the parent of a frameless SumatraPDF
            // (used e.g. for embedding it into a browser plugin)
            hwndPluginParent = (HWND)_ttol(argList[++n]);
        }
        else if (is_arg_with_param("-stress-test-dir")) {
            stressTestDir = str::Dup(argList[++n]);
        }
        else if (is_arg_with_param("-bench")) {
            TCHAR *s = str::Dup(argList[++n]);
            filesToBenchmark.Push(s);
            s = NULL;
            if ((n + 1 < argCount) && IsBenchPagesInfo(argList[n+1]))
                s = str::Dup(argList[++n]);
            filesToBenchmark.Push(s);
            exitImmediately = true;
        }
#ifdef DEBUG
        else if (is_arg("-enum-printers")) {
            EnumeratePrinters();
            /* this is for testing only, exit immediately */
            exitImmediately = true;
            return;
        }
#endif
        else {
            // Remember this argument as a filename to open
            TCHAR *filepath = NULL;
            if (str::EndsWithI(argList[n], _T(".lnk")))
                filepath = ResolveLnk(argList[n]);
            if (!filepath)
                filepath = str::Dup(argList[n]);
            fileNames.Push(filepath);
        }
    }
#undef is_arg
#undef is_arg_with_param
}
