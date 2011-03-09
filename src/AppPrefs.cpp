/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */

#include "BaseUtil.h"
#include "BencUtil.h"
#include "TStrUtil.h"
#include "FileUtil.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"

extern bool CurrLangNameSet(const char* langName);

static bool ParseDisplayMode(const char *txt, DisplayMode *resOut)
{
    assert(txt);
    if (!txt) return false;
    return DisplayModeEnumFromName(txt, resOut);
}

#define GLOBAL_PREFS_STR "gp"

#define DICT_NEW(boname) \
    benc_dict* boname = benc_dict_new(); \
    if (!boname) \
        goto Error;

#define DICT_ADD_INT64(doname,name,val) \
    ok = benc_dict_insert_int64(doname,name,(int64_t)val); \
    if (!ok) \
        goto Error;

#define DICT_ADD_STR(doname,name,val) \
        if (val) { \
            ok = benc_dict_insert_str(doname,name,val); \
            if (!ok) \
                goto Error; \
        }

#define DICT_ADD_TSTR(doname,name,val) \
        if (val) { \
            ok = benc_dict_insert_tstr(doname,name,val); \
            if (!ok) \
                goto Error; \
        }

#define DICT_ADD_BENC_OBJ(doname,name,val) \
        ok = benc_dict_insert2(doname,name,(benc_obj*)val); \
        if (!ok) { \
            benc_obj_delete((benc_obj*)val); \
            goto Error; \
        }

