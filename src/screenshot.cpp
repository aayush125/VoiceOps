#include <fstream>
#include <tuple>

#include "Screenshot.hpp"

void save_to_file(std::tuple<std::vector<BYTE>, int, int, int> imagedata) {
    std::vector<BYTE> pixels;
    int width, height, rowstride;
    std::tie(pixels, width, height, rowstride) = imagedata;

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;


    std::string filename = "C:/opengl-coding/ajhaibhayena.bmp";
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        BITMAPFILEHEADER bfh;
        bfh.bfType = 0x4D42; // 'BM'
        bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixels.size();
        bfh.bfReserved1 = 0;
        bfh.bfReserved2 = 0;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        file.write(reinterpret_cast<char*>(&bfh), sizeof(BITMAPFILEHEADER));
        file.write(reinterpret_cast<char*>(&bi), sizeof(BITMAPINFOHEADER));
        file.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
        file.close();
        std::cout << "Screenshot saved to " << filename << std::endl;
    } else {
        std::cerr << "Failed to open file " << filename << std::endl;
    }
}

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

// std::tuple<std::vector<BYTE>, int, int, int> get_screenshot() {
//     int x, y, w, h;
//     x = GetSystemMetrics(SM_XVIRTUALSCREEN);
//     y = GetSystemMetrics(SM_YVIRTUALSCREEN);
//     w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
//     h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

//     HDC hScreen = GetDC(NULL);
//     HDC hDC = CreateCompatibleDC(NULL);
//     HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
//     HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
//     BOOL bRet = BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY);

//     BITMAP bmp;
//     GetObject(hBitmap, sizeof(BITMAP), &bmp);

//     BITMAPINFOHEADER bi;
//     bi.biSize = sizeof(BITMAPINFOHEADER);
//     bi.biWidth = bmp.bmWidth;
//     bi.biHeight = bmp.bmHeight;
//     bi.biPlanes = 1;
//     bi.biBitCount = 32;
//     bi.biCompression = BI_RGB;
//     bi.biSizeImage = 0;
//     bi.biXPelsPerMeter = 0;
//     bi.biYPelsPerMeter = 0;
//     bi.biClrUsed = 0;
//     bi.biClrImportant = 0;

//     std::vector<BYTE> pixels(bi.biWidth * bi.biHeight * 4);
//     GetDIBits(hDC, hBitmap, 0, bi.biHeight, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

//     SelectObject(hDC, old_obj);
//     DeleteDC(hDC);
//     ReleaseDC(NULL, hScreen);
//     DeleteObject(hBitmap);

//     std::vector<BYTE> rgb_pixels(bi.biWidth * bi.biHeight * 3);
//     for (int i = 0; i < bi.biWidth * bi.biHeight; ++i) {
//         rgb_pixels[i * 3 + 0] = pixels[i * 4 + 2]; // Red
//         rgb_pixels[i * 3 + 1] = pixels[i * 4 + 1]; // Green
//         rgb_pixels[i * 3 + 2] = pixels[i * 4 + 0]; // Blue
//     }

//     return std::make_tuple(std::move(rgb_pixels), bi.biWidth, bi.biHeight, bi.biWidth * 3);
// }