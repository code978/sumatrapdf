/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef StrUtil_h
#define StrUtil_h

namespace str {

enum TrimOpt {
    TrimLeft,
    TrimRight,
    TrimBoth
};

inline size_t Len(const char *s) { return strlen(s); }
inline size_t Len(const WCHAR *s) { return wcslen(s); }

inline char *  Dup(const char *s) { return _strdup(s); }
inline WCHAR * Dup(const WCHAR *s) { return _wcsdup(s); }

void ReplacePtr(char **s, const char *snew);
void ReplacePtr(WCHAR **s, const WCHAR *snew);

char *  Join(const char *s1, const char *s2, const char *s3=NULL);
WCHAR * Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3=NULL);

bool Eq(const char *s1, const char *s2);
bool Eq(const WCHAR *s1, const WCHAR *s2);
bool EqI(const char *s1, const char *s2);
bool EqI(const WCHAR *s1, const WCHAR *s2);
bool EqIS(const TCHAR *s1, const TCHAR *s2);
bool EqN(const char *s1, const char *s2, size_t len);
bool EqN(const WCHAR *s1, const WCHAR *s2, size_t len);

template <typename T>
inline bool IsEmpty(T *s) {
    return !s || (0 == *s);
}

template <typename T>
inline bool StartsWith(const T* str, const T* txt) {
    return EqN(str, txt, Len(txt));
}

bool StartsWithI(const char *str, const char *txt);
bool StartsWithI(const WCHAR *str, const WCHAR *txt);
bool EndsWith(const char *txt, const char *end);
bool EndsWith(const WCHAR *txt, const WCHAR *end);
bool EndsWithI(const char *txt, const char *end);
bool EndsWithI(const WCHAR *txt, const WCHAR *end);

char *  DupN(const char *s, size_t lenCch);
WCHAR * DupN(const WCHAR *s, size_t lenCch);

void ToLower(char *s);
void ToLower(WCHAR *s);

char *  ToMultiByte(const WCHAR *txt, UINT CodePage);
char *  ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest);
WCHAR * ToWideChar(const char *src, UINT CodePage);
void    Utf8Encode(char *& dst, int c);

inline const char * FindChar(const char *str, const char c) {
    return strchr(str, c);
}
inline const WCHAR * FindChar(const WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}

inline const char * FindCharLast(const char *str, const char c) {
    return strrchr(str, c);
}
inline const WCHAR * FindCharLast(const WCHAR *str, const WCHAR c) {
    return wcsrchr(str, c);
}

inline const char * Find(const char *str, const char *find) {
    return strstr(str, find);
}
inline const WCHAR * Find(const WCHAR *str, const WCHAR *find) {
    return wcsstr(str, find);
}

const char * FindI(const char *s, const char *find);

int     FindStrPosI(const char *strings, const char *str, size_t len);
int     FindStrPos(const char *strings, const char *str, size_t len);

char *  FmtV(const char *fmt, va_list args);
char *  Format(const char *fmt, ...);
WCHAR * FmtV(const WCHAR *fmt, va_list args);
WCHAR * Format(const WCHAR *fmt, ...);

inline bool IsWs(char c) { return isspace((unsigned char)c); }
size_t  TrimWS(TCHAR *s, TrimOpt opt=TrimBoth);

size_t  TransChars(char *str, const char *oldChars, const char *newChars);
size_t  TransChars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars);

size_t  NormalizeWS(TCHAR *str);
size_t  RemoveChars(char *str, const char *toRemove);
size_t  RemoveChars(WCHAR *str, const WCHAR *toRemove);

size_t  BufSet(char *dst, size_t dstCchSize, const char *src);
size_t  BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src);

char *  MemToHex(const unsigned char *buf, int len);
bool    HexToMem(const char *s, unsigned char *buf, int bufLen);

TCHAR * FormatFloatWithThousandSep(double number, LCID locale=LOCALE_USER_DEFAULT);
TCHAR * FormatNumWithThousandSep(size_t num, LCID locale=LOCALE_USER_DEFAULT);
TCHAR * FormatRomanNumeral(int number);

int     CmpNatural(const TCHAR *a, const TCHAR *b);

#ifdef DEBUG
// don't call this directly, use DBG_OUT if the info is just for debug builds
// or a SimpleLog/DebugLogger if it's indeed wanted in release builds as well
// (as these are easier to redirect e.g. to the console or a file)
void    DbgOut(const TCHAR *format, ...);
#endif

const char  *   Parse(const char *str, const char *format, ...);
const char  *   Parse(const char *str, size_t len, const char *format, ...);
const WCHAR *   Parse(const WCHAR *str, const WCHAR *format, ...);

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t bufOutMax);

namespace conv {

#ifdef UNICODE
inline TCHAR *  FromWStr(const WCHAR *src) { return Dup(src); }
inline WCHAR *  ToWStr(const TCHAR *src) { return Dup(src); }
inline TCHAR *  FromCodePage(const char *src, UINT cp) { return ToWideChar(src, cp); }
inline char *   ToCodePage(const TCHAR *src, UINT cp) { return ToMultiByte(src, cp); }
#else
inline TCHAR *  FromWStr(const WCHAR *src) { return ToMultiByte(src, CP_ACP); }
inline WCHAR *  ToWStr(const TCHAR *src) { return ToWideChar(src, CP_ACP); }
inline TCHAR *  FromCodePage(const char *src, UINT cp) { return ToMultiByte(src, cp, CP_ACP); }
inline char *   ToCodePage(const TCHAR *src, UINT cp) { return ToMultiByte(src, CP_ACP, cp); }
#endif
inline TCHAR *  FromUtf8(const char *src) { return FromCodePage(src, CP_UTF8); }
inline char *   ToUtf8(const TCHAR *src) { return ToCodePage(src, CP_UTF8); }
inline TCHAR *  FromAnsi(const char *src) { return FromCodePage(src, CP_ACP); }
inline char *   ToAnsi(const TCHAR *src) { return ToCodePage(src, CP_ACP); }

size_t ToCodePageBuf(char *buf, size_t cbBufSize, const char *s, UINT cp);
size_t FromCodePageBuf(char *buf, size_t cchBufSize, const char *s, UINT cp);
size_t ToCodePageBuf(char *buf, size_t cbBufSize, const WCHAR *s, UINT cp);
size_t FromCodePageBuf(WCHAR *buf, size_t cchBufSize, const char *s, UINT cp);

}

}

// Quick conversions are no-ops for UNICODE builds
#ifdef UNICODE
#define AsWStrQ(src) ((WCHAR *)(src))
#define AsTStrQ(src) ((TCHAR *)(src))
#else
#define AsWStrQ(src) (ScopedMem<WCHAR>(str::conv::ToWStr(src)))
#define AsTStrQ(src) (ScopedMem<TCHAR>(str::conv::FromWStr(src)))
#endif

inline bool ChrIsDigit(const WCHAR c)
{
    return '0' <= c && c <= '9';
}

#define _MemToHex(ptr) str::MemToHex((const unsigned char *)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (unsigned char *)(ptr), sizeof(*ptr))

#ifndef DEBUG
  #define DBG_OUT(format, ...) NoOp()
#else
  #define DBG_OUT(format, ...) str::DbgOut(_T(format), __VA_ARGS__)
#endif

#ifdef UNICODE
  #define CF_T_TEXT CF_UNICODETEXT
#else
  #define CF_T_TEXT CF_TEXT
#endif

#endif
