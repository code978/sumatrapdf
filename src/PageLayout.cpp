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
// We don't want to collapse multiple consecutive newlines into
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
    di.font = font;
    return di;
}

DrawInstr DrawInstr::FixedSpace(float dx)
{
    DrawInstr di(InstrFixedSpace);
    di.fixedSpaceDx = dx;
    return di;
}

DrawInstr DrawInstr::Image(char *data, size_t len, RectF bbox)
{
    DrawInstr di(InstrImage);
    di.img.data = data;
    di.img.len = len;
    di.bbox = bbox;
    return di;
}

DrawInstr DrawInstr::LinkStart(size_t pos)
{
    DrawInstr di(InstrLinkStart);
    di.linkFilePos = pos;
    return di;
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

void PageLayout::SetCurrentFont(FontStyle fontStyle, float fontSize)
{
    Font *newFont = mui::GetCachedFont(defaultFontName.Get(), fontSize, fontStyle);
    if (currFont == newFont)
        return;
    currFontStyle = fontStyle;
    currFontSize = fontSize;
    currFont = newFont;
    currLineInstr.Append(DrawInstr::SetFont(currFont));
}

static bool ValidStyleForChangeFontStyle(FontStyle fs)
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
void PageLayout::ChangeFontStyle(FontStyle fs, bool addStyle)
{
    CrashAlwaysIf(!ValidStyleForChangeFontStyle(fs));
    FontStyle newFontStyle = currFontStyle;
    if (addStyle)
        newFontStyle = (FontStyle) (newFontStyle | fs);
    else
        newFontStyle = (FontStyle) (newFontStyle & ~fs);

    SetCurrentFont(newFontStyle, currFontSize);
}

void PageLayout::ChangeFontSize(float fontSize)
{
    SetCurrentFont(currFontStyle, fontSize);
}

// sum of widths of all elements with a fixed size and flexible
// spaces (using minimum value for its width)
REAL PageLayout::CurrLineDx()
{
    REAL dx = 0;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrString == i->type) {
            dx += i->bbox.Width;
        } else if (InstrElasticSpace == i->type) {
            dx += spaceDx;
        } else if (InstrFixedSpace == i->type) {
            dx += i->fixedSpaceDx;
        }
    }
    return dx;
}

// return the height of the tallest element on the line
float PageLayout::CurrLineDy()
{
    float dy = lineSpacing;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if ((InstrString == i->type) || (InstrLine == i->type)) {
            if (i->bbox.Height > dy)
                dy = i->bbox.Height;
        }
    }
    return dy;
}

// When this is called, Width and Height of each element is already set
// We set position x of each visible element
void PageLayout::LayoutLeftStartingAt(REAL offX)
{
    REAL x = offX;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrString == i->type) {
            i->bbox.X = x;
            x += i->bbox.Width;
        } else if (InstrElasticSpace == i->type) {
            x += spaceDx;
        } else if (InstrFixedSpace == i->type) {
            x += i->fixedSpaceDx;
        }
    }
}

// TODO: if elements are of different sizes (e.g. texts using different fonts)
// we should align them according to the baseline (which we would first need to
// record for each element)
static void SetYPos(Vec<DrawInstr>& instr, float y)
{
    for (DrawInstr *i = instr.IterStart(); i; i = instr.IterNext()) {
        CrashIf(0.f != i->bbox.Y);
        i->bbox.Y = y;
    }
}

// Redistribute extra space in the line equally among the spaces
void PageLayout::JustifyLineBoth()
{
    REAL extraSpaceDxTotal = pageDx - CurrLineDx();
    CrashIf(extraSpaceDxTotal < 0.f);
    LayoutLeftStartingAt(0.f);
    size_t spaces = 0;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrElasticSpace == i->type)
            ++spaces;
    }
    if (0 == spaces)
        return;
    // redistribute extra dx space among elastic spaces
    REAL extraSpaceDx = extraSpaceDxTotal / (float)spaces;
    float offX = 0.f;
    DrawInstr *lastStr = NULL;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        i->bbox.X += offX;
        if (InstrElasticSpace == i->type)
            offX += extraSpaceDx;
        else if (InstrString == i->type)
            lastStr = i;
    }
    // align the last element perfectly against the right edge in case
    // we've accumulated rounding errors
    if (lastStr)
        lastStr->bbox.X = pageDx - lastStr->bbox.Width;
}

