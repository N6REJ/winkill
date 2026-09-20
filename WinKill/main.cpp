#include <Windows.h>
#include <Shellapi.h>
#include <tchar.h>
#include "winkillhook.h"
#include <string>
#include <vector>
#include "Resource.h"
#include "startup.h"
#include "SettingsDialog.h"
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Version.lib")

#define WM_MYTRAYICON (WM_USER + 2000)
#define MENU_ITEM_VERSION 1980
#define MENU_ITEM_TOGGLE 1983
#define MENU_ITEM_STARTUP 1984
#define MENU_ITEM_SETTINGS 1985
#define MENU_ITEM_EXIT 1979

#define MENU_ITEM_TOGGLE_CAPTION L"Toggle"
#define MENU_ITEM_STARTUP_CAPTION L"Startup on Boot"
#define MENU_ITEM_SETTINGS_CAPTION L"Settings..."
#define MENU_ITEM_EXIT_CAPTION L"Exit"
#define WINDOW_CLASS L"WinKillClass"

static HICON iconActive = nullptr, iconKilled = nullptr;
static bool hooked = false, trayIconDataVisible = false;
static HMENU trayMenu = 0;
static NOTIFYICONDATA trayIconData = { };
static HWND mainWindow = NULL;
static HINSTANCE instance = NULL;

static void showTrayIcon();
static void setTrayIcon(HICON icon, bool isHooked);
static void hideTrayIcon();
static void createTrayMenu();
static void updateStartupMenuCheckmark();
static void updateToggleMenuState();
static void reloadHotkey();
static void startHook();
static void stopHook();
static void toggleHook();
static void createWindow(HINSTANCE instance);
static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static std::wstring GetAppVersionString() {
    wchar_t path[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, path, MAX_PATH)) {
        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSize(path, &handle);
        if (size > 0) {
            std::vector<BYTE> buffer(size);
            if (GetFileVersionInfo(path, handle, size, buffer.data())) {
                LPWSTR prodVer = nullptr;
                UINT prodVerLen = 0;
                if (VerQueryValue(buffer.data(), L"\\StringFileInfo\\040904b0\\ProductVersion", (LPVOID*)&prodVer, &prodVerLen) && prodVer && prodVerLen > 0) {
                    wchar_t ver[64];
                    swprintf_s(ver, L"WinKill v%s", prodVer);
                    return ver;
                }

                VS_FIXEDFILEINFO* fileInfo = nullptr;
                UINT len = 0;
                if (VerQueryValue(buffer.data(), L"\\", (LPVOID*)&fileInfo, &len) && len >= sizeof(VS_FIXEDFILEINFO)) {
                    UINT major = HIWORD(fileInfo->dwFileVersionMS);
                    UINT minor = LOWORD(fileInfo->dwFileVersionMS);
                    UINT patch = HIWORD(fileInfo->dwFileVersionLS);
                    UINT build = LOWORD(fileInfo->dwFileVersionLS);
                    wchar_t ver[64];
                    if (build > 0) {
                        swprintf_s(ver, L"WinKill v%u.%u.%u-%u", major, minor, patch, build);
                    } else {
                        swprintf_s(ver, L"WinKill v%u.%u.%u", major, minor, patch);
                    }
                    return ver;
                }
            }
        }
    }
    return L"WinKill v2025.8.1";
}

