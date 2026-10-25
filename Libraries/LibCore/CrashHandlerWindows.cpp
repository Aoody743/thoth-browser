/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/CrashHandler.h>
#include <LibCore/CrashReportData.h>
#include <LibCore/System.h>

#include <AK/Format.h>
#include <AK/ScopeGuard.h>
#include <AK/Platform.h>
#include <AK/Vector.h>

#if defined(AK_OS_WINDOWS)

#include <windows.h>
#include <dbghelp.h>

#include <AK/Windows.h>
#include <csignal>
#include <cstring>
#include <string>

// NB: Windows has no signal-based crash machinery like the POSIX backend. An unhandled
//     exception filter runs in the crashing process, which is enough to:
//     1. write the report frames to the inherited crash-report fd for the UI to read, and
//     2. drop a minidump next to the process for post-mortem debugging.
//     Symbol lookup uses dbghelp; without a PDB the frames still carry module + address.

namespace Core::CrashHandler {

using CrashReportData::ReportFrame;
using CrashReportData::ReportHeader;

static int s_report_fd { -1 };

static void write_minidump(EXCEPTION_POINTERS* exception)
{
    // MiniDumpWriteDump must run on the crashing thread to capture a faithful context.
    using MiniDumpWriteDumpProc = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    auto* module = GetModuleHandleW(L"dbghelp.dll");
    if (!module) {
        module = LoadLibraryW(L"dbghelp.dll");
        if (!module)
            return;
    }
    auto* write_dump = reinterpret_cast<MiniDumpWriteDumpProc>(GetProcAddress(module, "MiniDumpWriteDump"));
    if (!write_dump)
        return;

    WCHAR dump_path[MAX_PATH] = {};
    auto length = GetTempPathW(MAX_PATH, dump_path);
    if (length == 0 || length >= MAX_PATH - 64)
        return;
    wcsncat_s(dump_path, L"ThothCrashes\\", _TRUNCATE);
    WCHAR name[64] = {};
    swprintf_s(name, L"thoth-crash-%lu.dmp", GetCurrentProcessId());
    wcsncat_s(dump_path, name, _TRUNCATE);

    auto file = CreateFileW(dump_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exception_information;
    exception_information.ThreadId = GetCurrentThreadId();
    exception_information.ExceptionPointers = exception;
    exception_information.ClientPointers = FALSE;

    (void)write_dump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &exception_information, nullptr, nullptr);
    CloseHandle(file);
}

static void write_report_record(void const* data, size_t size, size_t offset)
{
    if (s_report_fd < 0)
        return;
    OVERLAPPED overlapped = {};
    overlapped.Offset = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
    WriteFile(to_handle(s_report_fd), data, static_cast<DWORD>(size), nullptr, &overlapped);
}

static void capture_crash(EXCEPTION_POINTERS* exception)
{
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    ReportHeader header = {};
    header.magic = CrashReportData::report_magic;
    if (exception && exception->ExceptionRecord)
        header.signal = static_cast<i32>(exception->ExceptionRecord->ExceptionCode);
    header.frame_count = 0;

    void* frames[64] = {};
    auto frame_count = CaptureStackBackTrace(2, 64, frames, nullptr);

    Vector<ReportFrame> report_frames;
    report_frames.ensure_capacity(frame_count);
    SymInitializeW(GetCurrentProcess(), nullptr, TRUE);
    for (decltype(frame_count) i = 0; i < frame_count; ++i) {
        alignas(SYMBOL_INFO) BYTE buffer[sizeof(SYMBOL_INFO) + 2048] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 2047;
        DWORD64 displacement = 0;

        ReportFrame frame = {};
        frame.address = reinterpret_cast<u64>(frames[i]);
        if (SymFromAddr(GetCurrentProcess(), frame.address, &displacement, symbol)) {
            auto name_length = strlen(symbol->Name);
            frame.description_length = static_cast<u32>(min<size_t>(name_length, frame.description.size() - 1));
            memcpy(frame.description.data(), symbol->Name, frame.description_length);
        }
        report_frames.unchecked_append(move(frame));
    }
    header.frame_count = static_cast<u32>(report_frames.size());

    write_report_record(&header, sizeof(header), 0);
    for (size_t i = 0; i < report_frames.size(); ++i)
        write_report_record(&report_frames[i], sizeof(ReportFrame), sizeof(ReportHeader) + i * sizeof(ReportFrame));
    FlushFileBuffers(to_handle(s_report_fd));

    write_minidump(exception);
}

static LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* exception)
{
    capture_crash(exception);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void crt_abort_handler()
{
    // VERIFY()/assert failures raise SIGABRT through the CRT; capture them like real faults so
    // assertion messages survive into the report store.
    capture_crash(nullptr);
}

ErrorOr<void> initialize(int fd)
{
    if (fd < 0)
        return Error::from_string_literal("No crash report descriptor was provided");
    TRY(System::set_close_on_exec(fd, true));
    s_report_fd = fd;

    SetUnhandledExceptionFilter(unhandled_exception_filter);
    signal(SIGABRT, [](int) { crt_abort_handler(); });
    return {};
}

}

#endif
