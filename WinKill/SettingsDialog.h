#pragma once
#include <windows.h>

// Resource ID for the settings dialog (should match your .rc file)
#define IDD_SETTINGS_DIALOG 101

// Dialog procedure for the settings dialog
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
