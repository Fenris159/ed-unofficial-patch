// fakechain -- a stand-in for EDHM or ReShade, for testing the chain.
//
// Pretends to be another d3d11 proxy: exports D3D11CreateDevice, forwards it to
// the system d3d11.dll, and records that it was called. It deliberately exports
// ONLY that one function, so the fallback path gets tested too -- a real proxy
// wraps a handful of entry points and leaves the rest to the system copy.
//
// It also does work in DllMain, which is the whole point. That is what made the
// first chaining attempt crash: loading this from inside edvr's DllMain would
// run this DllMain under the loader lock. Loading it after DllMain returns must
// not crash, and this file is how that gets checked without needing EDHM.
#include <windows.h>

#include <cstdio>

namespace {

HMODULE g_system = nullptr;

typedef HRESULT(WINAPI* PFN_Create)(void*, UINT, HMODULE, UINT, const void*, UINT, UINT,
                                    void**, UINT*, void**);
typedef HRESULT(WINAPI* PFN_CreateSwap)(void*, UINT, HMODULE, UINT, const void*, UINT, UINT,
                                        const void*, void**, void**, UINT*, void**);

void marker(const char* what) {
    char path[MAX_PATH]{};
    const DWORD envLen = GetEnvironmentVariableA("EDVR_FAKECHAIN_MARKER", path, MAX_PATH);
    if (envLen == 0 || envLen >= MAX_PATH) {
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) *(slash + 1) = 0;
        strcat_s(path, MAX_PATH, "fakechain_was_here.txt");
    }

    HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD n = 0;
    WriteFile(f, what, static_cast<DWORD>(strlen(what)), &n, nullptr);
    WriteFile(f, "\r\n", 2, &n, nullptr);
    CloseHandle(f);
}

void maybeCallbackDuringAttach() {
    char callName[64]{};
    const DWORD callLen = GetEnvironmentVariableA("EDVR_FAKECHAIN_ATTACH_CALL", callName, sizeof(callName));
    if (callLen == 0 || callLen >= sizeof(callName)) return;

    HMODULE proxy = nullptr;
    char proxyPath[MAX_PATH]{};
    const DWORD pathLen = GetEnvironmentVariableA("EDVR_FAKECHAIN_PROXY_PATH", proxyPath, MAX_PATH);
    if (pathLen > 0 && pathLen < MAX_PATH) {
        proxy = GetModuleHandleA(proxyPath);
    }
    if (!proxy) proxy = GetModuleHandleW(L"d3d11.dll");
    if (!proxy) return;

    // Pass D3D_DRIVER_TYPE_UNKNOWN (0) with null adapter so once ED-UP breaks the
    // InitOnce startup deadlock and routes to System32 (or returns E_FAIL), the
    // callback returns immediately without creating a device under loader lock (#255).
    if (strcmp(callName, "D3D11CreateDevice") == 0) {
        auto fn = reinterpret_cast<PFN_Create>(GetProcAddress(proxy, "D3D11CreateDevice"));
        if (fn) {
            (void)fn(nullptr, 0, nullptr, 0, nullptr, 0, 7, nullptr, nullptr, nullptr);
            marker("attach callback D3D11CreateDevice returned");
        }
    } else if (strcmp(callName, "D3D11CreateDeviceAndSwapChain") == 0) {
        auto fn = reinterpret_cast<PFN_CreateSwap>(
            GetProcAddress(proxy, "D3D11CreateDeviceAndSwapChain"));
        if (fn) {
            (void)fn(nullptr, 0, nullptr, 0, nullptr, 0, 7, nullptr, nullptr, nullptr, nullptr,
                     nullptr);
            marker("attach callback D3D11CreateDeviceAndSwapChain returned");
        }
    }
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI D3D11CreateDevice(
    void* adapter, UINT driverType, HMODULE software, UINT flags, const void* levels,
    UINT numLevels, UINT sdk, void** device, UINT* got, void** ctx) {
    marker("D3D11CreateDevice went through fakechain");
    if (!g_system) {
        // Resolve "the original d3d11.dll" BY NAME, which is what a real proxy
        // does and what 3Dmigoto does. This is the whole point of the test: the
        // module already loaded under that name is edvr, so this hands back
        // edvr's own export and the two call each other until the stack ends.
        //
        // A proxy that resolved the system path explicitly would not do this.
        // 3Dmigoto does not, hence EDHM crashing when chained.
        g_system = GetModuleHandleW(L"d3d11.dll");
        if (!g_system) {
            wchar_t sys[MAX_PATH]{};
            GetSystemDirectoryW(sys, MAX_PATH);
            wcscat_s(sys, MAX_PATH, L"\\d3d11.dll");
            g_system = LoadLibraryW(sys);
        }
    }
    if (!g_system) return E_FAIL;
    auto real = reinterpret_cast<PFN_Create>(GetProcAddress(g_system, "D3D11CreateDevice"));
    if (!real) return E_FAIL;
    return real(adapter, driverType, software, flags, levels, numLevels, sdk, device, got,
                ctx);
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Deliberately non-trivial, the way a real mod's entry point is. Under
        // the loader lock this is the sort of thing that goes wrong.
        marker("fakechain DllMain ran");
        maybeCallbackDuringAttach();
    }
    return TRUE;
}
