#include <windows.h>
#include <string>
#include <sstream>
#include "resource.h"
#include "startup.h"

const wchar_t* kSettingsKey = L"Software\\WinKill";
const wchar_t* kStartupStateValue = L"StartupState"; // "active" or "inactive"
const wchar_t* kKeybindValue = L"Hotkey"; // New registry value

enum class StartupState { Active, Inactive };

struct HotkeySetting {
    UINT fsModifiers; // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
    UINT vk;          // Virtual-key code
};

// --- Registry helpers ---

void SaveStartupState(StartupState state) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const wchar_t* value = (state == StartupState::Active) ? L"active" : L"inactive";
        RegSetValueExW(hKey, kStartupStateValue, 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

StartupState LoadStartupState() {
    HKEY hKey;
    wchar_t value[16] = L"";
    DWORD size = sizeof(value);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, kStartupStateValue, 0, NULL, (LPBYTE)value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (wcscmp(value, L"active") == 0) return StartupState::Active;
        } else {
            RegCloseKey(hKey);
        }
    }
    return StartupState::Inactive; // Default
}

void SaveHotkeySetting(const HotkeySetting& hk) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD data[2] = { hk.fsModifiers, hk.vk };
        RegSetValueExW(hKey, kKeybindValue, 0, REG_BINARY, (const BYTE*)data, sizeof(data));
        RegCloseKey(hKey);
    }
}

HotkeySetting LoadHotkeySetting() {
    HKEY hKey;
    HotkeySetting hk = { MOD_NOREPEAT, VK_PAUSE }; // Default: Pause/Break, no modifiers
    DWORD data[2] = { 0 };
    DWORD size = sizeof(data);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, kKeybindValue, 0, NULL, (LPBYTE)data, &size) == ERROR_SUCCESS && size == sizeof(data)) {
            hk.fsModifiers = data[0];
            hk.vk = data[1];
        }
        RegCloseKey(hKey);
    }
    return hk;
}

// --- Autostart helpers (implement these elsewhere as needed) ---

bool AddToStartup(const std::wstring& appName, const std::wstring& exePath);
bool RemoveFromStartup(const std::wstring& appName);
bool IsInStartup(const std::wstring& appName);

// --- Hotkey string helper ---

std::wstring HotkeyToString(const HotkeySetting& hk) {
    std::wstringstream ss;
    if (hk.fsModifiers & MOD_CONTROL) ss << L"Ctrl+";
    if (hk.fsModifiers & MOD_ALT)     ss << L"Alt+";
    if (hk.fsModifiers & MOD_SHIFT)   ss << L"Shift+";
    if (hk.fsModifiers & MOD_WIN)     ss << L"Win+";
    UINT scan = MapVirtualKeyW(hk.vk, MAPVK_VK_TO_VSC);
    wchar_t keyName[64] = {0};
    if (GetKeyNameTextW(scan << 16, keyName, 64) && hk.vk != 0)
        ss << keyName;
    else
        ss << L"Pause";
    return ss.str();
}

// --- Dialog procedure ---

static HotkeySetting g_pendingHotkey = { MOD_NOREPEAT, VK_PAUSE };
static bool g_capturingHotkey = false;

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static std::wstring appName = L"WinKill";
    static std::wstring exePath;

    switch (message) {
    case WM_INITDIALOG: {
        // Set dialog icon
        HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        // Center the dialog on the screen
        RECT rcDlg, rcScreen;
        GetWindowRect(hDlg, &rcDlg);
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);
        int dlgWidth = rcDlg.right - rcDlg.left;
        int dlgHeight = rcDlg.bottom - rcDlg.top;
        int x = rcScreen.left + ((rcScreen.right - rcScreen.left) - dlgWidth) / 2;
        int y = rcScreen.top + ((rcScreen.bottom - rcScreen.top) - dlgHeight) / 2;
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        wchar_t path[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, path, MAX_PATH);
        exePath = path;

        // Set checkbox state based on registry
        bool autostart = IsInStartup(appName);
        CheckDlgButton(hDlg, IDC_AUTOSTART_CHECK, autostart ? BST_CHECKED : BST_UNCHECKED);

        // Set radio button state
        StartupState state = LoadStartupState();
        CheckRadioButton(hDlg, IDC_START_ACTIVE, IDC_START_INACTIVE,
                         (state == StartupState::Active) ? IDC_START_ACTIVE : IDC_START_INACTIVE);

        // Show current hotkey
        g_pendingHotkey = LoadHotkeySetting();
        SetDlgItemTextW(hDlg, IDC_KEYBIND_EDIT, HotkeyToString(g_pendingHotkey).c_str());
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_KEYBIND_SETBTN:
            g_capturingHotkey = true;
            SetDlgItemTextW(hDlg, IDC_KEYBIND_EDIT, L"Press new key...");
            SetFocus(GetDlgItem(hDlg, IDC_KEYBIND_EDIT));
            return TRUE;
        case IDC_SAVE_BUTTON: {
            BOOL checked = (IsDlgButtonChecked(hDlg, IDC_AUTOSTART_CHECK) == BST_CHECKED);
            if (checked)
                AddToStartup(appName, exePath);
            else
                RemoveFromStartup(appName);

            // Save startup state
            StartupState state = (IsDlgButtonChecked(hDlg, IDC_START_ACTIVE) == BST_CHECKED)
                                 ? StartupState::Active : StartupState::Inactive;
            SaveStartupState(state);

            // Save hotkey
            SaveHotkeySetting(g_pendingHotkey);

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_capturingHotkey) {
            UINT mod = MOD_NOREPEAT;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
            if (GetAsyncKeyState(VK_MENU) & 0x8000)    mod |= MOD_ALT;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   mod |= MOD_SHIFT;
            if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) mod |= MOD_WIN;
            UINT vk = (UINT)wParam;
            // Ignore modifier-only keys
            if (vk != VK_CONTROL && vk != VK_MENU && vk != VK_SHIFT && vk != VK_LWIN && vk != VK_RWIN) {
                g_pendingHotkey.fsModifiers = mod;
                g_pendingHotkey.vk = vk;
                SetDlgItemTextW(hDlg, IDC_KEYBIND_EDIT, HotkeyToString(g_pendingHotkey).c_str());
                g_capturingHotkey = false;
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// --- Tray icon state initialization helper ---

void SetTrayIconActive();
void SetTrayIconInactive();

void InitializeTrayIconBasedOnStartupState() {
    StartupState state = LoadStartupState();
    if (state == StartupState::Active) {
        SetTrayIconActive();   // Show icon with no red line
        // Optionally, set your app logic to "active" mode
    } else {
        SetTrayIconInactive(); // Show icon with red line
        // Optionally, set your app logic to "inactive"/standby mode
    }

    // Register the hotkey based on the saved setting
    HotkeySetting hk = LoadHotkeySetting();
    RegisterHotKey(NULL, 1, hk.fsModifiers, hk.vk); // NULL should be replaced with your main window handle

    // Unregister the hotkey when needed
    UnregisterHotKey(NULL, 1);
}