static bool IsVisibleDrawInstr(DrawInstr *i)
{
    switch (i->type) {
        case InstrString:
        case InstrLine:
        case InstrImage:
            return true;
    }
    return false;
}

// empty page is one that consists of only invisible instructions
bool IsEmptyPage(PageData *p)
{
    if (!p)
        return false;
    DrawInstr *i;
    for (i = p->instructions.IterStart(); i; i = p->instructions.IterNext()) {
        // if a page only consits of lines we consider it empty. It's different
        // than what Kindle does but I don't see the purpose of showing such
        // pages to the user
        if (InstrLine == i->type)
            continue;
        if (IsVisibleDrawInstr(i))
            return false;
    }
    // all instructions were invisible
    return true;
}

bool PageLayout::IsCurrLineEmpty()
{
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (IsVisibleDrawInstr(i))
            return false;
    }
    return true;
}

// justifies current line and returns line dy (i.e. height of the tallest element)
void PageLayout::JustifyCurrLine(AlignAttr align)
{
    switch (align) {
        case Align_Left:
            LayoutLeftStartingAt(0.f);
            break;
        case Align_Right:
            LayoutLeftStartingAt(pageDx - CurrLineDx());
            break;
        case Align_Center:
            LayoutLeftStartingAt((pageDx - CurrLineDx()) / 2.f);
            break;
        case Align_Justify:
            JustifyLineBoth();
            break;
        default:
            CrashIf(true);
            break;
    }
}

void PageLayout::ForceNewPage()
{
    bool createdNewPage = FlushCurrLine(true);
    if (createdNewPage)
        return;
    pagesToSend.Append(currPage);
    pageCount++;
    currPage = new PageData();
    currPage->reparsePoint = currReparsePoint;

    currPage->instructions.Append(DrawInstr::SetFont(currFont));
    currY = 0.f;
    currX = 0.f;
    currJustification = Align_Justify;
    currLineTopPadding = 0.f;
}

// returns true if created a new page
bool PageLayout::FlushCurrLine(bool isParagraphBreak)
{
    if (IsCurrLineEmpty())
        return false;
    AlignAttr align = currJustification;
    if (isParagraphBreak && (Align_Justify == align))
        align = Align_Left;
    JustifyCurrLine(align);

    // create a new page if necessary
    float totalLineDy = CurrLineDy() + currLineTopPadding;
    PageData *newPage = NULL;
    if (currY + totalLineDy > pageDy) {
        pagesToSend.Append(currPage);
        pageCount++;
        // current line too big to fit in current page,
        // so need to start another page
        currY = 0.f;
        newPage = new PageData();
        newPage->reparsePoint = currReparsePoint;
    }
    SetYPos(currLineInstr, currY + currLineTopPadding);
    currY += totalLineDy;

    if (newPage) {
        // instructions for each page need to be self-contained
        // so we have to carry over some state (like current font)
        CrashIf(!currFont);
        newPage->instructions.Append(DrawInstr::SetFont(currFont));
        currPage = newPage;
    }
    currPage->instructions.Append(currLineInstr.LendData(), currLineInstr.Count());
    currLineInstr.Reset();
    currLineTopPadding = 0;
    currX = 0;
    return (newPage != NULL);
}

void PageLayout::EmitEmptyLine(float lineDy)
{
    CrashIf(!IsCurrLineEmpty());
    currY += lineDy;
    if (currY <= pageDy)
        return;
    ForceNewPage();
}

