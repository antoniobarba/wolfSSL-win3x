// Windows Status Bar
// By Philip J. Erdelsky
// Public Domain, from http://www.efgh.com/old_website/status.html

#include <windows.h>

const int MAX_TEXT_LENGTH = 127;
const int MARGIN_WIDTH = 8;
const int PADDING_WIDTH = 5;
const int MARGIN_HEIGHT = 2;

LRESULT CALLBACK StatusBarProc(HWND, UINT, WPARAM, LPARAM);


/*-----------------------------------------------------------------------------
This function registers the STATUSBAR class.

The external variables _hPrev and _hInstance duplicate the arguments
hPrevInstance and hInstance, which are passed to WinMain(). If the startup
code does not supply these external variables, you must pass the arguments to
this function and call it explicitly before invoking any PICTUREBUTTON
control.
-----------------------------------------------------------------------------*/

void register_status_bar(HANDLE hInstance, HANDLE hPrevInstance) {
    WNDCLASS w;

    if (!hPrevInstance) {
        w.cbClsExtra = 0;
        w.cbWndExtra = sizeof(HGLOBAL);
        w.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
        w.hCursor = LoadCursor(NULL, IDC_ARROW);
        w.hIcon = NULL;
        w.hInstance = hInstance;
        w.lpfnWndProc = StatusBarProc;
        w.lpszClassName = "STATUSBAR";
        w.lpszMenuName = NULL;
        w.style = 0;
        RegisterClass(&w);
  }
}

typedef struct {
  int height;
  int width;
  long style;
  char text[127];
  int text_length;
  HFONT font;
} STATUSBAR;

#ifdef WIN32

  inline void MoveTo(HDC dc, int x, int y)
  {
    MoveToEx(dc, x, y, NULL);
  }

  #define GetWindowHandle GetWindowLong
  #define SetWindowHandle SetWindowLong
  #define GWH_HINSTANCE GWL_HINSTANCE
  #define HANDLE_WORD DWORD

  inline void PostClickedMessage(HWND handle)
  {
    PostMessage(GetParent(handle), WM_COMMAND,
    MAKELONG((WORD) GetMenu(handle), BN_CLICKED), (LPARAM) handle);
  }

  static DWORD GetTextExtent(HDC dc, LPCSTR string, int length)
  {
    SIZE size;
    GetTextExtentPoint(dc, string, length, &size);
    return MAKELONG(size.cx, size.cy);
  }


#else

  #define GetWindowHandle GetWindowWord
  #define SetWindowHandle SetWindowWord
  #define GWH_HINSTANCE GWW_HINSTANCE
  #define HANDLE_WORD WORD

  inline void PostClickedMessage(HWND handle)
  {
    PostMessage(GetParent(handle), WM_COMMAND,
      GetMenu(handle), MAKELONG(handle, BN_CLICKED));
  }

#endif

/*-----------------------------------------------------------------------------
This function receives all messages directed to the control.
-----------------------------------------------------------------------------*/

