/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include "BaseUtil.h"
#include "Vec.h"
#include <unzip.h>

class ZipFile {
    unzFile uf;
    Allocator *allocator;
    Vec<const TCHAR *> filenames;
    Vec<unz_file_info64> fileinfo;

public:
    ZipFile(const TCHAR *path, Allocator *allocator=NULL);
    ZipFile(IStream *stream, Allocator *allocator=NULL);
    ~ZipFile();

    size_t GetFileCount() const;
    // the result is owned by ZipFile
    const TCHAR *GetFileName(size_t fileindex);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetFileData(const TCHAR *filename, size_t *len=NULL);
    char *GetFileData(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const TCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    bool UnzipFile(const TCHAR *filename, const TCHAR *dir, const TCHAR *unzippedName=NULL);

protected:
    void ExtractFilenames();
    size_t GetFileIndex(const TCHAR *filename);
};

#endif
