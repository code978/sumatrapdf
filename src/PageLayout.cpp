/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "DebugLog.h"
#include "Mui.h"
#include "PageLayout.h"
#include "StrUtil.h"

/*
TODO: PageLayout could be split into DrawInstrBuilder which knows pageDx, pageDy
and generates DrawInstr and splits them into pages and a better named class that
does the parsing of the document builds pages by invoking methods on DrawInstrBuilders.

TODO: instead of generating list of DrawInstr objects, we could add neccessary
support to mui and use list of Control objects instead (especially if we slim down
Control objects further to make allocating hundreds of them cheaper or introduce some
other base element(s) with less functionality and less overhead).
*/

static void SkipCharInStr(const char *& s, size_t& left, char c)
{
    while ((left > 0) && (*s == c)) {
        ++s; --left;
    }
}

static bool IsWordBreak(char c)
{
    return (c == ' ') || (c == '\n') || (c == '\r');
}

static void SkipNonWordBreak(const char *& s, size_t& left)
{
    while ((left > 0) && !IsWordBreak(*s)) {
        ++s; --left;
    }
}

static bool IsNewline(const char *s, const char *end)
{
    return (1 == end - s) && ('\n' == s[0]);
}

// return true if s points to "\n", "\r" or "\r\n"
// and advance s/left to skip it
// We don't want to collapse multiple consequitive newlines into
// one as we want to be able to detect paragraph breaks (i.e. empty
// newlines i.e. a newline following another newline)
static bool IsNewlineSkip(const char *& s, size_t& left)
{
    if (0 == left)
        return false;
    if ('\r' == *s) {
        --left; ++s;
        if ((left > 0) && ('\n' == *s)) {
            --left; ++s;
        }
        return true;
    }
    if ('\n' == *s) {
        --left; ++s;
        return true;
    }
    return false;
}

DrawInstr DrawInstr::Str(const char *s, size_t len, RectF bbox)
{
    DrawInstr di(InstrString, bbox);
    di.str.s = s;
    di.str.len = len;
    return di;
}

DrawInstr DrawInstr::SetFont(Font *font)
{
    DrawInstr di(InstrSetFont);
    di.setFont.font = font;
    return di;
}

DrawInstr DrawInstr::Line(RectF bbox)
{
    DrawInstr di(InstrLine, bbox);
    return di;
}

DrawInstr DrawInstr::Space()
{
    return DrawInstr(InstrSpace);
}

DrawInstr DrawInstr::ParagraphStart()
{
    return DrawInstr(InstrParagraphStart);
}

PageLayout::PageLayout() : currPage(NULL), gfx(NULL)
{
}

PageLayout::~PageLayout()
{
    // delete all pages that were not consumed by the caller
    for (PageData **p = pagesToSend.IterStart(); p; p = pagesToSend.IterNext()) {
        delete *p;
    }
    delete currPage;
    delete htmlParser;
    mui::FreeGraphicsForMeasureText(gfx);
}

void PageLayout::SetCurrentFont(FontStyle fs)
{
    currFontStyle = fs;
    currFont = mui::GetCachedFont(fontName.Get(), fontSize, fs);
}

static bool ValidFontStyleForChangeFont(FontStyle fs)
{
    if ((FontStyleBold == fs) ||
        (FontStyleItalic == fs) ||
        (FontStyleUnderline == fs) ||
        (FontStyleStrikeout == fs)) {
            return true;
    }
    return false;
}

// change the current font by adding (if addStyle is true) or removing
// a given font style from current font style
// TODO: it doesn't support the case where the same style is nested
// like "<b>fo<b>oo</b>bar</b>" - "bar" should still be bold but wont
// We would have to maintain counts for each style to do it fully right
void PageLayout::ChangeFont(FontStyle fs, bool addStyle)
{
    CrashAlwaysIf(!ValidFontStyleForChangeFont(fs));
    FontStyle newFontStyle = currFontStyle;
    if (addStyle)
        newFontStyle = (FontStyle) (newFontStyle | fs);
    else
        newFontStyle = (FontStyle) (newFontStyle & ~fs);

    if (newFontStyle == currFontStyle)
        return; // a no-op
    SetCurrentFont(newFontStyle);
    EmitSetFont(currFont);
}

