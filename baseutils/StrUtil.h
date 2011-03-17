/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef StrUtil_h
#define StrUtil_h

void    win32_dbg_out(const char *format, ...);
void    win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen);

#ifdef DEBUG
  #define DBG_OUT win32_dbg_out
  #define DBG_OUT_HEX win32_dbg_out_hex
#else
  #define DBG_OUT(...) NoOp()
  #define DBG_OUT_HEX(...) NoOp()
#endif

#define str_find_char strchr

/* Note: this demonstrates how eventually I would like to get rid of
   tstr_* and wstr_* functions and instead rely on C++'s ability
   to use overloaded functions and only have Str* functions */

namespace Str {

static inline size_t Len(const char *s)
{
    return strlen(s);
}

static inline size_t Len(const WCHAR *s)
{
    return wcslen(s);
}

static inline char *Dup(const char *s)
{
    return _strdup(s);
}

static inline WCHAR *Dup(const WCHAR *s)
{
    return _wcsdup(s);
}

static inline bool Eq(const char *str1, const char *str2)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == strcmp(str1, str2))
        return true;
    return false;
}

static inline bool Eq(const WCHAR *str1, const WCHAR *str2)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == wcscmp(str1, str2))
        return true;
    return false;
}

static inline bool EqI(const char *str1, const char *str2)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == _stricmp(str1, str2))
        return true;
    return false;
}

static inline bool EqI(const WCHAR *str1, const WCHAR *str2)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == _wcsicmp(str1, str2))
        return true;
    return false;
}

static bool EqN(const char *str1, const char *str2, size_t len)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == strncmp(str1, str2, len))
        return true;
    return false;
}

static bool EqN(const WCHAR *str1, const WCHAR *str2, size_t len)
{
    if (str1 == str2)
        return true;
    if (!str1 || !str2)
        return false;
    if (0 == wcsncmp(str1, str2, len))
        return true;
    return false;
}

/* return true if 'str' starts with 'txt', case-sensitive */
static bool StartsWith(const char *str, const char *txt)
{
    return Str::EqN(str, txt, Str::Len(txt));
}

static bool StartsWith(const WCHAR *str, const WCHAR *txt)
{
    return Str::EqN(str, txt, Str::Len(txt));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
static bool StartsWithI(const char *str, const char *txt)
{
    if (str == txt)
        return true;
    if (!str || !txt)
        return false;

    if (0 == _strnicmp(str, txt, Str::Len(txt)))
        return true;
    return false;
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
static bool StartsWithI(const WCHAR *str, const WCHAR *txt)
{
    if (!str && !txt)
        return true;
    if (!str || !txt)
        return false;

    if (0 == _wcsnicmp(str, txt, Str::Len(txt)))
        return true;
    return false;
}

}

static inline bool ChrIsDigit(const WCHAR c)
{
    return '0' <= c && c <= '9';
}

// TODO: make these return bool instead of int
int     str_endswith(const char *str, const char *end);
int     str_endswithi(const char *str, const char *end);
int     str_empty(const char *str);
int     str_copy(char *dst, size_t dst_cch_size, const char *src);
int     str_copyn(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_dupn(const char *str, size_t len);
char *  str_cat_s(char *dst, size_t dst_cch_size, const char *src);
char *  str_catn_s(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_cat(const char *str1, const char *str2);
char *  str_printf(const char *format, ...);
int     str_printf_s(char *out, size_t out_cch_size, const char *format, ...);

char *  mem_to_hexstr(const unsigned char *buf, int len);
bool    hexstr_to_mem(const char *s, unsigned char *buf, int bufLen);
#define _mem_to_hexstr(ptr) mem_to_hexstr((const unsigned char *)ptr, sizeof(*ptr))
#define _hexstr_to_mem(str, ptr) hexstr_to_mem(str, (unsigned char *)ptr, sizeof(*ptr))

char *  str_to_multibyte(const char *src, UINT CodePage);
char *  multibyte_to_str(const char *src, UINT CodePage);
#define str_to_utf8(src) str_to_multibyte((src), CP_UTF8)
#define utf8_to_str(src) multibyte_to_str((src), CP_UTF8)

#ifdef DEBUG
void StrUtil_test(void);
#endif

#endif
