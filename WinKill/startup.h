#pragma once
#include <string>

bool AddToStartup(const std::wstring& appName, const std::wstring& exePath);
bool RemoveFromStartup(const std::wstring& appName);
bool IsInStartup(const std::wstring& appName);
void SetTrayIconActive();
void SetTrayIconInactive();
