#define NULL 0 // Hoist this here because string.h defines it as void* but windows.h needs it to be 0
#include <string.h>
#include <windows.h>

#include "connection.h"
#include "statusbar.h"
#include "utils.h"
#include "wingpt.h"

#define STATUS_BAR_HEIGHT 24
#define MAX_API_KEY_LENGTH 64
#define API_KEY_KEY "APIKey"
#define INI_FILE_NAME "wingpt.ini"

BOOL FAR PASCAL _export AboutDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
BOOL FAR PASCAL _export OptionsDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
long FAR PASCAL _export WndProc (HWND, UINT, UINT, LONG);
LRESULT CALLBACK ComposeEditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static char szAppName[] = "WinGPT";
static WNDPROC defaultEditWndProc;
static HWND hwndSendBtn;
static char apiKey[MAX_API_KEY_LENGTH];

#pragma off(unreferenced)
int PASCAL WinMain (HANDLE hInstance, HANDLE hPrevInstance,
                    LPSTR lpszCmdLine, int nCmdShow) {
#pragma on(unreferenced)

  HWND hwnd;
  MSG msg;
  WNDCLASS wndclass;

  register_status_bar(hInstance, hPrevInstance);

  if (!hPrevInstance) {
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, "appicon");
    wndclass.hCursor = LoadCursor (NULL, IDC_ARROW);
    wndclass.hbrBackground = GetStockObject (LTGRAY_BRUSH);
    wndclass.lpszMenuName = szAppName;
    wndclass.lpszClassName = szAppName;

    RegisterClass(&wndclass);
  }

  hwnd = CreateWindow(
    szAppName,
    "WinGPT", // Window caption
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    NULL,
    NULL,
    hInstance,
    NULL
  ) ;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}

