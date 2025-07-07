#include "winkillhook.h"
#include <winuser.h>

static HHOOK hook = NULL;
static HWND hwnd = NULL;
static HINSTANCE instance = NULL;

LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        switch (wParam) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
                DWORD keyCode = ((PKBDLLHOOKSTRUCT) lParam)->vkCode;
                if ((keyCode == VK_LWIN) || (keyCode == VK_RWIN)) {
                    return 1;
                }
            }
            break;
        }
    }
    return ::CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool winkill_install_hook(HWND owner) {
    if ((!hook) && owner) {
        hwnd = owner;
        instance = (HINSTANCE)GetModuleHandle(NULL);
        hook = ::SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, instance, 0);
    }
    return (hook != NULL);
}

bool winkill_remove_hook() {
    if ((hook) && (::UnhookWindowsHookEx(hook))) {
        hook = NULL;
        hwnd = NULL;
    }
    return (hook == NULL);
}
