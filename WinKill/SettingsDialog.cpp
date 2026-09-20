#include "SettingsDialog.h"
#include <windows.h>
#include <string>
#include <sstream>
#include "resource.h"
#include "startup.h"
#include "winkillhook.h"

const wchar_t* kSettingsKey = L"Software\\WinKill";
const wchar_t* kStartupStateValue = L"StartupState"; // "active" or "inactive"
const wchar_t* kKeybindValue = L"Hotkey";
const wchar_t* kBlockCapsLockValue = L"BlockCapsLock";

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
            if (_wcsicmp(value, L"active") == 0) return StartupState::Active;
            return StartupState::Inactive;
        }
        RegCloseKey(hKey);
    }
    return StartupState::Active; // Default: Active
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

bool LoadCapsLockSetting() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, kBlockCapsLockValue, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return false; // Default: disabled
}

void SaveCapsLockSetting(bool blocked) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = blocked ? 1 : 0;
        RegSetValueExW(hKey, kBlockCapsLockValue, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

// --- Hotkey string helper ---

std::wstring HotkeyToString(const HotkeySetting& hk) {
    std::wstringstream ss;
    if (hk.fsModifiers & MOD_CONTROL) ss << L"Ctrl+";
    if (hk.fsModifiers & MOD_ALT)     ss << L"Alt+";
    if (hk.fsModifiers & MOD_SHIFT)   ss << L"Shift+";
    if (hk.fsModifiers & MOD_WIN)     ss << L"Win+";

    switch (hk.vk) {
    case VK_PAUSE:   ss << L"Pause"; return ss.str();
    case VK_CANCEL:  ss << L"Break"; return ss.str();
    case VK_ESCAPE:  ss << L"Esc"; return ss.str();
    case VK_SPACE:   ss << L"Space"; return ss.str();
    case VK_RETURN:  ss << L"Enter"; return ss.str();
    case VK_TAB:     ss << L"Tab"; return ss.str();
    case VK_BACK:    ss << L"Backspace"; return ss.str();
    case VK_CAPITAL: ss << L"Caps Lock"; return ss.str();
    case VK_PRIOR:   ss << L"Page Up"; return ss.str();
    case VK_NEXT:    ss << L"Page Down"; return ss.str();
    case VK_END:     ss << L"End"; return ss.str();
    case VK_HOME:    ss << L"Home"; return ss.str();
    case VK_LEFT:    ss << L"Left"; return ss.str();
    case VK_UP:      ss << L"Up"; return ss.str();
    case VK_RIGHT:   ss << L"Right"; return ss.str();
    case VK_DOWN:    ss << L"Down"; return ss.str();
    case VK_INSERT:  ss << L"Insert"; return ss.str();
    case VK_DELETE:  ss << L"Delete"; return ss.str();
    case VK_SNAPSHOT:ss << L"Print Screen"; return ss.str();
    case VK_SCROLL:  ss << L"Scroll Lock"; return ss.str();
    case VK_NUMLOCK: ss << L"Num Lock"; return ss.str();
    }

    if (hk.vk >= VK_F1 && hk.vk <= VK_F24) {
        ss << L"F" << (hk.vk - VK_F1 + 1);
        return ss.str();
    }

    if (hk.vk >= VK_NUMPAD0 && hk.vk <= VK_NUMPAD9) {
        ss << L"Num " << (hk.vk - VK_NUMPAD0);
        return ss.str();
    }

    if ((hk.vk >= '0' && hk.vk <= '9') || (hk.vk >= 'A' && hk.vk <= 'Z')) {
        ss << (wchar_t)hk.vk;
        return ss.str();
    }

    UINT scan = MapVirtualKeyW(hk.vk, MAPVK_VK_TO_VSC);
    if (scan != 0) {
        wchar_t keyName[64] = {0};
        LONG lParam = (scan << 16);
        if (hk.vk == VK_INSERT || hk.vk == VK_DELETE || hk.vk == VK_HOME ||
            hk.vk == VK_END || hk.vk == VK_PRIOR || hk.vk == VK_NEXT ||
            hk.vk == VK_LEFT || hk.vk == VK_UP || hk.vk == VK_RIGHT || hk.vk == VK_DOWN ||
            hk.vk == VK_DIVIDE || hk.vk == VK_NUMLOCK) {
            lParam |= (1 << 24);
        }
        if (GetKeyNameTextW(lParam, keyName, 64) > 0) {
            ss << keyName;
            return ss.str();
        }
    }

    if (hk.vk != 0) {
        ss << L"Key 0x" << std::hex << hk.vk;
    } else {
        ss << L"None";
    }
    return ss.str();
}

// --- Dialog procedure ---

static HotkeySetting g_pendingHotkey = { MOD_NOREPEAT, VK_PAUSE };
static bool g_capturingHotkey = false;
static WNDPROC g_oldEditProc = NULL;
static HWND g_hCurrentDlg = NULL;

static void ProcessCapturedKey(HWND hDlg, WPARAM wParam) {
    UINT mod = MOD_NOREPEAT;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)    mod |= MOD_ALT;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   mod |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) mod |= MOD_WIN;
    UINT vk = (UINT)wParam;
    // Ignore modifier-only keys
    if (vk != VK_CONTROL && vk != VK_MENU && vk != VK_SHIFT && vk != VK_LWIN && vk != VK_RWIN &&
        vk != VK_LCONTROL && vk != VK_RCONTROL && vk != VK_LMENU && vk != VK_RMENU &&
        vk != VK_LSHIFT && vk != VK_RSHIFT) {
        g_pendingHotkey.fsModifiers = mod;
        g_pendingHotkey.vk = vk;
        SetDlgItemTextW(hDlg, IDC_KEYBIND_EDIT, HotkeyToString(g_pendingHotkey).c_str());
        g_capturingHotkey = false;
    }
}