void PageLayout::EmitImage(ImageData *img)
{
    Rect imgSize = BitmapSizeFromData(img->data, img->len);
    if (imgSize.IsEmptyArea())
        return;

    // if the image we're adding early on is the same as cover
    // image, then skip it. 5 is a heuristic
    if ((img == coverImage) && (pageCount < 5))
        return;

    FlushCurrLine(true);
    // TODO: probably should respect current paragraph justification
    RectF bbox(0, 0, (REAL)imgSize.Width, (REAL)imgSize.Height);
    if (currY  + (bbox.Height / 2.f) > pageDy) {
        // move overly large images to a new page
        ForceNewPage();
    }

    if (bbox.Width > pageDx || bbox.Height > pageDy - currY) {
        // resize still too large images to fit a page
        REAL scale = min(pageDx / bbox.Width, (pageDy - currY) / bbox.Height);
        bbox.Width *= scale;
        bbox.Height *= scale;
    }
    currX += (pageDx - bbox.Width) / 2.f;
    bbox.X = currX;
    bbox.Y = currY;
    currPage->instructions.Append(DrawInstr::Image(img->data, img->len, bbox));
    currY += bbox.Height;
    ForceNewPage();
}

// add horizontal line (<hr> in html terms)
void PageLayout::EmitHr()
{
    // hr creates an implicit paragraph break
    FlushCurrLine(true);
    CrashIf(0 != currX);
    RectF bbox(0.f, 0.f, pageDx, lineSpacing);
    currLineInstr.Append(DrawInstr(InstrLine, bbox));
    FlushCurrLine(true);
}

static bool ShouldAddIndent(float indent, AlignAttr just)
{
    if (0.f == indent)
        return false;
    return (Align_Left == just) || (Align_Justify == just);
}

void PageLayout::EmitParagraph(float indent, float topPadding)
{
    FlushCurrLine(true);
    CrashIf(0 != currX);
    if (ShouldAddIndent(indent, currJustification) && EnsureDx(indent)) {
        currLineInstr.Append(DrawInstr::FixedSpace(indent));
        currX += indent;
    }
    // remember so that we can use it in FlushCurrLine()
    currLineTopPadding = topPadding;
}

// ensure there is enough dx space left in the current line
// if there isn't, we start a new line
// returns false if dx is bigger than pageDx
bool PageLayout::EnsureDx(float dx)
{
    if (currX + dx <= pageDx)
        return true;
    FlushCurrLine(false);
    return dx <= pageDx;
}

// don't emit multiple spaces and don't emit spaces
// at the beginning of the line
static bool CanEmitElasticSpace(float currX, float maxCurrX, Vec<DrawInstr>& currLineInstr)
{
    if (0 == currX)
        return false;
    // prevent elastic spaces from being flushed to the
    // beginning of the next line
    if (currX > maxCurrX)
        return false;
    DrawInstr& di = currLineInstr.Last();
    return (InstrElasticSpace != di.type) && (InstrFixedSpace != di.type);
}

void PageLayout::EmitNewLine()
{
    // a single newline is considered "soft" and ignored
    // two or more consecutive newlines are considered a
    // single paragraph break
    newLinesCount++;
    if (2 == newLinesCount)
        FlushCurrLine(true);
}

void PageLayout::EmitElasticSpace()
{
    if (!CanEmitElasticSpace(currX, pageDx - spaceDx, currLineInstr))
        return;
    EnsureDx(spaceDx);
    currX += spaceDx;
    currLineInstr.Append(DrawInstr(InstrElasticSpace));
}