int CALLBACK wWinMain(
    _In_ HINSTANCE inst,
    _In_opt_ HINSTANCE prev,
    _In_ LPWSTR args,
    _In_ int showType
) {
    HANDLE mutex = CreateMutex(NULL, TRUE, L"WinKillSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    createWindow(inst);

    winkill_set_capslock_blocked(LoadCapsLockSetting());

    StartupState state = LoadStartupState();
    if (args && (wcsstr(args, L"/startDisabled") != nullptr || wcsstr(args, L"-startDisabled") != nullptr)) {
        state = StartupState::Inactive;
    }

    if (state == StartupState::Active) {
        startHook();
    } else {
        stopHook();
    }

    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return 0;
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_MYTRAYICON: {
            switch (LOWORD(lParam)) {
                case WM_LBUTTONDOWN: {
                    toggleHook();
                    break;
                }

                case WM_RBUTTONUP:
                case WM_CONTEXTMENU: {
                    POINT cursor = { 0 };
                    ::GetCursorPos(&cursor);
                    ::SetForegroundWindow(mainWindow);
                    updateStartupMenuCheckmark();
                    updateToggleMenuState();
                    TrackPopupMenuEx(trayMenu, 0, cursor.x, cursor.y, hwnd, nullptr);
                    PostMessage(mainWindow, WM_NULL, 0, 0);
                    break;
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == 1) {
                toggleHook();
                return 0;
            }
            break;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_QUERYENDSESSION: {
            return TRUE;
        }

        case WM_ENDSESSION: {
            if (wParam) {
                UnregisterHotKey(hwnd, 1);
                stopHook();
                hideTrayIcon();
                PostQuitMessage(0);
            }
            return 0;
        }

        case WM_DESTROY: {
            UnregisterHotKey(hwnd, 1);
            stopHook();
            hideTrayIcon();
            PostQuitMessage(0);
            return 0;
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == 0) {
                switch (LOWORD(wParam)) {
                case MENU_ITEM_EXIT:
                    PostQuitMessage(0);
                    return 1;

                case MENU_ITEM_TOGGLE:
                    toggleHook();
                    return 1;

                case MENU_ITEM_STARTUP: {
                    wchar_t exePath[MAX_PATH];
                    GetModuleFileName(nullptr, exePath, MAX_PATH);
                    std::wstring appName = L"WinKill";
                    if (IsInStartup(appName)) {
                        RemoveFromStartup(appName);
                    } else {
                        AddToStartup(appName, exePath);
                    }
                    updateStartupMenuCheckmark();
                    return 1;
                }
                case MENU_ITEM_SETTINGS:
                    if (DialogBox(instance, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), mainWindow, SettingsDialogProc) == IDOK) {
                        reloadHotkey();
                        winkill_set_capslock_blocked(LoadCapsLockSetting());
                        updateStartupMenuCheckmark();
                    }
                    return 1;
                }
            }
            break;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void reloadHotkey() {
    if (mainWindow) {
        UnregisterHotKey(mainWindow, 1);
        HotkeySetting hk = LoadHotkeySetting();
        if (hk.vk != 0) {
            UINT fsModifiers = hk.fsModifiers;
            if (hk.vk == VK_PAUSE) {
                fsModifiers &= ~MOD_NOREPEAT;
            }
            if (!RegisterHotKey(mainWindow, 1, fsModifiers, hk.vk)) {
                RegisterHotKey(mainWindow, 1, fsModifiers & ~MOD_NOREPEAT, hk.vk);
            }
        }
    }
}

static void updateStartupMenuCheckmark() {
    if (trayMenu) {
        bool inStartup = IsInStartup(L"WinKill");
        CheckMenuItem(trayMenu, MENU_ITEM_STARTUP, inStartup ? MF_CHECKED : MF_UNCHECKED);
    }
}

static void createWindow(HINSTANCE inst) {
    instance = inst;
    iconActive = LoadIcon(instance, MAKEINTRESOURCE(IDR_ACTIVEICON)); // Active.ico (no red line, active/on)
    iconKilled = LoadIcon(instance, MAKEINTRESOURCE(IDR_MAINFRAME));  // Standby.ico (red line, standby/off)

    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"WinKillClass";
    RegisterClass(&wc);

    std::wstring verStr = GetAppVersionString();

    mainWindow =
        CreateWindowEx(
            WS_EX_TOOLWINDOW,
            WINDOW_CLASS,
            verStr.c_str(),
            0,
            0, 0, 0, 0, /* dimens */
            nullptr,
            nullptr,
            instance,
            nullptr);

    SetWindowLong(mainWindow, GWL_STYLE, 0); /* removes title, borders. */

    SetWindowPos(
        mainWindow,
        nullptr,
        -32000, -32000, 50, 50,
        SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER);

    reloadHotkey();
    createTrayMenu();
    showTrayIcon();
}

static void showTrayIcon() {
    if (trayIconDataVisible) {
        return;
    }

    std::wstring tip = hooked ? L"WinKill - Active" : L"WinKill - Disabled";

    SecureZeroMemory(&trayIconData, sizeof(trayIconData));
    trayIconData.cbSize = sizeof(trayIconData);
    trayIconData.hWnd = mainWindow;
    trayIconData.uID = 0;
    trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayIconData.uCallbackMessage = WM_MYTRAYICON;
    trayIconData.hIcon = hooked ? iconActive : iconKilled;
    ::wcsncpy_s(trayIconData.szTip, ARRAYSIZE(trayIconData.szTip), tip.c_str(), _TRUNCATE);

    trayIconDataVisible = (Shell_NotifyIcon(NIM_ADD, &trayIconData) != 0);

    if (trayIconDataVisible){
        trayIconData.uVersion = NOTIFYICON_VERSION;
        Shell_NotifyIcon(NIM_SETVERSION, &trayIconData);
    }
}

static void hideTrayIcon() {
    if (!trayIconDataVisible) {
        return;
    }

    trayIconDataVisible = !(Shell_NotifyIcon(NIM_DELETE, &trayIconData) != 0);
}

static void setTrayIcon(HICON icon, bool isHooked) {
    if (trayIconDataVisible) {
        trayIconData.hIcon = icon;
        trayIconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        std::wstring tip = isHooked ? L"WinKill - Active" : L"WinKill - Disabled";
        ::wcsncpy_s(trayIconData.szTip, ARRAYSIZE(trayIconData.szTip), tip.c_str(), _TRUNCATE);
        Shell_NotifyIcon(NIM_MODIFY, &trayIconData);
    }
}

static void updateToggleMenuState() {
    if (trayMenu) {
        CheckMenuItem(trayMenu, MENU_ITEM_TOGGLE, hooked ? MF_CHECKED : MF_UNCHECKED);
    }
}

static void createTrayMenu() {
    if (trayMenu) {
        DestroyMenu(trayMenu);
    }
    trayMenu = CreatePopupMenu();

    std::wstring verStr = GetAppVersionString();
    AppendMenu(trayMenu, MF_STRING | MF_GRAYED, MENU_ITEM_VERSION, verStr.c_str());
    AppendMenu(trayMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(trayMenu, MF_STRING, MENU_ITEM_TOGGLE, MENU_ITEM_TOGGLE_CAPTION);
    AppendMenu(trayMenu, MF_STRING, MENU_ITEM_STARTUP, MENU_ITEM_STARTUP_CAPTION);
    AppendMenu(trayMenu, MF_STRING, MENU_ITEM_SETTINGS, MENU_ITEM_SETTINGS_CAPTION);
    AppendMenu(trayMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(trayMenu, MF_STRING, MENU_ITEM_EXIT, MENU_ITEM_EXIT_CAPTION);

    updateStartupMenuCheckmark();
    updateToggleMenuState();
    showTrayIcon();
}

static void startHook() {
    winkill_set_capslock_blocked(LoadCapsLockSetting());
    hooked = winkill_install_hook(mainWindow);

    if (hooked) {
        setTrayIcon(iconActive, true);
        updateToggleMenuState();
    }
    else {
        MessageBox(mainWindow, L"Couldn't start keyboard hook!", L"WinKill", MB_OK);
    }
}

static void stopHook() {
    hooked = (!winkill_remove_hook());

    if (!hooked) {
        setTrayIcon(iconKilled, false);
        updateToggleMenuState();
    }
}

void toggleHook() {
    hooked ? stopHook() : startHook();
}
