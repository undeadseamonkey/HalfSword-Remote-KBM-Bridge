#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>
#include <cstdint>
#include <cstdio>

#ifndef PROBE_DLL_NAME
#define PROBE_DLL_NAME L"HalfSwordBridge19.dll"
#endif

static DWORD find_half_sword() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{sizeof(entry)};
    DWORD found = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"HalfSwordUE5-Win64-Shipping.exe") != 0) continue;
            if (found) { found = 0; break; } // Refuse an ambiguous target.
            found = entry.th32ProcessID;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

static std::uintptr_t remote_module(DWORD pid, const wchar_t *name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry{sizeof(entry)};
    std::uintptr_t base = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, name) == 0) {
                base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return base;
}

int load_bridge_into_half_sword() {
    DWORD pid = find_half_sword();
    if (!pid) {
        std::fwprintf(stderr, L"Start one Half Sword game process first.\n");
        return 1;
    }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_LIMITED_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        std::fwprintf(stderr, L"Cannot open Half Sword (error %lu).\n", GetLastError());
        return 2;
    }
    wchar_t process_path[MAX_PATH]{};
    DWORD path_length = MAX_PATH;
    if (!QueryFullProcessImageNameW(process, 0, process_path, &path_length) ||
        wcsstr(process_path, L"\\Half Sword\\HalfswordUE5\\Binaries\\Win64\\HalfSwordUE5-Win64-Shipping.exe") == nullptr) {
        std::fwprintf(stderr, L"Target path does not match the installed Half Sword game.\n");
        CloseHandle(process);
        return 3;
    }
    wchar_t dll_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, dll_path, MAX_PATH)) {
        CloseHandle(process);
        return 4;
    }
    wchar_t *slash = wcsrchr(dll_path, L'\\');
    if (!slash || (slash - dll_path) + 1 + wcslen(PROBE_DLL_NAME) >= MAX_PATH) {
        CloseHandle(process);
        return 4;
    }
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - dll_path), PROBE_DLL_NAME);
    const SIZE_T bytes = (wcslen(dll_path) + 1) * sizeof(wchar_t);
    void *remote_path = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path || !WriteProcessMemory(process, remote_path, dll_path, bytes, nullptr)) {
        std::fwprintf(stderr, L"Could not stage probe path in game process (error %lu).\n", GetLastError());
        if (remote_path) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 5;
    }
    const std::uintptr_t local_load = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    HMODULE owner = nullptr;
    if (local_load) GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(local_load), &owner);
    wchar_t owner_path[MAX_PATH]{};
    if (owner) GetModuleFileNameW(owner, owner_path, MAX_PATH);
    const wchar_t *owner_name = wcsrchr(owner_path, L'\\');
    const std::uintptr_t remote_base = owner_name ? remote_module(pid, owner_name + 1) : 0;
    const std::uintptr_t local_base = reinterpret_cast<std::uintptr_t>(owner);
    auto load = local_base && local_load && remote_base
        ? reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_base + (local_load - local_base))
        : nullptr;
    HANDLE thread = load ? CreateRemoteThread(process, nullptr, 0, load, remote_path, 0, nullptr) : nullptr;
    if (!thread) {
        std::fwprintf(stderr, L"Could not load probe (error %lu).\n", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 6;
    }
    DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD result = 0;
    if (wait == WAIT_OBJECT_0) GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    if (wait == WAIT_OBJECT_0) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);
    if (wait != WAIT_OBJECT_0 || result == 0) {
        std::fwprintf(stderr, L"Probe load was not confirmed; check that the game is still running.\n");
        return 7;
    }
    return 0;
}




