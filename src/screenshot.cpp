#include <fstream>
#include <tuple>

#include "Screenshot.hpp"

std::tuple<std::vector<BYTE>, int, int, int> get_screenshot() {
    int x, y, w, h;
    x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(NULL);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
    BOOL bRet = BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY);

    BITMAP bmpInfo;
    GetObject(hBitmap, sizeof(BITMAP), &bmpInfo);

    int stride = bmpInfo.bmWidthBytes;
    int size = bmpInfo.bmHeight * stride;

    std::cout << "Color planes: " << bmpInfo.bmPlanes << '\n';
    int bytesPerPixel = (bmpInfo.bmBitsPixel + 7) / 8;
    std::cout << "bytes per pixel: " << bytesPerPixel << '\n';
    int newStride = 4 * ((bmpInfo.bmWidth * bytesPerPixel + 3) / 4);

    std::cout << "Strides old: " << stride << '\n';
    std::cout << "Strides new: " << newStride << '\n';

    std::vector<BYTE> pixels(size);
    GetBitmapBits(hBitmap, size, pixels.data());

    SelectObject(hDC, old_obj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    DeleteObject(hBitmap);

    return std::make_tuple(std::move(pixels), bmpInfo.bmWidth, bmpInfo.bmHeight, stride);
}
