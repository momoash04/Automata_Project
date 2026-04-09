#include <windows.h>
#include <string>
#include "DFABackend.h"

// Colors
COLORREF bgMain = RGB(30, 30, 30);
COLORREF editBg = RGB(45, 45, 45);
COLORREF textMain = RGB(220, 220, 220);

// State variables for drawing
static std::wstring currentStateName = L"Start\n0 Ones";
static bool isAccepted = false;
static bool isError = false;

// Window procedure for the GUI
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hInput, hResult, hTitle, hLabel;
    static HBRUSH hbrBgMain, hbrEditBg;
    static HFONT hFontMain, hFontTitle, hFontResult, hFontCircle;
    
    switch (uMsg) {
        case WM_CREATE:
        {
            hbrBgMain = CreateSolidBrush(bgMain);
            hbrEditBg = CreateSolidBrush(editBg);
            
            hTitle = CreateWindowW(L"STATIC", L"DFA Real-Time Validator", WS_CHILD | WS_VISIBLE | SS_CENTER, 
                                   0, 20, 640, 35, hwnd, NULL, NULL, NULL);

            hLabel = CreateWindowW(L"STATIC", L"Input Binary String:", WS_CHILD | WS_VISIBLE, 
                                   50, 75, 160, 25, hwnd, NULL, NULL, NULL);
                          
            hInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
                                   220, 70, 360, 28, hwnd, (HMENU) 2, NULL, NULL);
                                       
            hResult = CreateWindowW(L"STATIC", L"Result: Waiting for input...", WS_CHILD | WS_VISIBLE | SS_CENTER, 
                                    50, 310, 530, 40, hwnd, NULL, NULL, NULL);
            
            hFontMain = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontTitle = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontResult = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontCircle = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            SendMessage(hInput, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            SendMessage(hResult, WM_SETFONT, (WPARAM)hFontResult, TRUE);
            
            break;
        }
            
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hInput) {
                char buffer[256];
                GetWindowTextA(hInput, buffer, 256);
                std::string inputStr(buffer);
                
                if (inputStr.empty()) {
                    currentStateName = L"Start\n0 Ones";
                    isAccepted = false;
                    isError = false;
                    SetWindowTextW(hResult, L"Result: Waiting for input...");
                } else {
                    bool invalid = false;
                    for (char c : inputStr) {
                        if (c != '0' && c != '1') invalid = true;
                    }
                    if (invalid) {
                        currentStateName = L"ERROR\nInvalid Char";
                        isAccepted = false;
                        isError = true;
                        SetWindowTextW(hResult, L"Result: ERROR");
                    } else {
                        DFAResult res = checkDFA(inputStr);
                        std::wstring wCurrent(res.currentStateName.begin(), res.currentStateName.end());
                        currentStateName = wCurrent;
                        isAccepted = res.accepted;
                        isError = false;
                        
                        if (res.accepted) {
                            SetWindowTextW(hResult, L"Result: ACCEPTED \x2714");
                        } else {
                            SetWindowTextW(hResult, L"Result: REJECTED \x2718");
                        }
                    }
                }
                
                InvalidateRect(hResult, NULL, TRUE);
                
                RECT circleRect = { (640-160)/2 - 5, 120 - 5, (640-160)/2 + 165, 120 + 165 };
                InvalidateRect(hwnd, &circleRect, TRUE);
            }
            break;
            
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            int cW = 160;
            int cH = 160;
            int cX = (640 - cW) / 2;
            int cY = 120;
            
            HBRUSH hCircleBrush;
            if (isError) hCircleBrush = CreateSolidBrush(RGB(150, 40, 40));
            else if (isAccepted) hCircleBrush = CreateSolidBrush(RGB(40, 150, 40));
            else hCircleBrush = CreateSolidBrush(RGB(50, 50, 70));
            
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hCircleBrush);
            HPEN hPen = CreatePen(PS_SOLID, 3, RGB(200, 200, 200));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            
            Ellipse(hdc, cX, cY, cX + cW, cY + cH);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFontCircle);
            
            // Offset a little logic for DT_VCENTER behavior 
            RECT rect = {cX + 15, cY + 30, cX + cW - 15, cY + cH - 15};
            DrawTextW(hdc, currentStateName.c_str(), -1, &rect, DT_CENTER | DT_WORDBREAK);
            
            SelectObject(hdc, hOldFont);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            
            DeleteObject(hPen);
            DeleteObject(hCircleBrush);
            
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            
            if (hwndCtrl == hInput) {
                SetTextColor(hdc, textMain);
                SetBkColor(hdc, editBg);
                return (LRESULT)hbrEditBg;
            }
            
            SetTextColor(hdc, textMain);
            SetBkColor(hdc, bgMain);
            
            if (hwndCtrl == hResult) {
                wchar_t buffer[256];
                GetWindowTextW(hResult, buffer, 256);
                std::wstring resStr(buffer);
                
                if (resStr.find(L"ACCEPTED") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(50, 205, 50)); 
                } else if (resStr.find(L"REJECTED") != std::wstring::npos || resStr.find(L"ERROR") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(255, 69, 0)); 
                } else {
                    SetTextColor(hdc, RGB(150, 150, 150)); 
                }
            }
            else if (hwndCtrl == hTitle) {
                SetTextColor(hdc, RGB(100, 200, 255)); 
            }
            
            return (LRESULT)hbrBgMain;
        }

        case WM_DESTROY:
            DeleteObject(hbrBgMain);
            DeleteObject(hbrEditBg);
            DeleteObject(hFontMain);
            DeleteObject(hFontTitle);
            DeleteObject(hFontResult);
            DeleteObject(hFontCircle);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Entry point for Windows GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"DFAGUIClass";
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassW(&wc);
    
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Real-Time DFA Validator",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, 
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) {
        return 0;
    }
    
    ShowWindow(hwnd, nCmdShow);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