long FAR PASCAL _export WndProc (HWND hwnd, UINT message, UINT wParam, LONG lParam) {
  static FARPROC lpfnAboutDlgProc;
  static FARPROC lpfnOptionsDlgProc;
  static HANDLE hInstance;
  static HWND hwndChatLogEdit;
  static HWND hwndComposeEdit;
  static HWND hwndStatusBar;
  static HWND hwndSpareStatusBar;
  static int cxChar, cyChar;
  HDC hdc;
  TEXTMETRIC tm;
  static int iBtnHeight;
  char lpComposeText[255];
  LOGFONT logFont;
  HFONT hSansLargeFont;
  HFONT hAnsiBoldFont;

  switch (message) {
    case WM_CREATE:
      hInstance = ((LPCREATESTRUCT)lParam)->hInstance;
      lpfnAboutDlgProc = MakeProcInstance((FARPROC)AboutDlgProc, hInstance);
      lpfnOptionsDlgProc = MakeProcInstance((FARPROC)OptionsDlgProc, hInstance);

      hdc = GetDC (hwnd) ;
      SelectObject(hdc, GetStockObject(ANSI_VAR_FONT)) ;
      GetTextMetrics(hdc, &tm) ;
      cxChar = tm.tmAveCharWidth ;
      cyChar = tm.tmHeight + tm.tmExternalLeading ;
      ReleaseDC (hwnd, hdc);

      // Create status bar
      hwndStatusBar = CreateWindow(
        "STATUSBAR",
        "Ready",
        WS_CHILD | WS_VISIBLE | WS_GROUP,
        0, 0, 0, 0,
        hwnd,
        NULL,
        ((LPCREATESTRUCT) lParam) -> hInstance,
        NULL
      );
      memset(&logFont, 0, sizeof(LOGFONT));
      logFont.lfPitchAndFamily = VARIABLE_PITCH | FF_DONTCARE;
      logFont.lfHeight = 16;
      hSansLargeFont = CreateFontIndirect(&logFont);
      SendMessage(hwndStatusBar, WM_SETFONT, (WPARAM)hSansLargeFont, 0L);

      // Create spare status bar
      hwndSpareStatusBar = CreateWindow(
        "STATUSBAR",
        "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hwnd,
        NULL,
        ((LPCREATESTRUCT) lParam) -> hInstance,
        NULL
      );

      // Create send button
      iBtnHeight = 2 * cyChar;
      hwndSendBtn = CreateWindow(
        "button",
        "&Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0,
        hwnd,
        NULL,
        ((LPCREATESTRUCT) lParam) -> hInstance,
        NULL
      );
      logFont.lfHeight = 13;
      logFont.lfWeight = 600;
      hAnsiBoldFont = CreateFontIndirect(&logFont);
      SendMessage(hwndSendBtn, WM_SETFONT, (WPARAM)hAnsiBoldFont, 0L);

      // Create chat log edit control
      hwndChatLogEdit = CreateWindow(
        "edit",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT
          | ES_MULTILINE | ES_AUTOVSCROLL,
        0, 0, 0, 0,
        hwnd,
        1,
        ((LPCREATESTRUCT) lParam) -> hInstance,
        NULL
      );
      SetWindowText(
        hwndChatLogEdit,
        "Welcome to WinGPT, your AI assistant for Windows 3.1.\r\n\r\n"
      );
      SendMessage(hwndChatLogEdit, WM_SETFONT, (WPARAM)hSansLargeFont, 0L);
      SendMessage(hwndChatLogEdit, EM_SETREADONLY, TRUE, 0L);

      // Create compose edit control
      hwndComposeEdit = CreateWindow(
        "edit",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        0, 0, 0, 0,
        hwnd,
        1,
        ((LPCREATESTRUCT) lParam) -> hInstance,
        NULL
      );
      SendMessage(hwndComposeEdit, WM_SETFONT, (WPARAM)hSansLargeFont, 0L);
      defaultEditWndProc = (WNDPROC)GetWindowLong(hwndComposeEdit, GWL_WNDPROC);
      SetWindowLong(hwndComposeEdit, GWL_WNDPROC, (LPARAM)ComposeEditWndProc);

      // Load ini file information
      GetPrivateProfileString(szAppName, API_KEY_KEY, "", apiKey, MAX_API_KEY_LENGTH, INI_FILE_NAME);

      return 0;

    case WM_SIZE:
      MoveWindow(
        hwndComposeEdit,
        0, HIWORD(lParam) - iBtnHeight - STATUS_BAR_HEIGHT - 5,
        LOWORD(lParam) - 20 * cxChar, iBtnHeight,
        TRUE
      );
      MoveWindow(
        hwndSendBtn,
        LOWORD(lParam) - 20 * cxChar, HIWORD(lParam) - iBtnHeight - STATUS_BAR_HEIGHT - 5,
        20 * cxChar, iBtnHeight,
        TRUE
      );
      MoveWindow(
        hwndChatLogEdit,
        0, 0,
        LOWORD(lParam), HIWORD(lParam) - iBtnHeight - STATUS_BAR_HEIGHT - 10,
        TRUE
      );
      MoveWindow(
        hwndStatusBar,
        0, HIWORD(lParam) - STATUS_BAR_HEIGHT,
        300, STATUS_BAR_HEIGHT,
        TRUE
      );
      MoveWindow(
        hwndSpareStatusBar,
        300, HIWORD(lParam) - STATUS_BAR_HEIGHT,
        LOWORD(lParam) - 300, STATUS_BAR_HEIGHT,
        TRUE
      );
      return 0;

    case WM_COMMAND:
      if (wParam == 1 && HIWORD(lParam) == EN_ERRSPACE) {
        MessageBox(hwnd, "Edit control is out of space.", szAppName, MB_OK | MB_ICONSTOP);
      } else if (lParam == hwndSendBtn) {
        // Check for empty
        if (GetWindowText(hwndComposeEdit, lpComposeText, 254) == 0) {
          MessageBox(hwnd, "Please type a message to send.", szAppName, MB_OK | MB_ICONEXCLAMATION);
          return 0;
        }
        // Check for API key
        if (strlen(apiKey) == 0) {
          MessageBox(hwnd, "Enter an OpenAI API key in File | Options... to send queries.", szAppName, MB_ICONSTOP | MB_OK);
          return 0;
        }

        SetWindowText(hwndComposeEdit, "");
        SetWindowText(hwndStatusBar, "Performing secure query...");
        AppendTextToEditControl(hwndChatLogEdit, "You: ");
        AppendTextToEditControl(hwndChatLogEdit, lpComposeText);
        AppendTextToEditControl(hwndChatLogEdit, "\r\n\r\n");
        RedrawWindow(hwnd, NULL, NULL, 0);
        UpdateWindow(hwnd);
        ProcessAssistantQuery(hwndChatLogEdit, lpComposeText, apiKey);
        SetWindowText(hwndStatusBar, "Ready");
        SetFocus(hwndComposeEdit);
      } else {
        switch (wParam) {
          case IDM_ABOUT:
            DialogBox(hInstance, "AboutBox", hwnd, lpfnAboutDlgProc);
            return 0;
          case IDM_OPTIONS:
            DialogBox(hInstance, "OptionsDlg", hwnd, lpfnOptionsDlgProc);
            return 0;
          case IDM_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
      }
      return 0;

    case WM_SETFOCUS:
      SetFocus(hwndComposeEdit);
      return 0;

    case WM_DESTROY:
      DeleteObject(SelectObject(hdc, hSansLargeFont));
      DeleteObject(SelectObject(hdc, hAnsiBoldFont));
      PostQuitMessage(0);
      return 0 ;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

#pragma off(unreferenced)
BOOL FAR PASCAL _export AboutDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam) {
#pragma on(unreferenced)
  switch(message) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      switch(wParam) {
        case IDOK:
        case IDCANCEL:
          EndDialog(hDlg, 0);
          return TRUE;
      }
      break;
  }
  return FALSE;
}

#pragma off(unreferenced)
BOOL FAR PASCAL _export OptionsDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam) {
#pragma on(unreferenced)
  switch(message) {
    case WM_INITDIALOG:
      SetDlgItemText(hDlg, IDE_APIKEY, apiKey);
      return TRUE;
    case WM_COMMAND:
      switch(wParam) {
        case IDOK:
          if (GetDlgItemText(hDlg, IDE_APIKEY, apiKey, MAX_API_KEY_LENGTH)) {
            WritePrivateProfileString(szAppName, API_KEY_KEY, apiKey, INI_FILE_NAME);
            EndDialog(hDlg, 0);
          } else {
            MessageBox(hDlg, "You must enter an API key.", szAppName, MB_ICONSTOP | MB_OK);
          }
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg, 0);
          return TRUE;
      }
      break;
  }
  return FALSE;
}

LRESULT CALLBACK ComposeEditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
    SendMessage(hwndSendBtn, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(0, 0));
    SendMessage(hwndSendBtn, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(0, 0));
    return 0;
  }
  return CallWindowProc((FARPROC)defaultEditWndProc, hwnd, msg, wParam, lParam);
}
