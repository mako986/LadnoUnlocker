// LadnoUnlocker.cpp : Определяет точку входа для приложения.
#include "framework.h"
#include "LadnoUnlocker.h"
#include <tlhelp32.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winsvc.h>
#include <vector>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")

#define MAX_LOADSTRING 100
#define ID_LISTVIEW 101

#define ID_BTN_TASKMGR     201
#define ID_BTN_AUTOSTART   202
#define ID_BTN_RESTRICTION 203
#define ID_BTN_ASSOC       204
#define ID_BTN_EXPLORER    205
#define ID_BTN_SERVICES    206
#define ID_BTN_TASKS       207

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND hListView = NULL;

HWND hBtnTaskMgr = NULL;
HWND hBtnAutoStart = NULL;
HWND hBtnRestriction = NULL;
HWND hBtnAssoc = NULL;
HWND hBtnExplorer = NULL;
HWND hBtnServices = NULL;
HWND hBtnTasks = NULL;

int currentMode = 0;
std::wstring currentExplorerPath = L"C:\\";

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void                ResetColumns(HWND hList, LPCWSTR col1, int width1, LPCWSTR col2, int width2);
void                LoadProcesses(HWND hList);
void                LoadAutostart(HWND hList);
void                LoadRestrictions(HWND hList);
void                LoadAssociations(HWND hList);
void                LoadExplorer(HWND hList, std::wstring path);
void                LoadServicesNative(HWND hList);
void                LoadTasksNative(HWND hList);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LADNOUNLOCKER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LADNOUNLOCKER));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LADNOUNLOCKER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LADNOUNLOCKER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 950, 650, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return FALSE;

    BOOL useImmersiveDarkMode = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &useImmersiveDarkMode, sizeof(useImmersiveDarkMode));

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

void ResetColumns(HWND hList, LPCWSTR col1, int width1, LPCWSTR col2, int width2)
{
    ListView_DeleteAllItems(hList);
    while (ListView_DeleteColumn(hList, 0) == TRUE);

    LVCOLUMN col = { 0 };
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = const_cast<LPWSTR>(col1);
    col.cx = width1;
    ListView_InsertColumn(hList, 0, &col);

    col.pszText = const_cast<LPWSTR>(col2);
    col.cx = width2;
    ListView_InsertColumn(hList, 1, &col);
}

void LoadProcesses(HWND hList)
{
    currentMode = 1;
    ResetColumns(hList, L"PID", 120, L"Имя процесса", 550);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        int index = 0;
        do {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
            wchar_t pidBuffer[32];
            wsprintfW(pidBuffer, L"%u", pe32.th32ProcessID);
            lvItem.pszText = pidBuffer;

            int inserted = ListView_InsertItem(hList, &lvItem);
            if (inserted != -1) ListView_SetItemText(hList, inserted, 1, pe32.szExeFile);
            index++;
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
}

void LoadAutostart(HWND hList)
{
    currentMode = 2;
    ResetColumns(hList, L"Параметр / Источник", 250, L"Значение / Путь", 500);

    int index = 0;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR value[512] = { 0 };
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AppInit_DLLs", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
            lvItem.pszText = const_cast<LPWSTR>(L"AppInit_DLLs (HKLM)");
            int inserted = ListView_InsertItem(hList, &lvItem);
            if (inserted != -1) ListView_SetItemText(hList, inserted, 1, value[0] ? value : const_cast<LPWSTR>(L"(пусто)"));
            index++;
        }
        RegCloseKey(hKey);
    }
}

