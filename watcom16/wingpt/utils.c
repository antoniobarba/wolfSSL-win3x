#include <windows.h>

void AppendTextToEditControl(HWND hwnd, LPCSTR newText) {
  int iTextLength;

  // Move caret to end
  iTextLength = GetWindowTextLength(hwnd);
  SendMessage(hwnd, EM_SETSEL, 0, MAKELONG(iTextLength, iTextLength));

  // Insert text at current position
  SendMessage(hwnd, EM_REPLACESEL, TRUE, (LONG)newText);
}
