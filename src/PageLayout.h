/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseUtil.h"
#include "HtmlPullParser.h"
#include "Scoped.h"
#include "Vec.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum DrawInstrType {
    // a piece of text
    InstrString = 0,
    // elastic space takes at least spaceDx pixels but can take more
    // if a line is justified
    InstrElasticSpace,
    // a fixed space takes a fixed amount of pixels. It's used e.g.
    // to implement paragraph indentation
    InstrFixedSpace,
    // a vertical line
    InstrLine,
    // change current font
    InstrSetFont
};

struct InstrStringData {
    const char *        s;
    size_t              len;
};

struct DrawInstr {
    DrawInstrType       type;
    union {
        // info specific to a given instruction
        InstrStringData     str;          // InstrString
        Font *              font;         // InstrSetFont
        float               fixedSpaceDx; // InstrFixedSpace
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }

    DrawInstr(DrawInstrType t, RectF bbox = RectF()) : type(t), bbox(bbox) { }

    // helper constructors for instructions that need additional arguments
    static DrawInstr Str(const char *s, size_t len, RectF bbox);
    static DrawInstr SetFont(Font *font);
    static DrawInstr FixedSpace(float dx);
};

struct PageData {
    Vec<DrawInstr>  instructions;
};

// just to pack args to LayoutHtml
class LayoutInfo {
public:
    LayoutInfo() :
      pageDx(0), pageDy(0), fontName(NULL), fontSize(0),
      textAllocator(NULL), htmlStr(0), htmlStrLen(0)
    { }

    int             pageDx;
    int             pageDy;

    const WCHAR *   fontName;
    float           fontSize;

    /* Most of the time string DrawInstr point to original html text
       that is read-only and outlives us. Sometimes (e.g. when resolving
       html entities) we need a modified text. This allocator is
       used to allocate this text. */
    Allocator *     textAllocator;

    const char *    htmlStr;
    size_t          htmlStrLen;
};

class PageLayout
{
public:
    PageLayout();
    ~PageLayout();

    PageData *IterStart(LayoutInfo* layoutInfo);
    PageData *IterNext();

private:
    void HandleHtmlTag(HtmlToken *t);
    void HandleText(HtmlToken *t);

    float CurrLineDx();
    float CurrLineDy();
    void  LayoutLeftStartingAt(REAL offX);
    void  JustifyLineBoth();
    void  JustifyCurrLine(AlignAttr align);
    bool  FlushCurrLine(bool isParagraphBreak);

    void  EmitLine();
    void  EmitTextRune(const char *s, const char *end);
    void  EmitNewLine();
    void  EmitElasticSpace();
    void  EmitParagraph(float indent, float topPadding);
    void  ForceNewPage();
    bool  EnsureDx(float dx);

    void  SetCurrentFont(FontStyle fs, float fontSize);
    void  ChangeFontStyle(FontStyle fs, bool isStart);
    void  ChangeFontSize(float fontSize);

    bool  IsCurrLineEmpty();

    // constant during layout process
    float               pageDx;
    float               pageDy;
    float               lineSpacing;
    float               spaceDx;
    Graphics *          gfx; // for measuring text
    ScopedMem<WCHAR>    defaultFontName;
    float               defaultFontSize;
    Allocator *         textAllocator;

    // temporary state during layout process
    FontStyle           currFontStyle;
    float               currFontSize;
    Font *              currFont;

    AlignAttr           currJustification;
    // current position in a page
    float               currX, currY;
    // remembered when we start a new line, used when we actually
    // layout a line
    float               currLineTopPadding;
    // number of consecutive newlines
    int                 newLinesCount;

    // isntructions for the current line
    Vec<DrawInstr>      currLineInstr;
    PageData *          currPage;

    HtmlPullParser *    htmlParser;

    // list of pages that we've build but haven't yet sent to client
    Vec<PageData*>      pagesToSend;
    bool                finishedParsing;

    WCHAR               buf[512];
};

Vec<PageData*> *LayoutHtml(LayoutInfo* li);

void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox);

#endif
