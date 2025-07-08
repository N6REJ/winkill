#include <Windows.h>
#include <Shellapi.h>
#include <tchar.h>
#include "winkillhook.h"
#include <string>
#include "Resource.h"
#include "startup.h"
#include "SettingsDialog.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#define WM_MYTRAYICON WM_USER + 2000
#define MENU_ITEM_TOGGLE 1983
#define MENU_ITEM_STARTUP 1984
#define MENU_ITEM_SETTINGS 1985
#define MENU_ITEM_EXIT 1979
#define MENU_ITEM_TOGGLE_CAPTION L"Toggle"
#define MENU_ITEM_STARTUP_CAPTION L"Startup on Boot"
#define MENU_ITEM_SETTINGS_CAPTION L"Settings..."
#define MENU_ITEM_EXIT_CAPTION L"Exit"
#define TRAY_ICON_TIP L"WinKill v2025.6.11"
#define WINDOW_CLASS L"WinKillClass"

static HICON iconActive = nullptr, iconKilled = nullptr;
static bool hooked = false, trayIconDataVisible = false;
static HMENU trayMenu = 0;
static NOTIFYICONDATA trayIconData = { };
static HWND mainWindow;
static HINSTANCE instance;

static void showTrayIcon();
static void setTrayIcon(HICON icon);
static void hideTrayIcon();
static void createTrayMenu();
static void startHook();
static void stopHook();
static void toggleHook();
static void createWindow(HINSTANCE instance);
static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int CALLBACK wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE prev,
    _In_ LPWSTR args,
    _In_ int showType
) {
    createWindow(instance);

    stopHook(); // Always start disabled

    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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

                case WM_RBUTTONUP: {
                        POINT cursor = { 0 };
                        ::GetCursorPos(&cursor);
                        ::SetForegroundWindow(mainWindow);
                        TrackPopupMenuEx(trayMenu, 0, cursor.x, cursor.y, hwnd, nullptr);
                        PostMessage(mainWindow, WM_NULL, 0, 0);
                    }
                    break;
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == 1) { // id=1 for Pause/Break
                toggleHook();
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            UnregisterHotKey(hwnd, 1); // Unregister Pause/Break hotkey
            stopHook();
            hideTrayIcon();
            break;
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
                        MessageBox(mainWindow, L"WinKill will no longer start with Windows.", L"Startup", MB_OK);
                    } else {
                        AddToStartup(appName, exePath);
                        MessageBox(mainWindow, L"WinKill will now start with Windows.", L"Startup", MB_OK);
                    }
                    return 1;
                }
                case MENU_ITEM_SETTINGS:
                    DialogBox(instance, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), mainWindow, SettingsDialogProc);
                    return 1;
                }
            }
            break;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}


static void createWindow(HINSTANCE inst) {
    instance = inst;
    iconActive = LoadIcon(instance, MAKEINTRESOURCE(IDR_ACTIVEICON)); // Killed.ico (red line, active/on)
    iconKilled = LoadIcon(instance, MAKEINTRESOURCE(IDR_MAINFRAME)); // Active.ico (no red line, standby/off)

    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"WinKillClass";
    RegisterClass(&wc);

    mainWindow =
        CreateWindowEx(
            WS_EX_TOOLWINDOW,
            WINDOW_CLASS,
            TRAY_ICON_TIP,
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
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    // Register Pause/Break key as a global hotkey (id=1)
    if (!RegisterHotKey(mainWindow, 1, 0, VK_PAUSE)) {
        MessageBox(mainWindow, L"Failed to register Pause/Break as a global hotkey. It may already be registered by another application.", L"Hotkey Registration Failed", MB_OK | MB_ICONERROR);
    }

    createTrayMenu();
    showTrayIcon();
}

static void showTrayIcon() {
    if (trayIconDataVisible) {
        return;
    }

    SecureZeroMemory(&trayIconData, sizeof(trayIconData));
    trayIconData.cbSize = sizeof(trayIconData);
    trayIconData.hWnd = mainWindow;
    trayIconData.uID = 0;
    trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayIconData.uCallbackMessage = WM_MYTRAYICON;
    trayIconData.hIcon = hooked ? iconActive : iconKilled;
    ::wcscpy_s(trayIconData.szTip, 255, TRAY_ICON_TIP);

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

static void setTrayIcon(HICON icon) {
    if (trayIconDataVisible) {
        trayIconData.hIcon = icon;
        Shell_NotifyIcon(NIM_MODIFY, &trayIconData);
    }
}

static void createTrayMenu() {
    trayMenu = CreatePopupMenu();

    MENUITEMINFO menuItem = { 0 };
    menuItem.cbSize = sizeof(MENUITEMINFO);
    menuItem.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;

    AppendMenu(trayMenu, 0, MENU_ITEM_TOGGLE, MENU_ITEM_TOGGLE_CAPTION);
    AppendMenu(trayMenu, 0, MENU_ITEM_STARTUP, MENU_ITEM_STARTUP_CAPTION);
    AppendMenu(trayMenu, 0, MENU_ITEM_SETTINGS, MENU_ITEM_SETTINGS_CAPTION);
    AppendMenu(trayMenu, MF_SEPARATOR, MENU_ITEM_TOGGLE, L"-");
    AppendMenu(trayMenu, 0, MENU_ITEM_EXIT, MENU_ITEM_EXIT_CAPTION);

    showTrayIcon();
}

static void startHook() {
    hooked = winkill_install_hook(mainWindow);

    if (hooked) {
        setTrayIcon(iconActive);
    }
    else {
        MessageBox(mainWindow, L"Couldn't start keyboard hook!", L"WinKill", MB_OK);
    }
}

static void stopHook() {
    hooked = (!winkill_remove_hook());

    if (!hooked) {
        setTrayIcon(iconKilled);
    }
}

void toggleHook() {
    hooked ? stopHook() : startHook();
}
