#include "cimmerian/visual/platform/win32-screen-capture.hpp"
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace Cimmerian::Visual {

Screenshot Win32ScreenCapture::Capture(void* windowHandle)
{
  HWND hwnd = windowHandle ? static_cast<HWND>(windowHandle) : GetDesktopWindow();

  RECT rect;
  if (!GetClientRect(hwnd, &rect)) {
    throw std::runtime_error("Win32ScreenCapture: GetClientRect failed");
  }
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;

  HDC windowDC = GetDC(hwnd);
  HDC memoryDC = CreateCompatibleDC(windowDC);
  HBITMAP bitmap = CreateCompatibleBitmap(windowDC, width, height);
  HGDIOBJ oldBitmap = SelectObject(memoryDC, bitmap);

  if (!BitBlt(memoryDC, 0, 0, width, height, windowDC, 0, 0, SRCCOPY)) {
    SelectObject(memoryDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDC);
    ReleaseDC(hwnd, windowDC);
    throw std::runtime_error("Win32ScreenCapture: BitBlt failed");
  }

  BITMAPINFOHEADER bitmapInfoHeader {};
  bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmapInfoHeader.biWidth = width;
  bitmapInfoHeader.biHeight = -height; // negative height selects a top-down DIB
  bitmapInfoHeader.biPlanes = 1;
  bitmapInfoHeader.biBitCount = 32;
  bitmapInfoHeader.biCompression = BI_RGB;

  Screenshot shot;
  shot.width = width;
  shot.height = height;
  shot.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);

  std::vector<uint8_t> bgra(shot.pixels.size());
  GetDIBits(
      memoryDC, bitmap, 0, static_cast<UINT>(height), bgra.data(),
      reinterpret_cast<BITMAPINFO*>(&bitmapInfoHeader), DIB_RGB_COLORS
  );

  for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) {
    shot.pixels[i + 0] = bgra[i + 2]; // R
    shot.pixels[i + 1] = bgra[i + 1]; // G
    shot.pixels[i + 2] = bgra[i + 0]; // B
    shot.pixels[i + 3] = 255;
  }

  SelectObject(memoryDC, oldBitmap);
  DeleteObject(bitmap);
  DeleteDC(memoryDC);
  ReleaseDC(hwnd, windowDC);

  return shot;
}

} // namespace Cimmerian::Visual