DrawInstr *PageLayout::InstructionsForCurrLine(DrawInstr *& endInst) const
{
    size_t len = currPage->Count() - currLineInstrOffset;
    DrawInstr *ret = currPage->drawInstructions.AtPtr(currLineInstrOffset);
    endInst = ret + len;
    return ret;
}

PageData *PageLayout::IterStart(LayoutInfo* layoutInfo)
{
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;
    pageSize.dx = pageDx;
    pageSize.dy = pageDy;
    textAllocator = layoutInfo->textAllocator;

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    fontName.Set(str::Dup(layoutInfo->fontName));
    fontSize = layoutInfo->fontSize;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    finishedParsing = false;
    currJustification = Align_Justify;
    SetCurrentFont(FontStyleRegular);
    InitFontMetricsCache(&fontMetrics, gfx, currFont);

    CrashIf(currPage);
    lineSpacing = currFont->GetHeight(gfx);
    // note: this is a heuristic, using 
    spaceDx = fontSize / 2.5f;
    float spaceDx2 = GetSpaceDx(gfx, currFont);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;
    // value chosen by expermients. Kindle seems to use 2x width
    // of an average character, like "e". Maybe should use e.g.
    // measured width of "ex" string as lineIndentDx
    lineIndentDx = spaceDx * 5.f;
    StartNewPage(true);
    return IterNext();
}

void PageLayout::StartNewPage(bool isParagraphBreak)
{
    if (currPage)
        pagesToSend.Append(currPage);
    currPage = new PageData;
    currX = currY = 0;
    newLinesCount = 0;
    // instructions for each page needb to be self-contained
    // so we have to carry over some state like the current font
    CrashIf(!currFont);
    EmitSetFont(currFont);
    currLineInstrOffset = currPage->Count();
    if (isParagraphBreak)
        EmitParagraphStart();
}

REAL PageLayout::GetCurrentLineDx()
{
    REAL dx = 0;
    DrawInstr *i, *end;
    for (i = InstructionsForCurrLine(end); i < end; i++) {
        if (InstrString == i->type) {
            dx += i->bbox.Width;
        } else if (InstrSpace == i->type) {
            dx += spaceDx;
        }
    }
    return dx;
}

void PageLayout::LayoutLeftStartingAt(REAL offX)
{
    REAL x = offX;
    DrawInstr *i, *end;
    for (i = InstructionsForCurrLine(end); i < end; i++) {
        if (InstrString == i->type) {
            // currInstr Width and Height are already set
            i->bbox.X = x;
            i->bbox.Y = currY;
            x += i->bbox.Width;
        } else if (InstrSpace == i->type) {
            x += spaceDx;
        }
    }
}

// Redistribute extra space in the line equally among the spaces
void PageLayout::JustifyLineBoth(REAL offX)
{
    REAL extraSpaceDxTotal = pageSize.dx - GetCurrentLineDx() - offX;
    LayoutLeftStartingAt(offX);
    DrawInstr *end;
    DrawInstr *start = InstructionsForCurrLine(end);
    size_t spaces = 0;
    for (DrawInstr *i = start; i < end; i++) {
        if (InstrSpace == i->type)
            ++spaces;
    }
    if (0 == spaces)
        return;
    REAL extraSpaceDx = extraSpaceDxTotal / (float)spaces;
    offX = 0;
    DrawInstr *lastStr = NULL;
    for (DrawInstr *i = start; i < end; i++) {
        i->bbox.X += offX;
        if (InstrSpace == i->type)
            offX += extraSpaceDx;
        else if (InstrString == i->type)
            lastStr = i;
    }
    // align the last element perfectly against the right edge in case
    // we've accumulated rounding errors
    if (lastStr)
        lastStr->bbox.X = pageSize.dx - lastStr->bbox.Width;
}

bool PageLayout::IsCurrentLineEmpty() const
{
    size_t instrCount = currPage->Count()- currLineInstrOffset;
    if (0 == instrCount)
        return true;
    if (1 == instrCount)
        return InstrParagraphStart == currPage->drawInstructions.At(currLineInstrOffset).type;
    return false;
}