static LRESULT CALLBACK HotkeyEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_capturingHotkey) {
        if (uMsg == WM_GETDLGCODE) {
            return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
        }
        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
            ProcessCapturedKey(g_hCurrentDlg, wParam);
            return 0;
        }
        if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP || uMsg == WM_CHAR) {
            return 0;
        }
    }
    return CallWindowProc(g_oldEditProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static std::wstring appName = L"WinKill";
    static std::wstring exePath;

    switch (message) {
    case WM_INITDIALOG: {
        g_hCurrentDlg = hDlg;

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
        g_capturingHotkey = false;
        SetDlgItemTextW(hDlg, IDC_KEYBIND_EDIT, HotkeyToString(g_pendingHotkey).c_str());

        // Subclass hotkey edit control
        HWND hEdit = GetDlgItem(hDlg, IDC_KEYBIND_EDIT);
        if (hEdit) {
            g_oldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)HotkeyEditProc);
        }

        // Set Caps Lock checkbox state
        bool blockCaps = LoadCapsLockSetting();
        CheckDlgButton(hDlg, IDC_CAPSLOCK_CHECK, blockCaps ? BST_CHECKED : BST_UNCHECKED);

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
            g_capturingHotkey = false;
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

            // Save Caps Lock setting
            bool blockCaps = (IsDlgButtonChecked(hDlg, IDC_CAPSLOCK_CHECK) == BST_CHECKED);
            SaveCapsLockSetting(blockCaps);
            winkill_set_capslock_blocked(blockCaps);

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            g_capturingHotkey = false;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_capturingHotkey) {
            ProcessCapturedKey(hDlg, wParam);
            return TRUE;
        }
        break;
    case WM_DESTROY: {
        g_capturingHotkey = false;
        HWND hEdit = GetDlgItem(hDlg, IDC_KEYBIND_EDIT);
        if (hEdit && g_oldEditProc) {
            SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)g_oldEditProc);
            g_oldEditProc = NULL;
        }
        break;
    }
    }
    return FALSE;
}
