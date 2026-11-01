// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dbghelp.h>
#endif
namespace fga {
#if JUCE_WINDOWS
using CrashDumpWriter=BOOL(WINAPI*)(HANDLE,DWORD,HANDLE,MINIDUMP_TYPE,PMINIDUMP_EXCEPTION_INFORMATION,PMINIDUMP_USER_STREAM_INFORMATION,PMINIDUMP_CALLBACK_INFORMATION);
inline CrashDumpWriter crashDumpWriter=nullptr;
#endif
inline void prepareCrashDiagnostics(){
   #if JUCE_WINDOWS
    // Resolve this while healthy, rather than loading a DLL from a faulting thread.
    const auto library=LoadLibraryExW(L"dbghelp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    crashDumpWriter=library?reinterpret_cast<CrashDumpWriter>(GetProcAddress(library,"MiniDumpWriteDump")):nullptr;
   #endif
}
inline void writeCrashDiagnostic(void* context,const juce::File& file){
    file.getParentDirectory().createDirectory();
    juce::String report="Fase Groove Arranger ";report+=JUCE_APPLICATION_VERSION_STRING;
    report+=" crash\n"+juce::Time::getCurrentTime().toString(true,true,true,true)+"\n";
   #if JUCE_WINDOWS
    auto* info=static_cast<EXCEPTION_POINTERS*>(context);
    report+="Thread: "+juce::String((juce::int64)GetCurrentThreadId())+"\n";
    if(info&&info->ExceptionRecord){
        const auto* record=info->ExceptionRecord;
        report+="Exception: 0x"+juce::String::toHexString((juce::int64)record->ExceptionCode)+"\nAddress: 0x"+juce::String::toHexString((juce::int64)(uintptr_t)record->ExceptionAddress)+"\n";
        HMODULE module=nullptr;
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(record->ExceptionAddress),&module)){
            wchar_t name[32768]{};GetModuleFileNameW(module,name,(DWORD)std::size(name));
            report+="Module: "+juce::String(name)+"\nModule offset: 0x"+juce::String::toHexString((juce::int64)((uintptr_t)record->ExceptionAddress-(uintptr_t)module))+"\n";
        }
    }
    // Persist metadata first: it remains useful even if writing a dump fails.
    file.replaceWithText(report);
    const auto dump=file.withFileExtension(".dmp");
    const auto writer=crashDumpWriter;
    const auto handle=writer?CreateFileW(dump.getFullPathName().toWideCharPointer(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr):INVALID_HANDLE_VALUE;
    bool written=false;
    if(writer&&handle!=INVALID_HANDLE_VALUE){MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(),info,FALSE};written=writer(GetCurrentProcess(),GetCurrentProcessId(),handle,(MINIDUMP_TYPE)(MiniDumpNormal|MiniDumpWithThreadInfo),info?&exception:nullptr,nullptr,nullptr)!=FALSE;}
    const auto error=GetLastError();if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
    report+=written?"Minidump: "+dump.getFileName()+"\n":"Minidump unavailable; Windows error "+juce::String((juce::int64)error)+"\n";
   #else
    juce::ignoreUnused(context);
   #endif
    report+="\n"+juce::SystemStats::getStackBacktrace();file.replaceWithText(report);
}
}
