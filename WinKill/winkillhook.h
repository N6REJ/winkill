#pragma once

#include <windows.h>

bool winkill_install_hook(HWND);
bool winkill_remove_hook();
void winkill_set_capslock_blocked(bool blocked);
bool winkill_is_capslock_blocked();