bool PageLayout::IsCurrentLineIndented() const
{
    size_t instrCount = currPage->Count()- currLineInstrOffset;
    if (instrCount >= 1)
        return InstrParagraphStart == currPage->drawInstructions.At(currLineInstrOffset).type;
    return false;
}

void PageLayout::JustifyLine(AlignAttr mode)
{
    if (IsCurrentLineEmpty())
        return;

    REAL indent = 0;
    if (IsCurrentLineIndented())
        indent = lineIndentDx;

    switch (mode) {
    case Align_Left:
        LayoutLeftStartingAt(indent);
        break;
    case Align_Right:
        LayoutLeftStartingAt(pageSize.dx - GetCurrentLineDx());
        break;
    case Align_Center:
        LayoutLeftStartingAt((pageSize.dx - GetCurrentLineDx()) / 2.f);
        break;
    case Align_Justify:
        JustifyLineBoth(indent);
        break;
    default:
        CrashIf(true);
        break;
    }
    currLineInstrOffset = currPage->Count();
}

void PageLayout::StartNewLine(bool isParagraphBreak)
{
    // don't create empty lines
    if (IsCurrentLineEmpty()) {
        if (isParagraphBreak && !IsCurrentLineIndented())
            EmitParagraphStart();
        return;
    }
    if (isParagraphBreak && Align_Justify == currJustification)
        JustifyLine(Align_Left);
    else
        JustifyLine(currJustification);

    currX = 0;
    currY += lineSpacing;
    currLineInstrOffset = currPage->Count();
    if (currY + lineSpacing > pageDy)
        StartNewPage(isParagraphBreak);
    else if (isParagraphBreak)
        EmitParagraphStart();
}

#if 0
struct KnownAttrInfo {
    HtmlAttr        attr;
    const char *    val;
    size_t          valLen;
};

static bool IsAllowedAttribute(HtmlAttr* allowedAttributes, HtmlAttr attr)
{
    while (Attr_NotFound != *allowedAttributes) {
        if (attr == *allowedAttributes++)
            return true;
    }
    return false;
}

static void GetKnownAttributes(HtmlToken *t, HtmlAttr *allowedAttributes, Vec<KnownAttrInfo> *out)
{
    out->Reset();
    AttrInfo *attrInfo;
    size_t tagLen = GetTagLen(t);
    const char *curr = t->s + tagLen;
    const char *end = t->s + t->sLen;
    for (;;) {
        attrInfo = t->NextAttr();
        if (NULL == attrInfo)
            break;
        HtmlAttr attr = FindAttr(attrInfo);
        if (!IsAllowedAttribute(allowedAttributes, attr))
            continue;
        KnownAttrInfo knownAttr = { attr, attrInfo->val, attrInfo->valLen };
        out->Append(knownAttr);
    }
}
#endif

void PageLayout::EmitSetFont(Font *font)
{
    currPage->Append(DrawInstr::SetFont(font));
}

// add horizontal line (<hr> in html terms)
void PageLayout::EmitLine()
{
    // hr creates an implicit paragraph break
    StartNewLine(true);
    currX = 0;
    // height of hr is lineSpacing. If drawing a line would cause
    // current position to exceed page bounds, go to another page
    if (currY + lineSpacing > pageDy)
        StartNewPage(false);

    RectF bbox(currX, currY, pageDx, lineSpacing);
    currPage->Append(DrawInstr::Line(bbox));
    StartNewLine(true);
}

void PageLayout::EmitParagraphStart()
{
    if ((Align_Left == currJustification) || (Align_Justify == currJustification)) {
        CrashIf(0 != currX);
        currX = lineIndentDx;
        currPage->drawInstructions.Append(DrawInstr::ParagraphStart());
    }
}

void PageLayout::EmitSpace()
{
    // don't put spaces at the beginnng of the line
    if (0 == currX)
        return;
    // don't allow consequitive spaces
    if (IsLastInstrSpace())
        return;
    // don't add a space if it would cause creating a new line, but
    // do create a new line
    if (currX + spaceDx > pageDx) {
        StartNewLine(false);
        return;
    }
    currX += spaceDx;
    currPage->drawInstructions.Append(DrawInstr::Space());
}