// a text rune is a string of consecutive text with uniform style
void PageLayout::EmitTextRun(const char *s, const char *end)
{
    CrashIf(IsSpaceOnly(s, end));
    const char *tmp = ResolveHtmlEntities(s, end, textAllocator);
    if (tmp != s) {
        s = tmp;
        end = s + str::Len(s);
    }

    size_t strLen = str::Utf8ToWcharBuf(s, end - s, buf, dimof(buf));
    RectF bbox = MeasureText(gfx, currFont, buf, strLen);
    bbox.Y = 0.f;
    EnsureDx(bbox.Width);
    if (bbox.Width > pageDx) {
        int lenThatFits = StringLenForWidth(gfx, currFont, buf, strLen, pageDx);
        bbox = MeasureText(gfx, currFont, buf, lenThatFits);
        bbox.Y = 0.f;
        CrashIf(bbox.Width > pageDx);
        currLineInstr.Append(DrawInstr::Str(s, lenThatFits, bbox));
        currX += bbox.Width;
        const char *newS = s + lenThatFits;
        if (end == newS)
            return;
        EmitTextRun(newS, end);
    } else {
        currLineInstr.Append(DrawInstr::Str(s, end - s, bbox));
        currX += bbox.Width;
    }
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

// parse the number in s as a float
static float ParseFloat(const char *s, size_t len)
{
    str::Str<char> sCopy;
    sCopy.Append(s, len);
    char *toParse = sCopy.Get();
    float x = 0;
    str::Parse(toParse, "%f", &x);
    return x;
}

// parses size in the form "1em" or "3pt". To interpret ems we need emInPoints
// to be passed by the caller
static float ParseSizeAsPixels(const char *s, size_t len, float emInPoints)
{
    str::Str<char> sCopy;
    sCopy.Append(s, len);
    char *toParse = sCopy.Get();
    float x = 0;
    float sizeInPoints = 0;
    if (str::Parse(toParse, "%fem", &x)) {
        sizeInPoints = x * emInPoints;
    } else if (str::Parse(toParse, "%fpt", &x)) {
        sizeInPoints = x;
    }
    // TODO: take dpi into account
    float sizeInPixels = sizeInPoints;
    return sizeInPixels;
}

void PageLayout::HandleTagBr()
{
    // Trying to match Kindle behavior
    if (IsCurrLineEmpty())
        EmitEmptyLine(lineSpacing);
    else
        FlushCurrLine(true);
}

// TODO: this only accepts <a> tags with filepos attribute
// there are files with regular http links etc. - not sure if we should
// support them. Sometimes they point to suspicious, dead web pages
void PageLayout::HandleTagA(HtmlToken *t)
{
    if (t->IsEndTag()) {
        if (inLink)
            currPage->instructions.Append(DrawInstr(InstrLinkEnd));
        inLink = false;
        return;
    }
    AttrInfo *attr = t->GetAttrByName("filepos");
    if (!attr) 
        return;
    // TODO: would be nice to have str::Parse() that can operate on string with a given
    // length, not only on zero-terminated strings
    str::Str<char> nStr;
    nStr.Append(attr->val, attr->valLen);
    int n = 0;
    if (!str::Parse(nStr.Get(), "%d", &n))
        return;
    if (n < 0)
        return;
    if ((size_t)n >= layoutInfo->htmlStrLen)
        return;
    inLink = true;
    currPage->instructions.Append(DrawInstr::LinkStart((size_t)n));
}

void PageLayout::HandleTagP(HtmlToken *t)
{
    if (t->IsEndTag()) {
        FlushCurrLine(true);
        currJustification = Align_Justify;
        return;
    }

    // handle a start p tag
    AlignAttr just = Align_NotFound;
    AttrInfo *attr = t->GetAttrByName("align");
    if (attr) 
        just = GetAlignAttrByName(attr->val, attr->valLen);;
    if (just != Align_NotFound)
        currJustification = just;
    // best I can tell, in mobi <p width="1em" height="3pt> means that
    // the first line of the paragrap is indented by 1em and there's
    /// 3pt top padding
    float lineIndent = 0;
    float topPadding = 0;
    attr = t->GetAttrByName("width");
    if (attr) {
        lineIndent = ParseSizeAsPixels(attr->val, attr->valLen, currFontSize);
        // there are files with negative width which produces partially invisible
        // text, so don't allow that
        if (lineIndent < 0.f)
            lineIndent = 0.f;
    }
    attr = t->GetAttrByName("height");
    if (attr)
        topPadding = ParseSizeAsPixels(attr->val, attr->valLen, currFontSize);
    EmitParagraph(lineIndent, topPadding);
}

// mobi format has image tags in the form:
// <img recindex="0000n" alt=""/>
// where recindex is the record number of pdb record
// that holds the image (within image record array, not a
// global record)
// TODO: handle alt attribute (?)
void PageLayout::HandleTagImg(HtmlToken *t)
{
    AttrInfo *attr = t->GetAttrByName("recindex");
    if (!attr)
        return;
    str::Str<char> nStr;
    nStr.Append(attr->val, attr->valLen);
    int n = 0;
    if (!str::Parse(nStr.Get(), "%d", &n))
        return;
    ImageData *img = layoutInfo->mobiDoc->GetImage(n);
    if (!img)
        return;

    EmitImage(img);
}

void PageLayout::HandleTagFont(HtmlToken *t)
{
    if (t->IsEndTag()) {
        ChangeFontSize(defaultFontSize);
        return;
    }

    AttrInfo *attr = t->GetAttrByName("name");
    float size;
    if (attr) {
        // TODO: also use font name (not sure if mobi documents use that)
        CrashIf(true);
        size = 0; // only so that we can set a breakpoing
    }

    attr = t->GetAttrByName("size");
    if (!attr)
        return;
    size = ParseFloat(attr->val, attr->valLen);
    // the sizes seem to be in 3-6 range. I try to convert it to
    // relative sizes that visually match Kindle app
    if (size < 1.f)
        size = 1.f;
    else if (size > 10.f)
        size = 10.f;
    float scale = 1.f + (size/10.f * .4f);
    ChangeFontSize(defaultFontSize * scale);
}

// List of tags that we don't know we don't handle yet (either because we don't want
// to or because we haven't implemented them yet)
static bool IgnoreThisTag(HtmlTag tag)
{
    static uint8 tagsToIgnore[] = { Tag_Html, Tag_Body, Tag_Head, Tag_Guide, 
        Tag_Reference, Tag_Table, Tag_Tr, Tag_Td, Tag_Tt };
    return IsInArray((uint8)tag, tagsToIgnore, dimof(tagsToIgnore));
}

static bool IsTagH(HtmlTag tag)
{
    switch (tag) {
        case Tag_H1:
        case Tag_H2:
        case Tag_H3:
        case Tag_H4:
        case Tag_H5:
            return true;
    }
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
        HandleTagP(t);
    } else if (Tag_Hr == tag) {
        // imitating Kindle: hr is proceeded by an empty line
        FlushCurrLine(false);
        EmitEmptyLine(lineSpacing);
        EmitHr();
    } else if ((Tag_B == tag) || (Tag_Strong == tag)) {
        ChangeFontStyle(FontStyleBold, t->IsStartTag());
    } else if ((Tag_I == tag) || (Tag_Em == tag)) {
        ChangeFontStyle(FontStyleItalic, t->IsStartTag());
    } else if (Tag_U == tag) {
        ChangeFontStyle(FontStyleUnderline, t->IsStartTag());
    } else if (Tag_Strike == tag) {
        ChangeFontStyle(FontStyleStrikeout, t->IsStartTag());
    } else if (Tag_Mbp_Pagebreak == tag) {
        ForceNewPage();
    } else if (Tag_Br == tag) {
        HandleTagBr();
    } else if (Tag_Font == tag) {
        HandleTagFont(t);
    } else if (Tag_Img == tag) {
        HandleTagImg(t);
    } else if (Tag_A == tag) {
        HandleTagA(t);
    } else if (Tag_Blockquote == tag) {
        // TODO: implement me
    } else if (Tag_Div == tag) {
        // TODO: implement me
    } else if (IsTagH(tag)) {
        // TODO: implement me
    } else if (Tag_Sup == tag) {
        // TODO: implement me
    } else if (Tag_Sub == tag) {
        // TODO: implement me
    } else if (Tag_Span == tag) {
        // TODO: implement me
    } else {
        // TODO: temporary debugging
        CrashIf(!IgnoreThisTag(tag));
    }
}