void LoadRestrictions(HWND hList)
{
    currentMode = 3;
    ResetColumns(hList, L"Политика / Блокировка", 300, L"Состояние (Двойной клик сбросит блокировку)", 450);

    int index = 0;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hKey, L"DisableTaskMgr", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
            lvItem.pszText = const_cast<LPWSTR>(L"DisableTaskMgr (Диспетчер задач)");
            int inserted = ListView_InsertItem(hList, &lvItem);
            if (inserted != -1) ListView_SetItemText(hList, inserted, 1, val ? const_cast<LPWSTR>(L"ЗАБЛОКИРОВАНО (кликните для сброса)") : const_cast<LPWSTR>(L"Свободно"));
            index++;
        }
        if (RegQueryValueExW(hKey, L"DisableRegistryTools", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
            lvItem.pszText = const_cast<LPWSTR>(L"DisableRegistryTools (Редактор реестра)");
            int inserted = ListView_InsertItem(hList, &lvItem);
            if (inserted != -1) ListView_SetItemText(hList, inserted, 1, val ? const_cast<LPWSTR>(L"ЗАБЛОКИРОВАНО (кликните для сброса)") : const_cast<LPWSTR>(L"Свободно"));
            index++;
        }
        RegCloseKey(hKey);
    }
}

void LoadAssociations(HWND hList)
{
    currentMode = 4;
    ResetColumns(hList, L"Команда / Описание", 250, L"Действие (Двойной клик восстановит ассоциации)", 450);

    LPWSTR items[][2] = {
        { const_cast<LPWSTR>(L"Восстановить .exe"), const_cast<LPWSTR>(L"assoc .exe=exefile & ftype exefile=\"%1\" %*") },
        { const_cast<LPWSTR>(L"Восстановить .bat"), const_cast<LPWSTR>(L"assoc .bat=batfile & ftype batfile=\"%1\" %*") },
        { const_cast<LPWSTR>(L"Восстановить .cmd"), const_cast<LPWSTR>(L"assoc .cmd=cmdfile & ftype cmdfile=\"%1\" %*") }
    };

    for (int i = 0; i < 3; i++) {
        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT; lvItem.iItem = i; lvItem.iSubItem = 0;
        lvItem.pszText = items[i][0];
        int inserted = ListView_InsertItem(hList, &lvItem);
        if (inserted != -1) ListView_SetItemText(hList, inserted, 1, items[i][1]);
    }
}