static benc_dict* Prefs_SerializeGlobal(SerializableGlobalPrefs *globalPrefs)
{
    BOOL       ok;
    const char * txt;

    DICT_NEW(prefs);
    DICT_ADD_INT64(prefs, SHOW_TOOLBAR_STR, globalPrefs->m_showToolbar);
    DICT_ADD_INT64(prefs, SHOW_TOC_STR, globalPrefs->m_showToc);
    DICT_ADD_INT64(prefs, TOC_DX_STR, globalPrefs->m_tocDx);
    DICT_ADD_INT64(prefs, PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs->m_pdfAssociateDontAskAgain);
    DICT_ADD_INT64(prefs, PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs->m_pdfAssociateShouldAssociate);

    DICT_ADD_INT64(prefs, BG_COLOR_STR, globalPrefs->m_bgColor);
    DICT_ADD_INT64(prefs, ESC_TO_EXIT_STR, globalPrefs->m_escToExit);
    DICT_ADD_INT64(prefs, ENABLE_AUTO_UPDATE_STR, globalPrefs->m_enableAutoUpdate);
    DICT_ADD_INT64(prefs, REMEMBER_OPENED_FILES_STR, globalPrefs->m_rememberOpenedFiles);
    DICT_ADD_INT64(prefs, GLOBAL_PREFS_ONLY_STR, globalPrefs->m_globalPrefsOnly);

    txt = DisplayModeNameFromEnum(globalPrefs->m_defaultDisplayMode);
    DICT_ADD_STR(prefs, DISPLAY_MODE_STR, txt);

    txt = str_printf("%.4f", globalPrefs->m_defaultZoom);
    if (txt) {
        DICT_ADD_STR(prefs, ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }
    DICT_ADD_INT64(prefs, WINDOW_STATE_STR, globalPrefs->m_windowState);
    DICT_ADD_INT64(prefs, WINDOW_X_STR, globalPrefs->m_windowPosX);
    DICT_ADD_INT64(prefs, WINDOW_Y_STR, globalPrefs->m_windowPosY);
    DICT_ADD_INT64(prefs, WINDOW_DX_STR, globalPrefs->m_windowDx);
    DICT_ADD_INT64(prefs, WINDOW_DY_STR, globalPrefs->m_windowDy);

    DICT_ADD_TSTR(prefs, INVERSE_SEARCH_COMMANDLINE, globalPrefs->m_inverseSearchCmdLine);
    DICT_ADD_INT64(prefs, ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs->m_enableTeXEnhancements);
    DICT_ADD_TSTR(prefs, VERSION_TO_SKIP_STR, globalPrefs->m_versionToSkip);
    DICT_ADD_STR(prefs, LAST_UPDATE_STR, globalPrefs->m_lastUpdateTime);
    DICT_ADD_STR(prefs, UI_LANGUAGE_STR, globalPrefs->m_currentLanguage);
    DICT_ADD_INT64(prefs, FWDSEARCH_OFFSET, globalPrefs->m_fwdsearchOffset);
    DICT_ADD_INT64(prefs, FWDSEARCH_COLOR, globalPrefs->m_fwdsearchColor);
    DICT_ADD_INT64(prefs, FWDSEARCH_WIDTH, globalPrefs->m_fwdsearchWidth);
    DICT_ADD_INT64(prefs, FWDSEARCH_PERMANENT, globalPrefs->m_fwdsearchPermanent);
    return prefs;
Error:
    benc_obj_delete((benc_obj*)prefs);
    return NULL;
}

static bool DisplayState_Deserialize(benc_dict* dict, DisplayState *ds, bool globalPrefsOnly)
{
    const char *filePath = dict_get_str(dict, FILE_STR);
    if (filePath)
        ds->filePath = utf8_to_tstr(filePath);
    const char *decryptionKey = dict_get_str(dict, DECRYPTION_KEY_STR);
    if (decryptionKey)
        ds->decryptionKey = StrCopy(decryptionKey);
    if (globalPrefsOnly) {
        ds->useGlobalValues = TRUE;
        return true;
    }

    const char* txt = dict_get_str(dict, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &ds->displayMode);
    dict_get_int(dict, PAGE_NO_STR, &ds->pageNo);
    dict_get_int(dict, ROTATION_STR, &ds->rotation);
    dict_get_int(dict, SCROLL_X_STR, &ds->scrollX);
    dict_get_int(dict, SCROLL_Y_STR, &ds->scrollY);
    dict_get_int(dict, WINDOW_STATE_STR, &ds->windowState);
    dict_get_int(dict, WINDOW_X_STR, &ds->windowX);
    dict_get_int(dict, WINDOW_Y_STR, &ds->windowY);
    dict_get_int(dict, WINDOW_DX_STR, &ds->windowDx);
    dict_get_int(dict, WINDOW_DY_STR, &ds->windowDy);
    dict_get_bool(dict, SHOW_TOC_STR, &ds->showToc);
    dict_get_int(dict, TOC_DX_STR, &ds->tocDx);
    dict_get_float_from_str(dict, ZOOM_VIRTUAL_STR, &ds->zoomVirtual);
    dict_get_bool(dict, USE_GLOBAL_VALUES_STR, &ds->useGlobalValues);

    benc_array *tocState = benc_obj_as_array(benc_dict_find2(dict, TOC_STATE_STR));
    if (tocState) {
        size_t len = benc_array_len(tocState);
        ds->tocState = SAZA(int, len + 1);
        if (ds->tocState) {
            ds->tocState[0] = (int)len;
            for (size_t i = 0; i < len; i++)
                benc_array_get_int(tocState, i, &ds->tocState[i + 1]);
        }
    }

    return true;
}

static benc_dict* DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    BOOL  ok;
    const char * txt;
    
    DICT_NEW(prefs);
    DICT_ADD_TSTR(prefs, FILE_STR, ds->filePath);
    if (ds->decryptionKey)
        DICT_ADD_STR(prefs, DECRYPTION_KEY_STR, ds->decryptionKey);

    if (globalPrefsOnly || ds->useGlobalValues) {
        DICT_ADD_INT64(prefs, USE_GLOBAL_VALUES_STR, TRUE);
        return prefs;
    }

    txt = DisplayModeNameFromEnum(ds->displayMode);
    if (txt)
        DICT_ADD_STR(prefs, DISPLAY_MODE_STR, txt);
    DICT_ADD_INT64(prefs, PAGE_NO_STR, ds->pageNo);
    DICT_ADD_INT64(prefs, ROTATION_STR, ds->rotation);
    DICT_ADD_INT64(prefs, SCROLL_X_STR, ds->scrollX);

    DICT_ADD_INT64(prefs, SCROLL_Y_STR, ds->scrollY);
    DICT_ADD_INT64(prefs, WINDOW_STATE_STR, ds->windowState);
    DICT_ADD_INT64(prefs, WINDOW_X_STR, ds->windowX);
    DICT_ADD_INT64(prefs, WINDOW_Y_STR, ds->windowY);
    DICT_ADD_INT64(prefs, WINDOW_DX_STR, ds->windowDx);
    DICT_ADD_INT64(prefs, WINDOW_DY_STR, ds->windowDy);

    DICT_ADD_INT64(prefs, SHOW_TOC_STR, ds->showToc);
    DICT_ADD_INT64(prefs, TOC_DX_STR, ds->tocDx);

    txt = str_printf("%.4f", ds->zoomVirtual);
    if (txt) {
        DICT_ADD_STR(prefs, ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }

    if (ds->tocState && ds->tocState[0] > 0) {
        benc_array *tocState = benc_array_new();
        if (tocState) {
            for (int i = 1; i <= ds->tocState[0]; i++)
                benc_array_append(tocState, (benc_obj *)benc_int64_new(ds->tocState[i]));
            DICT_ADD_BENC_OBJ(prefs, TOC_STATE_STR, tocState);
        }
    }

    return prefs;
Error:
    benc_obj_delete((benc_obj*)prefs);
    return NULL;
}

static benc_array *FileHistoryList_Serialize(FileHistoryList *root, bool globalPrefsOnly)
{
    BOOL ok;
    assert(root);
    if (!root) return NULL;

    benc_array* arr = benc_array_new();
    if (!arr)
        goto Error;

    // Don't save more file entries than will be useful
    int restCount = globalPrefsOnly ? MAX_RECENT_FILES_IN_MENU : INT_MAX;
    FileHistoryNode *curr = root->first;
    while (curr && restCount > 0) {
        benc_dict* bobj = DisplayState_Serialize(&curr->state, globalPrefsOnly);
        if (!bobj)
            goto Error;
        ok = benc_array_append(arr, (benc_obj *)bobj);
        if (!ok)
            goto Error;
        curr = curr->next;
        restCount--;
    }
    return arr;
Error:
    if (arr)
        benc_array_delete(arr);
    return NULL;      
}

static const char *Prefs_Serialize(SerializableGlobalPrefs *globalPrefs, FileHistoryList *root, size_t* lenOut)
{
    BOOL        ok;
    char *      data = NULL;

    DICT_NEW(prefs);

    benc_dict* global = Prefs_SerializeGlobal(globalPrefs);
    if (!global)
        goto Error;
    DICT_ADD_BENC_OBJ(prefs, GLOBAL_PREFS_STR, global);
    benc_array *fileHistory = FileHistoryList_Serialize(root, globalPrefs->m_globalPrefsOnly);
    if (!fileHistory)
        goto Error;
    DICT_ADD_BENC_OBJ(prefs, FILE_HISTORY_STR, fileHistory);

    data = benc_obj_to_data((benc_obj*)prefs, lenOut);
Error:
    benc_obj_delete((benc_obj*)prefs);
    return (const char*)data;
}

static void dict_get_str_helper(benc_dict *d, const char *key, char **val)
{
    const char *txt = dict_get_str(d, key);
    if (txt) {
        char *tmp = StrCopy(txt);
        if (tmp) {
            free(*val);
            *val = tmp;
        }
    }
}

static void dict_get_tstr_helper(benc_dict *d, const char *key, TCHAR **val)
{
    const char *txt = dict_get_str(d, key);
    if (txt) {
        TCHAR *tmp = utf8_to_tstr(txt);
        if (tmp) {
            free(*val);
            *val = tmp;
        }
    }
}

static bool Prefs_Deserialize(const char *prefsTxt, size_t prefsTxtLen, SerializableGlobalPrefs *globalPrefs, FileHistoryList *fileHistoryRoot)
{
    benc_obj * bobj;
    benc_str * bstr;
    bobj = benc_obj_from_data(prefsTxt, prefsTxtLen);
    if (!bobj)
        return false;
    benc_dict* prefs = benc_obj_as_dict(bobj);
    if (!prefs)
        goto Error;

    benc_dict* global = benc_obj_as_dict(benc_dict_find2(prefs, GLOBAL_PREFS_STR));
    if (!global)
        goto Error;

    dict_get_bool(global, SHOW_TOOLBAR_STR, &globalPrefs->m_showToolbar);
    dict_get_bool(global, SHOW_TOC_STR, &globalPrefs->m_showToc);
    dict_get_int(global, TOC_DX_STR, &globalPrefs->m_tocDx);
    dict_get_bool(global, PDF_ASSOCIATE_DONT_ASK_STR, &globalPrefs->m_pdfAssociateDontAskAgain);
    dict_get_bool(global, PDF_ASSOCIATE_ASSOCIATE_STR, &globalPrefs->m_pdfAssociateShouldAssociate);
    dict_get_bool(global, ESC_TO_EXIT_STR, &globalPrefs->m_escToExit);
    dict_get_int(global, BG_COLOR_STR, &globalPrefs->m_bgColor);
    dict_get_bool(global, ENABLE_AUTO_UPDATE_STR, &globalPrefs->m_enableAutoUpdate);
    dict_get_bool(global, REMEMBER_OPENED_FILES_STR, &globalPrefs->m_rememberOpenedFiles);
    dict_get_bool(global, GLOBAL_PREFS_ONLY_STR, &globalPrefs->m_globalPrefsOnly);

    const char* txt = dict_get_str(global, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &globalPrefs->m_defaultDisplayMode);
    dict_get_float_from_str(global, ZOOM_VIRTUAL_STR, &globalPrefs->m_defaultZoom);
    dict_get_int(global, WINDOW_STATE_STR, &globalPrefs->m_windowState);

    dict_get_int(global, WINDOW_X_STR, &globalPrefs->m_windowPosX);
    dict_get_int(global, WINDOW_Y_STR, &globalPrefs->m_windowPosY);
    dict_get_int(global, WINDOW_DX_STR, &globalPrefs->m_windowDx);
    dict_get_int(global, WINDOW_DY_STR, &globalPrefs->m_windowDy);

    dict_get_tstr_helper(global, INVERSE_SEARCH_COMMANDLINE, &globalPrefs->m_inverseSearchCmdLine);
    dict_get_int(global, ENABLE_TEX_ENHANCEMENTS_STR, &globalPrefs->m_enableTeXEnhancements);
    dict_get_tstr_helper(global, VERSION_TO_SKIP_STR, &globalPrefs->m_versionToSkip);
    dict_get_str_helper(global, LAST_UPDATE_STR, &globalPrefs->m_lastUpdateTime);

    bstr = benc_obj_as_str(benc_dict_find2(global, UI_LANGUAGE_STR));
    CurrLangNameSet(bstr->m_str);

    dict_get_int(global, FWDSEARCH_OFFSET, &globalPrefs->m_fwdsearchOffset);
    dict_get_int(global, FWDSEARCH_COLOR, &globalPrefs->m_fwdsearchColor);
    dict_get_int(global, FWDSEARCH_WIDTH, &globalPrefs->m_fwdsearchWidth);
    dict_get_int(global, FWDSEARCH_PERMANENT, &globalPrefs->m_fwdsearchPermanent);

    benc_array* fileHistory = benc_obj_as_array(benc_dict_find2(prefs, FILE_HISTORY_STR));
    if (!fileHistory)
        goto Error;
    size_t dlen = benc_array_len(fileHistory);
    for (size_t i = 0; i < dlen; i++) {
        benc_dict *dict = benc_obj_as_dict(benc_array_get(fileHistory, i));
        assert(dict);
        if (!dict) continue;
        FileHistoryNode *node = new FileHistoryNode();
        DisplayState_Deserialize(dict, &node->state, globalPrefs->m_globalPrefsOnly);
        if (node->state.filePath)
            fileHistoryRoot->Append(node);
        else
            delete node;
    }
    benc_obj_delete(bobj);
    return true;
Error:
    benc_obj_delete(bobj);
    return false;
}

namespace Prefs {

/* Load preferences from the preferences file.
   Returns true if preferences file was loaded, false if there was an error.
*/
bool Load(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistoryList *fileHistoryRoot)
{
    bool            ok = false;

#ifdef DEBUG
    static bool     loaded = false;
    assert(!loaded);
    loaded = true;
#endif

    assert(filepath);
    if (!filepath)
        return false;

    size_t prefsFileLen;
    ScopedMem<char> prefsTxt(file_read_all(filepath, &prefsFileLen));
    if (!str_empty(prefsTxt)) {
        ok = Prefs_Deserialize(prefsTxt, prefsFileLen, globalPrefs, fileHistoryRoot);
        assert(ok);
    }

    // TODO: add a check if a file exists, to filter out deleted files
    // but only if a file is on a non-network drive (because
    // accessing network drives can be slow and unnecessarily spin
    // the drives).
#if 0
    FileHistoryNode *node = fileHistoryRoot->first;
    while (node) {
        FileHistoryNode *next = node->next;
        if (!file_exists(node->state.filePath)) {
            DBG_OUT_T("Prefs_Load() file '%s' doesn't exist anymore\n", node->state.filePath);
            fileHistoryRoot->Remove(node);
            delete node;
        }
        node = next;
    }
#endif

    return ok;
}

bool Save(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistoryList *fileHistoryRoot)
{
    assert(filepath);
    if (!filepath)
        return false;

    size_t dataLen;
    ScopedMem<const char> data(Prefs_Serialize(globalPrefs, fileHistoryRoot, &dataLen));
    if (!data)
        return false;

    assert(dataLen > 0);
    /* TODO: consider 2-step process:
        * write to a temp file
        * rename temp file to final file */
    return write_to_file(filepath, (void*)data.Get(), dataLen);
}

}