void PageLayout::HandleText(HtmlToken *t)
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
        // collapse multiple, consecutive white-spaces into a single space
        if (curr > currStart) {
            if (IsNewline(currStart, curr))
                EmitNewLine();
            else
                EmitElasticSpace();
        }
        currStart = curr;
        SkipNonWs(curr, end);
        if (curr > currStart) {
            EmitTextRun(currStart, curr);
            newLinesCount = 0;
        }
    }
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
            if (IsEmptyPage(ret))
                delete ret;
            else
                return ret;
        }
        // we can call ourselves recursively to send outstanding
        // pages after parsing has finished so this is to detect
        // that case and really end parsing
        if (finishedParsing)
            return NULL;
        HtmlToken *t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        currReparsePoint = t->GetReparsePoint();
        if (t->IsTag())
            HandleHtmlTag(t);
        else
            HandleText(t);
    }
    // force layout of the last line
    FlushCurrLine(true);

    pagesToSend.Append(currPage);
    currPage = NULL;
    // call ourselves recursively to return accumulated pages
    finishedParsing = true;
    return IterNext();
}

PageData *PageLayout::IterStart(LayoutInfo* li)
{
    CrashIf(currPage);
    finishedParsing = false;
    layoutInfo = li;
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;
    textAllocator = layoutInfo->textAllocator;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    defaultFontName.Set(str::Dup(layoutInfo->fontName));
    defaultFontSize = layoutInfo->fontSize;
    SetCurrentFont(FontStyleRegular, defaultFontSize);

    coverImage = NULL;
    pageCount = 0;
    inLink = false;

    lineSpacing = currFont->GetHeight(gfx);
    spaceDx = currFontSize / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(gfx, currFont);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    currJustification = Align_Justify;
    currX = 0; currY = 0;
    currPage = new PageData;
    currPage->reparsePoint = currReparsePoint;

    currLineTopPadding = 0;
    if (layoutInfo->mobiDoc) {
        ImageData *img = layoutInfo->mobiDoc->GetCoverImage();
        if (img) {
            EmitImage(img);
            coverImage = img; // must do that after EmitImage()
        }
    }
    return IterNext();
}

