/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Allocator.h"

#include "HtmlPullParser.h"
#include "StrUtil.h"

// map of entity names to their Unicde runes, based on
// http://en.wikipedia.org/wiki/List_of_XML_and_HTML_character_entity_references
// the order of strings in gHtmlEntityNames corresponds to
// order of Unicode runes in gHtmlEntityRunes
static const char *gHtmlEntityNames = "AElig\0Aacute\0Acirc\0Agrave\0Alpha\0Aring\0Atilde\0Auml\0Beta\0Ccedil\0Chi\0Dagger\0Delta\0ETH\0Eacute\0Ecirc\0Egrave\0Epsilon\0Eta\0Euml\0Gamma\0Iacute\0Icirc\0Igrave\0Iota\0Iuml\0Kappa\0Lambda\0Mu\0Ntilde\0Nu\0OElig\0Oacute\0Ocirc\0Ograve\0Omega\0Omicron\0Oslash\0Otilde\0Ouml\0Phi\0Pi\0Prime\0Psi\0Rho\0Scaron\0Sigma\0THORN\0Tau\0Theta\0Uacute\0Ucirc\0Ugrave\0Upsilon\0Uuml\0Xi\0Yacute\0Yuml\0Zeta\0aacute\0acirc\0acute\0aelig\0agrave\0alefsym\0alpha\0amp\0and\0ang\0apos\0aring\0asymp\0atilde\0auml\0bdquo\0beta\0brvbar\0bull\0cap\0ccedil\0cedil\0cent\0chi\0circ\0clubs\0cong\0copy\0crarr\0cup\0curren\0dArr\0dagger\0darr\0deg\0delta\0diams\0divide\0eacute\0ecirc\0egrave\0empty\0emsp\0ensp\0epsilon\0equiv\0eta\0eth\0euml\0euro\0exist\0fnof\0forall\0frac12\0frac14\0frac34\0frasl\0gamma\0ge\0gt\0hArr\0harr\0hearts\0hellip\0iacute\0icirc\0iexcl\0igrave\0image\0infin\0int\0iota\0iquest\0isin\0iuml\0kappa\0lArr\0lambda\0lang\0laquo\0larr\0lceil\0ldquo\0le\0lfloor\0lowast\0loz\0lrm\0lsaquo\0lsquo\0lt\0macr\0mdash\0micro\0middot\0minus\0mu\0nabla\0nbsp\0ndash\0ne\0ni\0not\0notin\0nsub\0ntilde\0nu\0oacute\0ocirc\0oelig\0ograve\0oline\0omega\0omicron\0oplus\0or\0ordf\0ordm\0oslash\0otilde\0otimes\0ouml\0para\0part\0permil\0perp\0phi\0pi\0piv\0plusmn\0pound\0prime\0prod\0prop\0psi\0quot\0rArr\0radic\0rang\0raquo\0rarr\0rceil\0rdquo\0real\0reg\0rfloor\0rho\0rlm\0rsaquo\0rsquo\0sbquo\0scaron\0sdot\0sect\0shy\0sigma\0sigmaf\0sim\0spades\0sub\0sube\0sum\0sup\0sup1\0sup2\0sup3\0supe\0szlig\0tau\0there4\0theta\0thetasym\0thinsp\0thorn\0tilde\0times\0trade\0uArr\0uacute\0uarr\0ucirc\0ugrave\0uml\0upsih\0upsilon\0uuml\0weierp\0xi\0yacute\0yen\0yuml\0zeta\0zwj\0zwnj\0";

