import os, re

g_src_dir = os.path.join(os.path.split(__file__)[0], "..", "src")

TRANSLATIONS_TXT_C = """\
/* Generated by scripts\\update_translations.py
   DO NOT EDIT MANUALLY */

#ifndef MAKELANGID
#include <windows.h>
#endif

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

#define LANGS_COUNT   %(langs_count)d
#define STRINGS_COUNT %(translations_count)d

typedef struct {
    const char *code;
    const char *fullName;
    LANGID id;
    BOOL isRTL;
} LangDef;

#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)

LangDef gLangData[LANGS_COUNT] = {
    %(lang_data)s
};

#undef _LANGID

const char *gTranslations[LANGS_COUNT * STRINGS_COUNT] = {
%(translations)s
};
"""

# use octal escapes because hexadecimal ones can consist of
# up to four characters, e.g. \xABc isn't the same as \253c
def c_oct(c):
    o = "00" + oct(ord(c))
    return "\\" + o[-3:]

def c_escape(txt, encode_to_utf=False):
    if txt is None:
        return "NULL"
    # the old, pre-apptranslator translation system required encoding to utf8
    if encode_to_utf:
        txt = txt.encode("utf-8")
    # escape all quotes
    txt = txt.replace('"', r'\"')
    # and all non-7-bit characters of the UTF-8 encoded string
    txt = re.sub(r"[\x80-\xFF]", lambda m: c_oct(m.group(0)[0]), txt)
    return '"%s"' % txt

def get_trans_for_lang(strings_dict, keys, lang_arg):
    trans = []
    for k in keys:
        txt = None
        for (lang, tr) in strings_dict[k]:
            if lang_arg == lang:
                # don't include a translation, if it's the same as the default
                if tr != k:
                    txt = tr
                break
        trans.append(txt)
    return trans

def lang_sort_func(x,y):
    # special case: default language is first
    if x[0] == "en": return -1
    if y[0] == "en": return 1
    return cmp(x[1], y[1])

def make_lang_id(lang):
    ids = lang[2]
    if ids is None:
        return "-1"
    ids = [el.replace(" ", "_") for el in ids]
    if len(ids) == 2:
        id = "MAKELANGID(LANG_%s, SUBLANG_%s_%s)" % (ids[0], ids[0], ids[1])
    else:
        assert len(ids) == 1
        id = "_LANGID(LANG_%s)" % (ids[0])
    return id.upper()

def is_rtl_lang(lang):
    return len(lang) > 3 and lang[3] == "RTL"

# correctly sorts strings containing escaped tabulators
def key_sort_func(a, b):
    return cmp(a.replace(r"\t", "\t"), b.replace(r"\t", "\t"))

def gen_c_code(langs_idx, strings_dict, file_name, encode_to_utf=False):
    langs_idx = sorted(langs_idx, cmp=lang_sort_func)
    assert "en" == langs_idx[0][0]
    langs_count = len(langs_idx)
    translations_count = len(strings_dict)

    keys = strings_dict.keys()
    keys.sort(cmp=key_sort_func)
    lines = []
    for lang in langs_idx:
        if "en" == lang[0]:
            trans = keys
        else:
            trans = get_trans_for_lang(strings_dict, keys, lang[0])
        lines.append("")
        lines.append("  /* Translations for language %s */" % lang[0])
        lines += ["  %s," % c_escape(t, encode_to_utf) for t in trans]
    translations = "\n".join(lines)
    #print [l[1] for l in langs_idx]
    lang_data = ['{ "%s", %s, %s, %d },' % (lang[0], c_escape(lang[1]), make_lang_id(lang), 1 if is_rtl_lang(lang) else 0) for lang in langs_idx]
    lang_data = "\n    ".join(lang_data)

    file_content = TRANSLATIONS_TXT_C % locals()
    file(file_name, "wb").write(file_content)

def main():
    import apptransdl
    changed = apptransdl.downloadAndUpdateTranslationsIfChanged()
    if changed:
        print("\nNew translations received from the server, checkin Translations_txt.cpp and translations.txt")

if __name__ == "__main__":
    main()