// TODO: draw link in the appropriate format (blue text, unerlined, should show hand cursor when
// mouse is over a link. There's a slight complication here: we only get explicit information about
// strings, not about the whitespace and we should underline the whitespace as well. Also the text
// should be underlined at a baseline
void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox)
{
    //SolidBrush brText(Color(0,0,0));
    SolidBrush brText(Color(0x5F, 0x4B, 0x32)); // this color matches Kindle app
    Pen pen(Color(255, 0, 0), 1);
    //Pen linePen(Color(0, 0, 0), 2.f);
    Pen linePen(Color(0x5F, 0x4B, 0x32), 2.f);
    Font *font = NULL;

    WCHAR buf[512];
    PointF pos;
    DrawInstr *i;
    bool insideLink = false;
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
            g->DrawLine(&linePen, p1, p2);
        } else if (InstrString == i->type) {
            size_t strLen = str::Utf8ToWcharBuf(i->str.s, i->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (showBbox)
                g->DrawRectangle(&pen, bbox);
            g->DrawString(buf, strLen, font, pos, NULL, &brText);
        } else if (InstrSetFont == i->type) {
            font = i->font;
        } else if ((InstrElasticSpace == i->type) || (InstrFixedSpace == i->type)) {
            // ignore
        } else if (InstrImage == i->type) {
            // TODO: cache the bitmap somewhere (?)
            Bitmap *bmp = BitmapFromData(i->img.data, i->img.len);
            if (bmp)
                g->DrawImage(bmp, bbox, 0, 0, (REAL)bmp->GetWidth(), (REAL)bmp->GetHeight(), UnitPixel);
            delete bmp;
        } else if (InstrLinkStart == i->type) {
            insideLink = true;
        } else if (InstrLinkEnd == i->type) {
            insideLink = false;
        } else {
            CrashIf(true);
        }
    }
}