static uint16 gHtmlEntityRunes[] = { 198, 193, 194, 192, 913, 197, 195, 196, 914, 199, 935, 8225, 916, 208, 201, 202, 200, 917, 919, 203, 915, 205, 206, 204, 921, 207, 922, 923, 924, 209, 925, 338, 211, 212, 210, 937, 927, 216, 213, 214, 934, 928, 8243, 936, 929, 352, 931, 222, 932, 920, 218, 219, 217, 933, 220, 926, 221, 376, 918, 225, 226, 180, 230, 224, 8501, 945, 38, 8743, 8736, 39, 229, 8776, 227, 228, 8222, 946, 166, 8226, 8745, 231, 184, 162, 967, 710, 9827, 8773, 169, 8629, 8746, 164, 8659, 8224, 8595, 176, 948, 9830, 247, 233, 234, 232, 8709, 8195, 8194, 949, 8801, 951, 240, 235, 8364, 8707, 402, 8704, 189, 188, 190, 8260, 947, 8805, 62, 8660, 8596, 9829, 8230, 237, 238, 161, 236, 8465, 8734, 8747, 953, 191, 8712, 239, 954, 8656, 955, 9001, 171, 8592, 8968, 8220, 8804, 8970, 8727, 9674, 8206, 8249, 8216, 60, 175, 8212, 181, 183, 8722, 956, 8711, 160, 8211, 8800, 8715, 172, 8713, 8836, 241, 957, 243, 244, 339, 242, 8254, 969, 959, 8853, 8744, 170, 186, 248, 245, 8855, 246, 182, 8706, 8240, 8869, 966, 960, 982, 177, 163, 8242, 8719, 8733, 968, 34, 8658, 8730, 9002, 187, 8594, 8969, 8221, 8476, 174, 8971, 961, 8207, 8250, 8217, 8218, 353, 8901, 167, 173, 963, 962, 8764, 9824, 8834, 8838, 8721, 8835, 185, 178, 179, 8839, 223, 964, 8756, 952, 977, 8201, 254, 732, 215, 8482, 8657, 250, 8593, 251, 249, 168, 978, 965, 252, 8472, 958, 253, 165, 255, 950, 8205, 8204 };

#define MAX_ENTITY_NAME_LEN 8
#define MAX_ENTITY_CHAR     122

// returns -1 if didn't find
int HtmlEntityNameToRune(const char *name, size_t nameLen)
{
    if (nameLen > MAX_ENTITY_NAME_LEN)
        return -1;
    int pos = str::FindStrPos(gHtmlEntityNames, name, nameLen);
    if (-1 == pos)
        return -1;
    return (int)gHtmlEntityRunes[pos];
}

static uint8 gSelfClosingTags[] = { Tag_Area, Tag_Base, Tag_Basefont, Tag_Br, Tag_Col, Tag_Frame, Tag_Hr, Tag_Img, Tag_Input, Tag_Link, Tag_Mbp_Pagebreak, Tag_Meta, Tag_Pagebreak, Tag_Param };

bool IsTagSelfClosing(HtmlTag tag)
{
    CrashIf(tag > 255);
    CrashIf(Tag_Last > 255);
    uint8 tagNo = (uint8)tag;
    for (size_t i = 0; i < dimof(gSelfClosingTags); i++) {
        if (tagNo == gSelfClosingTags[i])
            return true;
    }
    return false;
}

bool IsTagSelfClosing(const char *s, size_t len)
{
    HtmlTag tag = FindTag(s, len);
    return IsTagSelfClosing(tag);
}

static bool SkipUntil(const char*& s, const char *end, char c)
{
    while ((s < end) && (*s != c)) {
        ++s;
    }
    return *s == c;
}

