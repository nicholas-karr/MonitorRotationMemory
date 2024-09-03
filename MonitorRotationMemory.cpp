#include "framework.h"
#include "monitor.h"
#include "timer.h"

// Handle to the application module
HINSTANCE appHinst = NULL;

// Called when system tray icon is clicked
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

constexpr wchar_t WINDOW_CLASS[] = L"MonitorRotationMemory";

// Use a guid to uniquely identify our icon
class __declspec(uuid("f22385ff-7b02-47c4-b21c-2cb1390bab57")) APP_ICON_ID;

void                RegisterWindowClass(PCWSTR pszClassName, PCWSTR pszMenuName, WNDPROC lpfnWndProc);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void                ShowContextMenu(HWND hwnd, POINT pt);
BOOL                AddNotificationIcon(HWND hwnd);
BOOL                DeleteNotificationIcon();

// Compat to allow launching as a console mode application with a terminal (to see the logs)
int main() {
    return _tWinMain(GetModuleHandle(NULL), NULL, GetCommandLine(), SW_SHOW);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR /*lpCmdLine*/, int nCmdShow)
{
    appHinst = hInstance;
    RegisterWindowClass(WINDOW_CLASS, MAKEINTRESOURCE(IDC_MONITORROTATIONMEMORY), WndProc);

    // Create the hidden main window. It is never shown.
    WCHAR szTitle[100];
    LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));
    HWND hwnd = CreateWindow(WINDOW_CLASS, szTitle, SW_HIDE,
        CW_USEDEFAULT, 0, 250, 200, NULL, NULL, appHinst, NULL);
    nassert(hwnd);

    initMonitorTimer();

    // Execute rotation for the first time
    executeMonitorTimer(hwnd, 0, 0, 0);

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void RegisterWindowClass(PCWSTR pszClassName, PCWSTR pszMenuName, WNDPROC lpfnWndProc)
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = lpfnWndProc;
    wcex.hInstance = appHinst;
    wcex.hIcon = LoadIcon(appHinst, MAKEINTRESOURCE(IDI_MONITORROTATIONMEMORY));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = pszMenuName;
    wcex.lpszClassName = pszClassName;
    RegisterClassEx(&wcex);
}

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    // add the icon, setting the icon, tooltip, and callback message.
    // the icon will be identified with the GUID
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = __uuidof(APP_ICON_ID);
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    LoadIconMetric(appHinst, MAKEINTRESOURCE(IDI_MONITORROTATIONMEMORY), LIM_SMALL, &nid.hIcon);
    LoadString(appHinst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // NOTIFYICON_VERSION_4 is prefered
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = __uuidof(APP_ICON_ID);
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void PositionFlyout(HWND hwnd, REFGUID guidIcon)
{
    // find the position of our system tray icon
    NOTIFYICONIDENTIFIER nii = { sizeof(nii) };
    nii.guidItem = guidIcon;
    RECT rcIcon;
    HRESULT hr = Shell_NotifyIconGetRect(&nii, &rcIcon);
    if (SUCCEEDED(hr))
    {
        // display the flyout in an appropriate position close to the printer icon
        POINT const ptAnchor = { (rcIcon.left + rcIcon.right) / 2, (rcIcon.top + rcIcon.bottom) / 2 };

        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        SIZE sizeWindow = { rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top };

        if (CalculatePopupWindowPosition(&ptAnchor, &sizeWindow, TPM_VERTICAL | TPM_VCENTERALIGN | TPM_CENTERALIGN | TPM_WORKAREA, &rcIcon, &rcWindow))
        {
            // position the flyout and make it the foreground window
            SetWindowPos(hwnd, HWND_TOPMOST, rcWindow.left, rcWindow.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU contextMenu = LoadMenu(appHinst, MAKEINTRESOURCE(IDC_CONTEXTMENU));
    if (contextMenu)
    {
        HMENU hSubMenu = GetSubMenu(contextMenu, 0);
        if (hSubMenu)
        {
            // Mutate submenu pause to have correct checkmark
            MENUITEMINFO info = {};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_STATE;
            info.fState = paused ? MFS_CHECKED : MFS_UNCHECKED;

            nassert(SetMenuItemInfo(hSubMenu, IDM_PAUSE, false, &info));

            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);

            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(contextMenu);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND s_hwndFlyout = NULL;

    switch (message)
    {
    case WM_DISPLAYCHANGE:
        std::cout << "Noticed display event\n";
        setMonitorTimer(hwnd);
        break;
    case WM_CREATE:
        // add the notification icon
        if (!AddNotificationIcon(hwnd))
        {
            MessageBox(hwnd,
                L"Please read the ReadMe.txt file for troubleshooting",
                L"Error adding icon", MB_OK);
            return -1;
        }
        break;
    case WM_COMMAND:
    {
        int const wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            MessageBox(hwnd, L"By Nicholas Karr\nvcinventerman on Github\nhttps://karrmedia.com", L"MonitorRotationMemory", MB_OK);
            break;

        case IDM_PAUSE:
        {
            paused = !paused;

            if (!paused)
            {
                executeMonitorTimer(hwnd, 0, 0, 0);
            }

            break;
        }

        case IDM_CAPTURE:
            appendConfigToFile(MonitorSetup::getCurrent());
            break;

        case IDM_OPEN_CONFIG:
        {
            ShellExecute(0, 0, utf8ToUtf16(getConfigFilePath().string()).c_str(), 0, 0, SW_SHOW);
            break;
        }

        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
        }

        break;
    }

    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case NIN_SELECT:
            if (IsWindowVisible(s_hwndFlyout))
            {
                s_hwndFlyout = NULL;
            }
            break;

        case WM_CONTEXTMENU:
        {
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
            ShowContextMenu(hwnd, pt);
        }
        break;
        }
        break;

    case WM_DESTROY:
        DeleteNotificationIcon();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}