void LoadExplorer(HWND hList, std::wstring path)
{
    currentMode = 5;
    currentExplorerPath = path;
    ResetColumns(hList, L"Имя файла / Папки", 350, L"Путь / Тип", 350);

    int index = 0;
    if (path == L"") {
        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<LPWSTR>(L"[..] Назад к дискам");
        int inserted = ListView_InsertItem(hList, &lvItem);
        if (inserted != -1) ListView_SetItemText(hList, inserted, 1, const_cast<LPWSTR>(L"Корень"));
        index++;

        WCHAR drives[256] = { 0 };
        GetLogicalDriveStringsW(256, drives);
        wchar_t* pDrive = drives;
        while (*pDrive) {
            LVITEM item = { 0 };
            item.mask = LVIF_TEXT; item.iItem = index; item.iSubItem = 0;
            item.pszText = pDrive;
            int ins = ListView_InsertItem(hList, &item);
            if (ins != -1) ListView_SetItemText(hList, ins, 1, const_cast<LPWSTR>(L"Диск"));
            index++;
            pDrive += wcslen(pDrive) + 1;
        }
        return;
    }

    LVITEM lvItem = { 0 };
    lvItem.mask = LVIF_TEXT; lvItem.iItem = index; lvItem.iSubItem = 0;
    lvItem.pszText = const_cast<LPWSTR>(L"[..] Наверх");
    int inserted = ListView_InsertItem(hList, &lvItem);
    if (inserted != -1) ListView_SetItemText(hList, inserted, 1, const_cast<LPWSTR>(path.c_str()));
    index++;

    std::wstring searchPath = path + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
            LVITEM item = { 0 };
            item.mask = LVIF_TEXT; item.iItem = index; item.iSubItem = 0;
            item.pszText = findData.cFileName;
            int ins = ListView_InsertItem(hList, &item);
            if (ins != -1) ListView_SetItemText(hList, ins, 1, (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? const_cast<LPWSTR>(L"[Папка]") : const_cast<LPWSTR>(L"Файл"));
            index++;
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
}

void LoadServicesNative(HWND hList)
{
    currentMode = 6;
    ResetColumns(hList, L"Имя службы (Системное)", 250, L"Отображаемое имя / Статус", 450);

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCManager) return;

    DWORD bytesNeeded = 0, servicesReturned = 0, resumeHandle = 0;
    EnumServicesStatusExW(hSCManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        NULL, 0, &bytesNeeded, &servicesReturned, &resumeHandle, NULL);

    std::vector<BYTE> buffer(bytesNeeded);
    if (EnumServicesStatusExW(hSCManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        buffer.data(), (DWORD)buffer.size(), &bytesNeeded, &servicesReturned, &resumeHandle, NULL)) {

        LPENUM_SERVICE_STATUS_PROCESSW pServices = (LPENUM_SERVICE_STATUS_PROCESSW)buffer.data();
        for (DWORD i = 0; i < servicesReturned; i++) {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_TEXT; lvItem.iItem = (int)i; lvItem.iSubItem = 0;
            lvItem.pszText = pServices[i].lpServiceName;
            int inserted = ListView_InsertItem(hList, &lvItem);

            std::wstring statusStr = pServices[i].lpDisplayName;
            statusStr += (pServices[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING) ? L" [ЗАПУЩЕНА]" : L" [ОСТАНОВЛЕНА]";
            if (inserted != -1) ListView_SetItemText(hList, inserted, 1, const_cast<LPWSTR>(statusStr.c_str()));
        }
    }
    CloseServiceHandle(hSCManager);
}

void LoadTasksNative(HWND hList)
{
    currentMode = 7;
    ResetColumns(hList, L"Действие / Команда", 300, L"Описание (Двойной клик выполнит команду)", 400);

    LPWSTR tasks[][2] = {
        { const_cast<LPWSTR>(L"schtasks /query /fo LIST"), const_cast<LPWSTR>(L"Показать все системные задачи в консоли") },
        { const_cast<LPWSTR>(L"net stop Schedule"), const_cast<LPWSTR>(L"Остановить службу планировщика") },
        { const_cast<LPWSTR>(L"net start Schedule"), const_cast<LPWSTR>(L"Запустить службу планировщика") }
    };

    for (int i = 0; i < 3; i++) {
        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT; lvItem.iItem = i; lvItem.iSubItem = 0;
        lvItem.pszText = tasks[i][0];
        int inserted = ListView_InsertItem(hList, &lvItem);
        if (inserted != -1) ListView_SetItemText(hList, inserted, 1, tasks[i][1]);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        int btnX = 10;
        hBtnTaskMgr = CreateWindowW(L"BUTTON", L"Processes", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_TASKMGR, hInst, NULL);
        btnX += 110;
        hBtnAutoStart = CreateWindowW(L"BUTTON", L"Autostart", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_AUTOSTART, hInst, NULL);
        btnX += 110;
        hBtnRestriction = CreateWindowW(L"BUTTON", L"Policies", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_RESTRICTION, hInst, NULL);
        btnX += 110;
        hBtnAssoc = CreateWindowW(L"BUTTON", L"Assoc", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_ASSOC, hInst, NULL);
        btnX += 110;
        hBtnExplorer = CreateWindowW(L"BUTTON", L"Explorer", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_EXPLORER, hInst, NULL);
        btnX += 110;
        hBtnServices = CreateWindowW(L"BUTTON", L"Services", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 100, 30, hWnd, (HMENU)ID_BTN_SERVICES, hInst, NULL);
        btnX += 110;
        hBtnTasks = CreateWindowW(L"BUTTON", L"TaskSch", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, 10, 90, 30, hWnd, (HMENU)ID_BTN_TASKS, hInst, NULL);

        INITCOMMONCONTROLSEX icEx = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icEx);

        hListView = CreateWindowEx(
            0, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 50, 915, 540,
            hWnd, (HMENU)ID_LISTVIEW, hInst, NULL
        );

        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        SendMessage(hListView, LVM_SETBKCOLOR, 0, (LPARAM)RGB(45, 45, 45));
        SendMessage(hListView, LVM_SETTEXTBKCOLOR, 0, (LPARAM)RGB(45, 45, 45));
        SendMessage(hListView, LVM_SETTEXTCOLOR, 0, (LPARAM)RGB(240, 240, 240));

        LoadProcesses(hListView);
    }
    break;

    case WM_NOTIFY:
    {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->idFrom == ID_LISTVIEW && nmhdr->code == NM_DBLCLK) {
            LPNMITEMACTIVATE itemAct = (LPNMITEMACTIVATE)lParam;
            if (itemAct->iItem != -1) {
                wchar_t text[512] = { 0 };
                ListView_GetItemText(hListView, itemAct->iItem, 0, text, 512);

                if (currentMode == 5) {
                    if (wcscmp(text, L"[..] Наверх") == 0 || wcscmp(text, L"[..] Назад к дискам") == 0) {
                        size_t found = currentExplorerPath.find_last_of(L"\\");
                        if (found == 2 || found == std::wstring::npos) LoadExplorer(hListView, L"");
                        else LoadExplorer(hListView, currentExplorerPath.substr(0, found));
                    }
                    else {
                        wchar_t type[64] = { 0 };
                        ListView_GetItemText(hListView, itemAct->iItem, 1, type, 64);
                        if (wcscmp(type, L"[Папка]") == 0 || wcscmp(type, L"Диск") == 0) {
                            std::wstring newPath = (currentExplorerPath == L"") ? std::wstring(text) : currentExplorerPath + L"\\" + text;
                            if (newPath.length() == 2 && newPath[1] == L':') newPath += L"\\";
                            LoadExplorer(hListView, newPath);
                        }
                        else {
                            ShellExecuteW(hWnd, L"open", (currentExplorerPath + L"\\" + text).c_str(), NULL, currentExplorerPath.c_str(), SW_SHOW);
                        }
                    }
                }
                else if (currentMode == 3) {
                    HKEY hKey;
                    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                        DWORD zero = 0;
                        if (wcsstr(text, L"DisableTaskMgr")) RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
                        if (wcsstr(text, L"DisableRegistryTools")) RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
                        RegCloseKey(hKey);
                        LoadRestrictions(hListView);
                        MessageBoxW(hWnd, L"Ограничение успешно снято!", L"Успех", MB_OK);
                    }
                }
                else if (currentMode == 4) {
                    wchar_t cmd[512] = { 0 };
                    ListView_GetItemText(hListView, itemAct->iItem, 1, cmd, 512);
                    ShellExecuteW(hWnd, L"open", L"cmd.exe", (std::wstring(L"/c ") + cmd).c_str(), NULL, SW_SHOW);
                }
                else if (currentMode == 6) {
                    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
                    if (hSCM) {
                        SC_HANDLE hSvc = OpenServiceW(hSCM, text, SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
                        if (hSvc) {
                            SERVICE_STATUS_PROCESS ssp;
                            DWORD bytesNeeded;
                            QueryServiceStatusEx(hSvc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded);
                            if (ssp.dwCurrentState == SERVICE_RUNNING) {
                                ControlService(hSvc, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
                                MessageBoxW(hWnd, L"Служба остановлена", L"Готово", MB_OK);
                            }
                            else {
                                StartServiceW(hSvc, 0, NULL);
                                MessageBoxW(hWnd, L"Служба запущенна", L"Готово", MB_OK);
                            }
                            CloseServiceHandle(hSvc);
                            LoadServicesNative(hListView);
                        }
                        CloseServiceHandle(hSCM);
                    }
                }
                else if (currentMode == 7) {
                    ShellExecuteW(hWnd, L"open", L"cmd.exe", (std::wstring(L"/k ") + text).c_str(), NULL, SW_SHOW);
                }
            }
        }
    }
    break;

    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        if (hListView) MoveWindow(hListView, 10, 50, rc.right - 20, rc.bottom - 60, TRUE);
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_BTN_TASKMGR: LoadProcesses(hListView); break;
        case ID_BTN_AUTOSTART: LoadAutostart(hListView); break;
        case ID_BTN_RESTRICTION: LoadRestrictions(hListView); break;
        case ID_BTN_ASSOC: LoadAssociations(hListView); break;
        case ID_BTN_EXPLORER: LoadExplorer(hListView, L"C:"); break;
        case ID_BTN_SERVICES: LoadServicesNative(hListView); break;
        case ID_BTN_TASKS: LoadTasksNative(hListView); break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    if (message == WM_INITDIALOG) return (INT_PTR)TRUE;
    if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}