bool PageLayout::IsLastInstrSpace() const
{
    if (0 == currPage->Count())
        return false;
    DrawInstr& di = currPage->drawInstructions.Last();
    return (InstrParagraphStart == di.type) || (InstrSpace == di.type);
}

// a text rune is a string of consequitive text with uniform style
void PageLayout::EmitTextRune(const char *s, const char *end)
{
    // collapse multiple, consequitive white-spaces into a single space
    if (IsSpaceOnly(s, end)) {
        EmitSpace();
        return;
    }

    if (IsNewline(s, end)) {
        // a single newline is considered "soft" and ignored
        // two or more consequitive newlines are considered a
        // single paragraph break
        newLinesCount++;
        if (2 == newLinesCount) {
            bool needsTwo = (currX != 0);
            StartNewLine(true);
            if (needsTwo)
                StartNewLine(true);
        }
        return;
    }
    newLinesCount = 0;

    const char *tmp = ResolveHtmlEntities(s, end, textAllocator);
    if (tmp != s) {
        s = tmp;
        end = s + str::Len(s);
    }

    size_t strLen = str::Utf8ToWcharBuf(s, end - s, buf, dimof(buf));
    RectF bbox = MeasureText(gfx, currFont, buf, strLen);
    REAL dx = bbox.Width;
    if (currX + dx > pageDx) {
        // start new line if the new text would exceed the line length
        StartNewLine(false);
        // TODO: handle a case where a single word is bigger than the whole
        // line, in which case it must be split into multiple lines
    }
    bbox.Y = currY;
    currPage->Append(DrawInstr::Str(s, end - s, bbox));
    currX += dx;
}

#if 0
void DumpAttr(uint8 *s, size_t sLen)
{
    static Vec<char *> seen;
    char *sCopy = str::DupN((char*)s, sLen);
    bool didSee = false;
    for (size_t i = 0; i < seen.Count(); i++) {
        char *tmp = seen.At(i);
        if (str::EqI(sCopy, tmp)) {
            didSee = true;
            break;
        }
    }
    if (didSee) {
        free(sCopy);
        return;
    }
    seen.Append(sCopy);
    printf("%s\n", sCopy);
}
#endif

// tags that I want to explicitly ignore and not define
// HtmlTag enums for them
// One file has a bunch of st1:* tags (st1:city, st1:place etc.)
static bool IgnoreTag(const char *s, size_t sLen)
{
    if (sLen >= 4 && s[3] == ':' && s[0] == 's' && s[1] == 't' && s[2] == '1')
        return true;
    // no idea what "o:p" is
    if (sLen == 3 && s[1] == ':' && s[0] == 'o'  && s[2] == 'p')
        return true;
    return false;
}

void PageLayout::HandleHtmlTag(HtmlToken *t)
{
    CrashAlwaysIf(!t->IsTag());

    // HtmlToken string includes potential attributes,
    // get the length of just the tag
    size_t tagLen = GetTagLen(t);
    if (IgnoreTag(t->s, tagLen))
        return;

    HtmlTag tag = FindTag(t);
    // TODO: ignore instead of crashing once we're satisfied we covered all the tags
    CrashIf(tag == Tag_NotFound);

    if (Tag_P == tag) {
        StartNewLine(true);
        currJustification = Align_Justify;
        if (t->IsStartTag() || t->IsEmptyElementEndTag()) {
            AttrInfo *attrInfo = t->GetAttrByName("align");
            if (attrInfo) {
                AlignAttr just = GetAlignAttrByName(attrInfo->val, attrInfo->valLen);
                if (just != Align_NotFound)
                    currJustification = just;
            }
        }
    } else if (Tag_Hr == tag) {
        EmitLine();
    } else if ((Tag_B == tag) || (Tag_Strong == tag)) {
        ChangeFont(FontStyleBold, t->IsStartTag());
    } else if ((Tag_I == tag) || (Tag_Em == tag)) {
        ChangeFont(FontStyleItalic, t->IsStartTag());
    } else if (Tag_U == tag) {
        ChangeFont(FontStyleUnderline, t->IsStartTag());
    } else if (Tag_Strike == tag) {
        ChangeFont(FontStyleStrikeout, t->IsStartTag());
    } else if (Tag_Mbp_Pagebreak == tag) {
        JustifyLine(currJustification);
        StartNewPage(true);
    } else if (Tag_Br == tag) {
        StartNewLine(false);
    }
}

