#include "osemb.h"
#include <thread>
#include <windows.h>

class OsImpl
  :public Os
{
public:
  OsImpl()
  {

  }
  ~OsImpl()
  {

  }
  virtual void Poll()
  {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  virtual void Flash(uint32_t color, int duration = 3)
  {
    static uint32_t color_;
    static int duration_;
    color_ = color;
    duration_ = duration;

    WNDCLASS wc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "flashcolor";
    wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)->LRESULT{
      switch (message)
      {
      case WM_CREATE:
        SetTimer(hWnd, 0, duration_ * 1000, 0);
        break;
      case WM_TIMER:
        SendMessage(hWnd, WM_PAINT, 0, 0);
        DestroyWindow(hWnd);
        break;
      case WM_CLOSE:
        PostQuitMessage(0);
        break;
      case WM_PAINT:
      {
        PAINTSTRUCT ps;
        RECT rect;
        GetWindowRect(GetDesktopWindow(), &rect);
        HDC hdc = BeginPaint(hWnd, &ps);
        printf("%X\n", color_);
        HBRUSH hbr = CreateSolidBrush(color_);
        FillRect(hdc, &rect, hbr);
        EndPaint(hWnd, &ps);
        DeleteObject(hbr);
      }
      break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
      }
      return 0;
    };
    wc.style = NULL;
    wc.hIcon = LoadIcon(wc.hInstance, "");
    wc.hCursor = LoadCursor((HINSTANCE)NULL, IDC_ARROW);
    wc.lpszMenuName = NULL;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClass(&wc);
    {
      RECT rect;
      GetWindowRect(GetDesktopWindow(), &rect);
      DWORD WindowStyle = WS_POPUP | WS_VISIBLE;
      HWND FlashWindow = NULL;
      if (FlashWindow = CreateWindowEx(WS_EX_TOPMOST, "flashcolor", "flashcolor", WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, wc.hInstance, this))
      {
        UpdateWindow(FlashWindow);
      }
    }

  }
};

Os* Os::Create()
{
  return new OsImpl;
}

