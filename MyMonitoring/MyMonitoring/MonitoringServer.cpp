#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <Winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <map>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <ctime>
#include "screenshot.h"
#include <string>
#include <locale>
#include <codecvt>
#include <filesystem>
using namespace std::chrono;




struct AppInfo {
    std::string name;
    milliseconds activeTime;

    AppInfo() : activeTime(0) {}
};

void send_json(SOCKET clientSocket, std::map<std::string, AppInfo>& appInfoMap) {
    nlohmann::json jsonData;
    for (const auto& pair : appInfoMap) {
        //jsonData[std::string(pair.second.name)]["name"] = std::string(pair.second.name);
        jsonData[std::string(pair.second.name)]["activeTime ms"] = pair.second.activeTime.count();
    }

    std::string jsonString = jsonData.dump(-1, ' ', true, nlohmann::json::error_handler_t::ignore);
    int bytesSent = send(clientSocket, jsonString.c_str(), jsonString.size(), 0);
    if (bytesSent == SOCKET_ERROR) {
        // Обработка ошибок при отправке данных на сервер
    }
}

std::string ConvertTCHARToString(const TCHAR* tcharString) {
    int size = WideCharToMultiByte(CP_UTF8, 0, tcharString, -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) 
        return "Unknoew"; // Обработка ошибки
    
    std::string utf8String(size, 0);
    if (WideCharToMultiByte(CP_UTF8, 0, tcharString, -1, &utf8String[0], size, nullptr, nullptr) == 0) 
        return "Unknoew"; // Обработка ошибки
    return utf8String;
}

void sendComputerInfo(SOCKET clientSocket) {
    //DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    TCHAR username[MAX_PATH];
    char machine[MAX_COMPUTERNAME_LENGTH + 1];
    char domain[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_PATH;

    if (GetComputerNameA(machine, &size) && 
        GetUserName(username, &size) &&
        GetComputerNameExA(ComputerNameDnsDomain, domain, &size)) 
    {
        std::string computerInfo = ConvertTCHARToString(username) + " " + std::string(machine) + " " + std::string(domain);
        if (std::string(domain).empty()) {
            computerInfo += "Unknown";
        }
        send(clientSocket, computerInfo.c_str(), computerInfo.length(), 0);
    }
    else {
        std::cerr << "Не удалось получить имя компьютера." << std::endl;
    }

}

void HandleClientData(SOCKET clientSocket, std::map<std::string, AppInfo>& appInfoMap) {
    sendComputerInfo(clientSocket);
    while (true) {
        char key[1024];
        byte bytesRead = recv(clientSocket, key, sizeof(key), 0);
        if (bytesRead == -1) {
            // Обработка ошибок при приеме данных
        }
        else {
            key[bytesRead] = '\0';
            if (strcmp(key, "screenshot") == 0) {
                CaptureScreenshot(clientSocket);
            }
            if (strcmp(key, "json") == 0) {
                send_json(clientSocket, appInfoMap);
            }
        }
    }
}

void addToRegistry() {
    TCHAR exePath[MAX_PATH]; DWORD pathLength = GetModuleFileName(NULL, exePath, MAX_PATH);
    if (pathLength == 0) 
        return; //"Ошибка при получении пути к исполняемому файлу."

    HKEY hKey; // Открываем ключ для записи
    LPCTSTR lpValueName = TEXT("MonitoringServer"); 
    LPCTSTR lpSubKey = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    LPCTSTR lpData = exePath; 

    if (RegOpenKeyEx(HKEY_CURRENT_USER, lpSubKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        if (RegSetValueEx(hKey, lpValueName, 0, REG_SZ, (LPBYTE)lpData, sizeof(TCHAR) * (lstrlen(lpData) + 1)) == ERROR_SUCCESS)
            RegCloseKey(hKey);
        //else std::cerr << "Ошибка при добавлении в реестр." << std::endl;
}
bool g_ShutdownRequested = false;

// Функция, которая будет вызвана перед выключением компьютера
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_LOGOFF_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT) {
        g_ShutdownRequested = true;
        return TRUE;
    }
    return FALSE;
}

std::string convertHWNDToUTF8(HWND hwnd) {
    // Получите текст в формате Unicode
    wchar_t windowTitle[512]; // Предположим, что 512 - это максимальная длина заголовка окна
    GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0]));

    // Преобразуйте его в UTF-8
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, windowTitle, -1, nullptr, 0, nullptr, nullptr);
    std::string utf8WindowTitle(utf8Length, 0);
    WideCharToMultiByte(CP_UTF8, 0, windowTitle, -1, &utf8WindowTitle[0], utf8Length, nullptr, nullptr);
    utf8WindowTitle.pop_back();
    return utf8WindowTitle;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{

    addToRegistry();
    // Регистрируем обработчик события выключения
    //SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    SetConsoleOutputCP(CP_UTF8);

    std::map<std::string, AppInfo> appInfoMap;

    

    high_resolution_clock::time_point prevTimePoint = high_resolution_clock::now();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // Обработка ошибок при инициализации Winsock
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        // Обработка ошибок при создании сокета
        WSACleanup();
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8085);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        // Обработка ошибок при подключении к серверу
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    std::thread(HandleClientData, clientSocket, std::ref(appInfoMap)).detach();
    HWND prevHwnd = GetForegroundWindow();
    std::string prevAppName = convertHWNDToUTF8(prevHwnd);
    
    while (!g_ShutdownRequested) {
        HWND hwnd = GetForegroundWindow();

        if (hwnd != NULL && hwnd != prevHwnd) {
            std::string appName = convertHWNDToUTF8(hwnd);
            if (appName == "")
                continue;

            high_resolution_clock::time_point currentTime = high_resolution_clock::now();
            milliseconds elapsed = duration_cast<milliseconds>(currentTime - prevTimePoint);
            appInfoMap[prevAppName].activeTime += elapsed;
            appInfoMap[prevAppName].name = prevAppName;

            prevHwnd = hwnd;
            prevAppName = appName;
            prevTimePoint = currentTime;
        }

        Sleep(1000);
    }

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