LRESULT CALLBACK StatusBarProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  STATUSBAR far *p;
  HGLOBAL hp = (HGLOBAL) GetWindowHandle(handle, 0);
  HPEN dark_gray_pen;

  if (hp != NULL)
    p = (STATUSBAR far *) GlobalLock(hp);
  switch (message)
  {
    case WM_CREATE:
    {
      hp = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(STATUSBAR));
      if (hp == NULL)
        return -1L;
      p = (STATUSBAR __far *) GlobalLock(hp);
      p->height = ((LPCREATESTRUCT) lParam)->cy;
      p->width = ((LPCREATESTRUCT) lParam)->cx;
      p->style = ((LPCREATESTRUCT) lParam)->style;
      {
        int i, c;
        for (i = 0; i < MAX_TEXT_LENGTH &&
          (c = ((LPCREATESTRUCT) lParam)->lpszName[i]) != 0; i++)
        {
          p->text[i] = c;
        }
        p->text_length = i;
      }
      SetWindowHandle(handle, 0, (HANDLE_WORD) hp);
      GlobalUnlock(hp);
      return 0;
    }
    case WM_SIZE:
    {
      p->height = HIWORD(lParam);
      p->width = LOWORD(lParam);
      InvalidateRect(handle, NULL, TRUE);
      return 0;
    }
    case WM_SETTEXT:
    {
      int i, c;
      for (i = 0; i < MAX_TEXT_LENGTH &&
          (c = ((char far *) lParam)[i]) != 0; i++)
      {
        p->text[i] = c;
      }
      p->text_length = i;
      InvalidateRect(handle, NULL, TRUE);
      GlobalUnlock(hp);
      return 0;
    }
    case WM_GETTEXT:
    {
      int i;
      for (i = 0; i < p->text_length && i+1 < (int) wParam; i++)
        ((char far *) lParam)[i] = p->text[i];
      ((char far *) lParam)[i++] = 0;
      GlobalUnlock(hp);
      return (long) i;
    }
    case WM_SETFONT:
      p->font = (HFONT) wParam;
      InvalidateRect(handle, NULL, TRUE);
      GlobalUnlock(hp);
      return 0;
    case WM_GETFONT:
      GlobalUnlock(hp);
      return (long) (HANDLE_WORD) p->font;
    case WM_PAINT:
    {
      const COLORREF LIGHT_GRAY = RGB(192, 192, 192);
      const COLORREF DARK_GRAY = RGB(128, 128, 128);
      const COLORREF BLACK = RGB(0, 0, 0);
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(handle, &paint);
      int height = p->height;
      int width = p->width;
      HPEN old_pen = SelectObject(dc, GetStockObject(BLACK_PEN));
      HBRUSH old_brush = SelectObject(dc, GetStockObject(LTGRAY_BRUSH));
      int marginLeftWidth = (p->style & WS_GROUP) ? 8 : 0;

      // Background
      HPEN light_gray_pen = CreatePen(PS_SOLID, 1, LIGHT_GRAY);
      SelectObject(dc, light_gray_pen);
      Rectangle(dc, 0, 1, width, height);

      // Right and bottom white lines
      SelectObject(dc, GetStockObject(WHITE_PEN));
      DeleteObject(light_gray_pen);
      MoveTo(dc, marginLeftWidth + 1, height - MARGIN_HEIGHT - 1); // Bottom left corner
      LineTo(dc, width - 1 - MARGIN_WIDTH, height - MARGIN_HEIGHT - 1); // Bottom right corner
      LineTo(dc, width - 1 - MARGIN_WIDTH, MARGIN_HEIGHT + 1);

      // Topmost line
      MoveTo(dc, 0, 1);
      LineTo(dc, width, 1);

      // Even topper black line
      SelectObject(dc, old_pen);
      MoveTo(dc, 0, 0);
      LineTo(dc, width, 0);

      // Top and left dark gray lines
      dark_gray_pen = CreatePen(PS_SOLID, 1, DARK_GRAY);
      SelectObject(dc, dark_gray_pen);
      MoveTo(dc, marginLeftWidth, height - MARGIN_HEIGHT - 1); // Bottom left corner
      LineTo(dc, marginLeftWidth, MARGIN_HEIGHT + 1); // Top left corner
      LineTo(dc, width - MARGIN_WIDTH, MARGIN_HEIGHT + 1); // Top right corner
      SelectObject(dc, old_pen);
      DeleteObject(dark_gray_pen);

      if (p->text_length != 0)
      {
        HFONT old_font;
        if (p->font != NULL)
        old_font = SelectObject(dc, p->font);
        SetBkColor(dc, LIGHT_GRAY);
        SetTextColor(dc, BLACK);
        TextOut(dc, marginLeftWidth + 1 + PADDING_WIDTH /* LOWORD(GetTextExtent(dc, "m", 1)) / 2*/,
          (height - HIWORD(GetTextExtent(dc, p->text, p->text_length)))/2 /*- 1*/,
          p->text, p->text_length);
        if (p->font != NULL)
          SelectObject(dc, old_font);
      }
      SelectObject(dc, old_brush);
      EndPaint(handle, &paint);
      GlobalUnlock(hp);
      return 0;
    }
    case WM_DESTROY:
      GlobalUnlock(hp);
      GlobalFree(hp);
      return 0;
  }
  if (hp != NULL) GlobalUnlock(hp);
  return DefWindowProc(handle, message, wParam, lParam);
}
