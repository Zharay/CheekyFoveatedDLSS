#pragma once
#include <Windows.h>
#include <cstdio>

namespace cheeky::logging {
// DLL-local statics cannot share a live toggle. This process-scoped, page-file
// mapping costs no filesystem I/O and keeps the hot-path check in memory.
inline volatile LONG* errors_only_flag() noexcept {
    static volatile LONG* flag = []() noexcept -> volatile LONG* {
        const auto error = GetLastError();
        wchar_t name[80]{};
        swprintf_s(name, L"Local\\CheekyLoggingV1-%lu", GetCurrentProcessId());
        const auto mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
            PAGE_READWRITE, 0, sizeof(LONG), name);
        auto* view = mapping ? static_cast<volatile LONG*>(
            MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG))) : nullptr;
        if (mapping && !view) CloseHandle(mapping);
        if (!view) OutputDebugStringA("Cheeky: logging policy unavailable; keeping diagnostics enabled.\n");
        SetLastError(error);
        // Keep both handle and view until exit so later DLLs can open the name.
        return view;
    }();
    return flag;
}
inline bool enabled() noexcept {
    const auto flag = errors_only_flag();
    return !flag || InterlockedCompareExchange(flag, 0, 0) == 0;
}
inline void set_enabled(bool value) noexcept {
    if (const auto flag = errors_only_flag()) InterlockedExchange(flag, value ? 0 : 1);
}
inline bool should_log(int level) noexcept {
    return level <= 2 || enabled();
}
}
