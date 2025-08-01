#include "startup.h"
#include <Windows.h>

bool AddToStartup(const std::wstring& appName, const std::wstring& exePath) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                               0, KEY_SET_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegSetValueExW(hKey, appName.c_str(), 0, REG_SZ, 
                           (const BYTE*)exePath.c_str(), 
                           (DWORD)((exePath.length() + 1) * sizeof(wchar_t)));
    
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

bool RemoveFromStartup(const std::wstring& appName) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                               0, KEY_SET_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegDeleteValueW(hKey, appName.c_str());
    RegCloseKey(hKey);
    
    // Return true if the value was deleted or if it didn't exist
    return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}

bool IsInStartup(const std::wstring& appName) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
                               L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                               0, KEY_QUERY_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    wchar_t value[MAX_PATH];
    DWORD valueSize = sizeof(value);
    result = RegQueryValueExW(hKey, appName.c_str(), 0, NULL, 
                             (LPBYTE)value, &valueSize);
    
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

void SetTrayIconActive() {
    // This function is implemented in main.cpp as part of the tray icon management
    // No additional implementation needed here
}

void SetTrayIconInactive() {
    // This function is implemented in main.cpp as part of the tray icon management
    // No additional implementation needed here
}
