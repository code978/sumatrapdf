/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#ifndef CrashHandler_h
#define CrashHandler_h

void InstallCrashHandler(const TCHAR *crashDumpPath);
void SubmitCrashInfo();
void UninstallCrashHandler();

#endif
