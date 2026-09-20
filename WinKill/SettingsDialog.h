#pragma once

#include <windows.h>
#include <string>
#include "resource.h"

enum class StartupState {
    Active,
    Inactive
};

struct HotkeySetting {
    UINT fsModifiers;
    UINT vk;
};

// Startup state settings
StartupState LoadStartupState();
void SaveStartupState(StartupState state);

// Hotkey settings
HotkeySetting LoadHotkeySetting();
void SaveHotkeySetting(const HotkeySetting& setting);
std::wstring HotkeyToString(const HotkeySetting& setting);

// Caps Lock setting
bool LoadCapsLockSetting();
void SaveCapsLockSetting(bool blocked);

// Dialog procedure for the settings dialog
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