static bool IsWs(int c) {
    return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

void SkipWs(const char* & s, const char *end)
{
    while ((s < end) && IsWs(*s)) {
        ++s;
    }
}

void SkipNonWs(const char* & s, const char *end)
{
    while ((s < end) && !IsWs(*s)) {
        ++s;
    }
}

static int IsNameChar(int c)
{
    return c == '.' || c == '-' || c == '_' || c == ':' ||
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

// skip all html tag or attribute characters
static void SkipName(const char*& s, const char *end)
{
    while ((s < end) && IsNameChar(*s)) {
        s++;
    }
}

// return true if s consists only of whitespace
bool IsSpaceOnly(const char *s, const char *end)
{
    SkipWs(s, end);
    return s == end;
}

void MemAppend(char *& dst, const char *s, size_t len)
{
    if (0 == len)
        return;
    memcpy(dst, s, len);
    dst += len;
}

// rune is a unicode character (borrowing terminology from Go)
// We encode rune as utf8 to dst buffer and advance dst pointer.
// The caller must ensure there is enough free space (6 bytes)
// in dst
static void Utf8Encode(char *& dst, int rune)
{
    uint8 *tmp = (uint8*)dst;
    if (rune <= 0x7f)
        *tmp++ = (uint8)rune;
    else {
        // TODO: implement me
        CrashIf(true);
    }
    dst = (char*)tmp;
}

// if "&foo;" was the entity, s points at the char
// after '&' and len is lenght of the string (3 in case of "foo")
// the caller must ensure that there is terminating ';'
static int ResolveHtmlEntity(const char *s, size_t len)
{
    int rune;
    if (str::Parse(s, "%d;", &rune))
        return rune;
    if (str::Parse(s, "#%x;", &rune))
        return rune;
    return HtmlEntityNameToRune(s, len);
}

// if s doesn't contain html entities, we just return it
// if it contains html entities, we'll return string allocated
// with alloc in which entities are converted to their values
// Entities are encoded as utf8 in the result.
// alloc can be NULL, in which case we'll allocate with malloc()
const char *ResolveHtmlEntities(const char *s, const char *end, Allocator *alloc)
{
    char *        res = NULL;
    size_t        resLen;
    char *        dst;
    const char *  colon;

    const char *curr = s;
    for (;;) {
        bool found = SkipUntil(curr, end, '&');
        if (!found) {
            if (!res)
                return s;
            // copy the remaining string
            MemAppend(dst, s, end - s);
            break;
        }
        if (!res) {
            // allocate memory for the result string
            // I'm banking that text after resolving entities will
            // be smaller than the original
            resLen = end - s + 8; // +8 just in case
            res = (char*)Allocator::Alloc(alloc, resLen);
            dst = res;
        }
        MemAppend(dst, s, curr - s);
        // curr points at '&'. Make sure there is ';' within source string
        colon = curr;
        found = SkipUntil(colon, end, ';');
        if (!found) {
            MemAppend(dst, curr, end - curr);
            break;
        }
        int rune = ResolveHtmlEntity(curr+1, colon - curr - 1);
        if (-1 == rune) {
            // unknown entity, copy the string verbatim
            MemAppend(dst, curr, colon - curr + 1);
        } else {
            Utf8Encode(dst, rune);
        }
        curr = colon + 1;
        s = curr;
    }
    *dst = 0;
    CrashIf(dst >= res + resLen);
    return (const char*)res;
}

static bool StrLenIs(const char *s, size_t len, const char *s2)
{
    return str::Len(s2) == len && str::StartsWith(s, s2);
}

bool AttrInfo::NameIs(const char *s) const
{
    return StrLenIs(name, nameLen, s);
}

bool AttrInfo::ValIs(const char *s) const
{
    return StrLenIs(val, valLen, s);
}

void HtmlToken::SetValue(TokenType new_type, const char *new_s, const char *end)
{
    type = new_type;
    s = new_s;
    sLen = end - s;
    nextAttr = NULL;
}

void HtmlToken::SetError(ParsingError err, const char *errContext)
{
    type = Error;
    error = err;
    s = errContext;
}

bool HtmlToken::NameIs(const char *name) const
{
    return  (str::Len(name) == GetTagLen(this)) && str::StartsWith(s, name);
}

// reparse point is an address within html that we can
// can feed to HtmlPullParser() to start parsing from that point
const char *HtmlToken::GetReparsePoint() const
{
    if (IsStartTag() || IsEmptyElementEndTag())
        return s - 1;
    if (IsEndTag())
        return s - 2;
    if (IsText())
        return s;
    CrashIf(true); // don't call us on error tokens
    return NULL;
}

AttrInfo *HtmlToken::GetAttrByName(const char *name)
{
    nextAttr = NULL; // start from the beginning
    for (AttrInfo *a = NextAttr(); a; a = NextAttr()) {
        if (a->NameIs(name))
            return a;
    }
    return NULL;
}

// We expect:
// whitespace | attribute name | = | attribute value
// where attribute value can be quoted
AttrInfo *HtmlToken::NextAttr()
{
    // start after the last attribute found (or the beginning)
    const char *curr = nextAttr;
    if (!curr)
        curr = s + GetTagLen(this);
    const char *end = s + sLen;

    // parse attribute name
    SkipWs(curr, end);
    if (curr == end) {
NoNextAttr:
        nextAttr = NULL;
        return NULL;
    }
    attrInfo.name = curr;
    SkipName(curr, end);
    attrInfo.nameLen = curr - attrInfo.name;
    if (0 == attrInfo.nameLen)
        goto NoNextAttr;
    SkipWs(curr, end);
    if ((curr == end) || ('=' != *curr)) {
        // attributes without values get their names as value in HTML
        attrInfo.val = attrInfo.name;
        attrInfo.valLen = attrInfo.nameLen;
        nextAttr = curr;
        return &attrInfo;
    }

    // parse attribute value
    ++curr; // skip '='
    SkipWs(curr, end);
    if (curr == end) {
        // attribute with implicit empty value
        attrInfo.val = curr;
        attrInfo.valLen = 0;
    } else if (('\'' == *curr) || ('\"' == *curr)) {
        // attribute with quoted value
        ++curr;
        attrInfo.val = curr;
        if (!SkipUntil(curr, end, *(curr - 1)))
            goto NoNextAttr;
        attrInfo.valLen = curr - attrInfo.val;
        ++curr;
    } else {
        attrInfo.val = curr;
        SkipNonWs(curr, end);
        attrInfo.valLen = curr - attrInfo.val;
    }
    nextAttr = curr;
    return &attrInfo;
}

// Given a tag like e.g.:
// <tag attr=">" />
// tries to find the closing '>' and not be confused by '>' that
// are part of attribute value. We're not very strict here
// Returns false if didn't find 
static bool SkipUntilTagEnd(const char*& s, const char *end)
{
    while (s < end) {
        char c = *s++;
        if ('>' == c) {
            --s;
            return true;
        }
        if (('\'' == c) || ('"' == c)) {
            if (!SkipUntil(s, end, c))
                return false;
            ++s;
        }
    }
    return false;
}

// record the tag for the purpose of building current state
// of html tree
static void RecordStartTag(Vec<HtmlTag>* tagNesting, HtmlTag tag)
{
    if (!IsTagSelfClosing(tag))
        tagNesting->Append(tag);
}

// remove the tag from state of html tree
static void RecordEndTag(Vec<HtmlTag> *tagNesting, HtmlTag tag)
{
    // when closing a tag, if the top tag doesn't match but
    // there are only potentially self-closing tags on the
    // stack between the matching tag, we pop all of them
    if (tagNesting->Find(tag)) {
        while ((tagNesting->Count() > 0) && (tagNesting->Last() != tag))
            tagNesting->Pop();
    }
    if (tagNesting->Count() > 0) {
        CrashIf(tagNesting->Last() != tag);
        tagNesting->Pop();
    }
}

static void UpdateTagNesting(Vec<HtmlTag> *tagNesting, HtmlToken *t)
{
    // update the current state of html tree
     HtmlTag tag = FindTag(t);
    if (t->IsStartTag())
        RecordStartTag(tagNesting, tag);
    else if (t->IsEndTag())
        RecordEndTag(tagNesting, tag);
}

// Returns next part of html or NULL if finished
HtmlToken *HtmlPullParser::Next()
{
    if (currPos >= end)
        return NULL;

Next:
    const char *start = currPos;
    if (*currPos != '<') {
        // this must text between tags
        if (!SkipUntil(currPos, end, '<')) {
            // ignore whitespace after the last tag
            if (IsSpaceOnly(start, currPos))
                return NULL;
            // text cannot be at the end
            currToken.SetError(HtmlToken::NonTagAtEnd, start);
        } else {
            currToken.SetValue(HtmlToken::Text, start, currPos);
        }
        return &currToken;
    }

    // '<' - tag begins
    ++start;

    if (!SkipUntilTagEnd(currPos, end)) {
        currToken.SetError(HtmlToken::UnclosedTag, start);
        return &currToken;
    }

    CrashIf('>' != *currPos);
    if (currPos == start) {
        // skip empty tags (<>), because we're lenient
        ++currPos;
        goto Next;
    }

    // skip <? and <! (processing instructions and comments)
    if (('?' == *start) || ('!' == *start)) {
        ++currPos;
        goto Next;
    }

    if (('/' == *start) && ('/' == currPos[-1])) { // </foo/>
        currToken.SetError(HtmlToken::InvalidTag, start);
    } else if ('/' == *start) { // </foo>
        currToken.SetValue(HtmlToken::EndTag, start + 1, currPos);
    } else if ('/' == currPos[-1]) { // <foo/>
        currToken.SetValue(HtmlToken::EmptyElementTag, start, currPos - 1);
    } else {
        currToken.SetValue(HtmlToken::StartTag, start, currPos);
    }
    ++currPos;
    UpdateTagNesting(&tagNesting, &currToken);
    return &currToken;
}

HtmlTag FindTag(const char *s, size_t len)
{
    if (-1 == len)
        len = str::Len(s);
    return (HtmlTag)str::FindStrPosI(HTML_TAGS_STRINGS, s, len);
}

HtmlTag FindTag(HtmlToken *tok)
{
    return FindTag(tok->s, GetTagLen(tok));
}

#if 0
HtmlAttr FindAttr(AttrInfo *attrInfo)
{
    return (HtmlAttr)str::FindStrPosI(HTML_ATTRS_STRINGS, attrInfo->name, attrInfo->nameLen);
}
#endif

AlignAttr GetAlignAttrByName(const char *attr, size_t len)
{
    return (AlignAttr)str::FindStrPosI(ALIGN_ATTRS_STRINGS, attr, len);
}

size_t GetTagLen(const HtmlToken *tok)
{
    for (size_t i = 0; i < tok->sLen; i++) {
        if (!IsNameChar(tok->s[i]))
            return i;
    }
    return tok->sLen;
}

static bool IsInlineTag(HtmlTag tag)
{
    switch (tag) {
    case Tag_A: case Tag_Abbr: case Tag_Acronym: case Tag_B:
    case Tag_Br: case Tag_Em: case Tag_Font: case Tag_I:
    case Tag_Img: case Tag_S: case Tag_Small: case Tag_Span:
    case Tag_Strike: case Tag_Strong: case Tag_Sub: case Tag_Sup:
    case Tag_U:
        return true;
    default:
        return false;
    };
}

static void HtmlAddWithNesting(str::Str<char>* out, HtmlToken *tok, HtmlTag tag, size_t nesting)
{
    CrashIf(!tok->IsStartTag() && !tok->IsEndTag() && !tok->IsEmptyElementEndTag());
    bool isInline = IsInlineTag(tag);
    // add a newline before block start tags (unless there already is one)
    bool onNewLine = out->Count() == 0 || out->Last() == '\n';
    if (!onNewLine && !isInline && !tok->IsEndTag()) {
        out->Append('\n');
        onNewLine = true;
    }
    // indent the tag if it starts on a new line
    if (onNewLine) {
        for (size_t i = 0; i < nesting; i++)
            out->Append('\t');
        if (tok->IsEndTag() && nesting > 0)
            out->Pop();
    }
    // output the tag and all its attributes
    out->Append('<');
    if (tok->IsEndTag())
        out->Append('/');
    // TODO: normalize whitespace between attributes?
    out->Append(tok->s, tok->sLen);
    if (tok->IsEmptyElementEndTag())
        out->Append('/');
    out->Append('>');
    // add a newline after block end tags
    if (!isInline && !tok->IsStartTag())
        out->Append('\n');
}

char *PrettyPrintHtml(const char *s, size_t len, size_t& lenOut)
{
    str::Str<char> res(len);
    HtmlPullParser parser(s, len);
    HtmlToken *t;
    while ((t = parser.Next()) && !t->IsError())
    {
        if (t->IsText())
            res.Append(t->s, t->sLen);
        if (!t->IsTag())
            continue;

        size_t nesting = 0;
        for (HtmlTag *tag = parser.tagNesting.IterStart(); tag; tag = parser.tagNesting.IterNext()) {
            if (Tag_NotFound == *tag || IsInlineTag(*tag))
                continue;
            // TODO: skip html, body and possibly more
            ++nesting;
        }
        HtmlTag tag = FindTag(t);
        HtmlAddWithNesting(&res, t, tag, nesting);

    }
    lenOut = res.Count();
    return res.StealData();
}

#ifdef DEBUG
#include <assert.h>

namespace unittests {

static void Test00(const char *s, HtmlToken::TokenType expectedType = HtmlToken::EmptyElementTag) {
    HtmlPullParser parser(s, str::Len(s));
    HtmlToken *t = parser.Next();
    assert(t->type == expectedType);
    assert(t->NameIs("p"));
    HtmlTag tag = FindTag(t);
    assert(Tag_P == tag);
    AttrInfo *a = t->GetAttrByName("a1");
    assert(a->NameIs("a1"));
    assert(a->ValIs(">"));

    a = t->GetAttrByName("foo");
    assert(a->NameIs("foo"));
    assert(a->ValIs("bar"));

    a = t->GetAttrByName("nope");
    assert(!a);

    t = parser.Next();
    assert(!t);
}

static void HtmlEntities()
{
    struct {
        const char *s; int rune;
    } entities[] = {
        { "&Uuml;", 220 },
        { "&uuml;", 252 },
        { "&times;", 215 },
        { "&AElig;", 198 },
        { "&zwnj;", 8204 },
        { "&58;", 58 },
        { "&32783;", 32783 },
        { "&#20;", 32 },
        { "&#Af34;", 44852 },
        { "&Auml;", 196 },
        { "&a3;", -1 },
        { "&#z312;", -1 },
        { "&aer;", -1 }
    };
    for (size_t i = 0; i < dimof(entities); i++ ) {
        const char *s = entities[i].s;
        int got = ResolveHtmlEntity(s + 1, str::Len(s) - 2);
        assert(got == entities[i].rune);
    }
    const char *unchanged[] = {
        "foo", "", " as;d "
    };
    for (size_t i = 0; i < dimof(unchanged); i++) {
        const char *s = unchanged[i];
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(res == s);
    }

    struct {
        const char *s; const char *res;
    } changed[] = {
        // implementation detail: if there is '&' in the string
        // we always allocate, even if it isn't a valid entity
        { "a&12", "a&12" },
        { "a&#30", "a&#30" },

        { "&32;b", " b" },
        { "&#20;ra", " ra" },
        { "&lt;", "<" },
        { "a&amp; &32;to&#20;end", "a&  to end" }
    };
    for (size_t i = 0; i < dimof(changed); i++) {
        const char *s = changed[i].s;
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(str::Eq(res, changed[i].res));
        free((void*)res);
    }
}

}

void HtmlPullParser_UnitTests()
{
    unittests::Test00("<p a1='>' foo=bar />");
    unittests::Test00("<p a1 ='>'     foo=\"bar\"/>");
    unittests::Test00("<p a1=  '>' foo=bar>", HtmlToken::StartTag);
    unittests::HtmlEntities();
}

#endif