void PageLayout::EmitText(HtmlToken *t)
{
    CrashIf(!t->IsText());
    const char *end = t->s + t->sLen;
    const char *curr = t->s;
    const char *currStart;
    // break text into runes i.e. chunks taht are either all
    // whitespace or all non-whitespace
    while (curr < end) {
        currStart = curr;
        SkipWs(curr, end);
        if (curr > currStart)
            EmitTextRune(currStart, curr);
        currStart = curr;
        SkipNonWs(curr, end);
        if (curr > currStart)
            EmitTextRune(currStart, curr);
    }
}

// The first instruction in a page is always SetFont() instruction
// so to determine if a page is empty, we check if there's at least
// one "visible" instruction.
static bool IsEmptyPage(PageData *p)
{
    if (!p)
        return true;
    DrawInstr *i;
    for (i = p->drawInstructions.IterStart(); i; i = p->drawInstructions.IterNext()) {
        switch (i->type) {
            case InstrSetFont:
            case InstrLine:
            case InstrSpace:
            case InstrParagraphStart:
                // those are "invisible" instruction. I consider a line invisible
                // (which is different to what Kindel app does), but a page
                // with just lines is not very interesting
                break;
            default:
                return false;
        }
    }
    // all instructions were invisible
    return true;
}

// Return the next parsed page. Returns NULL if finished parsing.
// For simplicity of implementation, we parse xml text node or
// xml element at a time. This might cause a creation of one
// or more pages, which we remeber and send to the caller
// if we detect accumulated pages.
PageData *PageLayout::IterNext()
{
    for (;;)
    {
        // send out all pages accumulated so far
        while (pagesToSend.Count() > 0) {
            PageData *ret = pagesToSend.At(0);
            pagesToSend.RemoveAt(0);
            if (!IsEmptyPage(ret))
                return ret;
            else
                delete ret;
        }
        // we can call ourselves recursively to send outstanding
        // pages after parsing has finished so this is to detect
        // that case and really end parsing
        if (finishedParsing)
            return NULL;
        HtmlToken *t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        if (t->IsTag())
            HandleHtmlTag(t);
        else
            EmitText(t);
    }
    // force layout of the last line
    StartNewLine(true);

    pagesToSend.Append(currPage);
    currPage = NULL;
    // call ourselves recursively to return accumulated pages
    finishedParsing = true;
    return IterNext();
}

Vec<PageData*> *LayoutHtml(LayoutInfo* li)
{
    Vec<PageData*> *pages = new Vec<PageData*>();
    PageLayout l;
    for (PageData *pd = l.IterStart(li); pd; pd = l.IterNext()) {
        pages->Append(pd);
    }
    return pages;
}

void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox)
{
    //SolidBrush brText(Color(0,0,0));
    SolidBrush brText(Color(0x5F, 0x4B, 0x32)); // this color matches Kindle app
    Pen pen(Color(255, 0, 0), 1);
    Pen blackPen(Color(0, 0, 0), 1);
    Font *font = NULL;

    WCHAR buf[512];
    PointF pos;
    DrawInstr *i;
    for (i = drawInstructions->IterStart(); i; i = drawInstructions->IterNext()) {
        RectF bbox = i->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrLine == i->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = bbox.Y + bbox.Height / 2.f;
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (showBbox)
                g->DrawRectangle(&pen, bbox);
            g->DrawLine(&blackPen, p1, p2);
        } else if (InstrString == i->type) {
            size_t strLen = str::Utf8ToWcharBuf(i->str.s, i->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (showBbox)
                g->DrawRectangle(&pen, bbox);
            g->DrawString(buf, strLen, font, pos, NULL, &brText);
        } else if (InstrSetFont == i->type) {
            font = i->setFont.font;
        } else if ((InstrSpace == i->type) || (InstrParagraphStart == i->type)) {
            // ignore
        } else {
            CrashIf(true);
        }
    }
}
