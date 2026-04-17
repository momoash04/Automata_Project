#include <windows.h>
#include <string>
#include <vector>
#include <cmath> // Required for arrow trigonometry
#include "DFABackend.h"

// Colors
COLORREF bgMain = RGB(30, 30, 30);
COLORREF editBg = RGB(45, 45, 45);
COLORREF textMain = RGB(220, 220, 220);

// State variables for drawing
static int currentStateIndex = 0;
static bool isAccepted = false;
static bool isError = false;
static int animFrame = 0;

static int lastTransitionFrom = -1;
static int lastTransitionTo = -1;
static wchar_t lastTransitionLabel = L'\0';
static int transitionHighlightTimer = 0;

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
                                   0, 0, 0, 0, hwnd, NULL, NULL, NULL);

            hLabel = CreateWindowW(L"STATIC", L"Input Binary String:", WS_CHILD | WS_VISIBLE, 
                                   0, 0, 0, 0, hwnd, NULL, NULL, NULL);
                          
            hInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
                                   0, 0, 0, 0, hwnd, (HMENU) 2, NULL, NULL);
                                       
            // Limit input size to prevent buffer overflow when using GetWindowTextA
            SendMessage(hInput, EM_LIMITTEXT, 255, 0);

            hResult = CreateWindowW(L"STATIC", L"Result: Waiting for input...", WS_CHILD | WS_VISIBLE | SS_CENTER, 
                                    0, 0, 0, 0, hwnd, NULL, NULL, NULL);
            
            hFontMain = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontTitle = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontResult = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontCircle = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            
            SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            SendMessage(hInput, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            SendMessage(hResult, WM_SETFONT, (WPARAM)hFontResult, TRUE);
            
            SetTimer(hwnd, 1, 100, NULL);
            break;
        }

        case WM_SIZE:
        {
            // Dynamically resize and center controls when window size changes
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            SetWindowPos(hTitle, NULL, 0, 20, width, 35, SWP_NOZORDER);
            
            // Center the input area
            int inputGroupWidth = 160 + 480;
            int startX = (width - inputGroupWidth) / 2;
            
            SetWindowPos(hLabel, NULL, startX, 75, 160, 25, SWP_NOZORDER);
            SetWindowPos(hInput, NULL, startX + 160, 70, 480, 28, SWP_NOZORDER);
            
            // Pin result to the bottom
            SetWindowPos(hResult, NULL, 0, height - 60, width, 40, SWP_NOZORDER);
            break;
        }

        case WM_ERASEBKGND:
            // Crucial for removing flicker: tell Windows we handle the background ourselves
            return 1;
            
        case WM_TIMER:
        {
            bool redraw = false;
            if (isAccepted && !isError && currentStateIndex == 7) {
                animFrame = (animFrame + 1) % 10;
                redraw = true;
            }
            if (transitionHighlightTimer > 0) {
                transitionHighlightTimer--;
                if (transitionHighlightTimer == 0) {
                    lastTransitionFrom = -1;
                    lastTransitionTo = -1;
                }
                redraw = true;
            }
            if (redraw) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hInput) {
                char buffer[256];
                GetWindowTextA(hInput, buffer, 256);
                std::string inputStr(buffer);
                
                if (inputStr.empty()) {
                    currentStateIndex = 0;
                    isAccepted = false;
                    isError = false;
                    lastTransitionFrom = -1;
                    lastTransitionTo = -1;
                    SetWindowTextW(hResult, L"Result: Waiting for input...");
                } else {
                    bool invalid = false;
                    for (char c : inputStr) {
                        if (c != '0' && c != '1') invalid = true;
                    }
                    if (invalid) {
                        currentStateIndex = -1;
                        isAccepted = false;
                        isError = true;
                        lastTransitionFrom = -1;
                        lastTransitionTo = -1;
                        SetWindowTextW(hResult, L"Result: ERROR (Invalid Char)");
                    } else {
                        // Compute one step before last to find 'from' state
                        std::string previousStr = inputStr.substr(0, inputStr.length() - 1);
                        DFAResult prevRes = checkDFA(previousStr);
                        
                        DFAResult res = checkDFA(inputStr);
                        
                        lastTransitionFrom = prevRes.finalState;
                        lastTransitionTo = res.finalState;
                        lastTransitionLabel = inputStr.back() == '0' ? L'0' : L'1';
                        transitionHighlightTimer = 10; // Highlight for 10 ticks (1 second)
                        
                        currentStateIndex = res.finalState;
                        isAccepted = res.accepted;
                        isError = false;
                        animFrame = 0;
                        
                        if (res.accepted) {
                            SetWindowTextW(hResult, L"Result: ACCEPTED \x2714");
                        } else {
                            SetWindowTextW(hResult, L"Result: REJECTED \x2718");
                        }
                    }
                }
                
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
            
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rectClient;
            GetClientRect(hwnd, &rectClient);
            
            // Double buffering to avoid flicker
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, rectClient.right, rectClient.bottom);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);
            
            FillRect(hMemDC, &rectClient, hbrBgMain);
            
            HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFontCircle);
            SetBkMode(hMemDC, TRANSPARENT);
            SetTextColor(hMemDC, RGB(255, 255, 255));
            
            int cW = 100;
            int cH = 100;
            
            // Center the graph vertically and horizontally
            int availableHeight = rectClient.bottom - 160; 
            int startX = rectClient.right / 10;
            int startY = 120 + (availableHeight - 320) / 2; 
            int spacingX = (rectClient.right - (startX * 2)) / 3; 
            int spacingY = 220;
            
            // Collect center coordinates for all states
            POINT centers[8];
            for (int i = 0; i < 8; i++) {
                int row = i / 4;
                int col = i % 4;
                centers[i].x = startX + col * spacingX;
                centers[i].y = startY + row * spacingY;
            }
            
            // Draw Edges (Transitions)
            HPEN hEdgePen = CreatePen(PS_SOLID, 2, RGB(160, 160, 160));
            HPEN hHighlightEdgePen = CreatePen(PS_SOLID, 4, RGB(255, 165, 0));
            HPEN hOldEdgePen = (HPEN)SelectObject(hMemDC, hEdgePen);
            
            // Helper lambda to draw the arrowheads
            auto drawArrow = [&](HDC hdcArrow, int tipX, int tipY, double angle, bool isHighlighted) {
                POINT arrowPts[3];
                arrowPts[0] = { tipX, tipY };
                int L = 14; // Length of the arrowhead
                double phi = 0.5; // Width angle of the arrowhead (~30 degrees)
                
                arrowPts[1] = { (int)(tipX - L * cos(angle - phi)), (int)(tipY - L * sin(angle - phi)) };
                arrowPts[2] = { (int)(tipX - L * cos(angle + phi)), (int)(tipY - L * sin(angle + phi)) };
                
                COLORREF color = isHighlighted ? RGB(255, 165, 0) : RGB(160, 160, 160);
                HBRUSH hBrush = CreateSolidBrush(color);
                HPEN hPen = CreatePen(PS_SOLID, 1, color);
                
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcArrow, hBrush);
                HPEN hOldPen = (HPEN)SelectObject(hdcArrow, hPen);
                
                Polygon(hdcArrow, arrowPts, 3);
                
                SelectObject(hdcArrow, hOldPen);
                SelectObject(hdcArrow, hOldBrush);
                DeleteObject(hPen);
                DeleteObject(hBrush);
            };

            auto drawEdgeLine = [&](int from, int to, const wchar_t* label, int curveOffset) {
                bool isHighlighted = (transitionHighlightTimer > 0 && lastTransitionFrom == from && lastTransitionTo == to && label[0] == lastTransitionLabel);
                
                if (isHighlighted) {
                    SelectObject(hMemDC, hHighlightEdgePen);
                } else {
                    SelectObject(hMemDC, hEdgePen);
                }

                if (from == to) {
                    // Draw Self Loop
                    Arc(hMemDC, centers[from].x - 20, centers[from].y - 60, centers[from].x + 20, centers[from].y - 20, 
                        centers[from].x, centers[from].y - 40, centers[from].x, centers[from].y - 40);
                    
                    if (isHighlighted) SetTextColor(hMemDC, RGB(255, 165, 0));
                    TextOutW(hMemDC, centers[from].x - 5, centers[from].y - 75, label, lstrlenW(label));
                    if (isHighlighted) SetTextColor(hMemDC, RGB(255, 255, 255));

                    // Arrow for self-loop
                    int tipX = centers[from].x + 12;
                    int tipY = centers[from].y - 48; 
                    double angle = 1.7; 
                    drawArrow(hMemDC, tipX, tipY, angle, isHighlighted);
                    
                    return;
                }
                
                int x1 = centers[from].x;
                int y1 = centers[from].y;
                int x2 = centers[to].x;
                int y2 = centers[to].y;
                
                POINT pts[4];
                pts[0] = {x1, y1};
                
                int mx = (x1 + x2) / 2;
                int my = (y1 + y2) / 2;
                
                int dx = x2 - x1;
                int dy = y2 - y1;
                
                pts[1] = {mx - dy * curveOffset / 100, my + dx * curveOffset / 100};
                pts[2] = {mx - dy * curveOffset / 100, my + dx * curveOffset / 100};
                pts[3] = {x2, y2};
                
                PolyBezier(hMemDC, pts, 4);
                
                if (isHighlighted) SetTextColor(hMemDC, RGB(255, 165, 0));
                TextOutW(hMemDC, pts[1].x - 5, pts[1].y - 10, label, lstrlenW(label));
                if (isHighlighted) SetTextColor(hMemDC, RGB(255, 255, 255));

                // Calculate angle and draw arrow
                double angle = atan2(pts[3].y - pts[2].y, pts[3].x - pts[2].x);
                int radius = 50;
                int tipX = pts[3].x - (int)(radius * cos(angle));
                int tipY = pts[3].y - (int)(radius * sin(angle));
                
                drawArrow(hMemDC, tipX, tipY, angle, isHighlighted);
            };

            // Edges
            drawEdgeLine(0, 1, L"0", 20);
            drawEdgeLine(0, 2, L"1", -20);
            drawEdgeLine(1, 1, L"0", 0);
            drawEdgeLine(1, 2, L"1", 20);
            drawEdgeLine(2, 3, L"0", 20);
            drawEdgeLine(2, 4, L"1", -20);
            drawEdgeLine(3, 3, L"0", 0);
            drawEdgeLine(3, 4, L"1", 20);
            drawEdgeLine(4, 5, L"0", 20);
            drawEdgeLine(4, 6, L"1", -20);
            drawEdgeLine(5, 5, L"0", 0);
            drawEdgeLine(5, 6, L"1", 20);
            drawEdgeLine(6, 7, L"0", 20);
            drawEdgeLine(6, 2, L"1", -20);
            drawEdgeLine(7, 7, L"0", 0);
            drawEdgeLine(7, 2, L"1", 20);

            SelectObject(hMemDC, hOldEdgePen);
            DeleteObject(hEdgePen);
            DeleteObject(hHighlightEdgePen);
            
            for (int i = 0; i < 8; i++) {
                int cX = centers[i].x - cW / 2;
                int cY = centers[i].y - cH / 2;
                
                COLORREF stateColor;
                bool isAnim = false;
                
                if (i == 7) { 
                    stateColor = RGB(40, 150, 40); 
                    if (currentStateIndex == 7 && isAccepted && !isError) {
                        if (animFrame % 2 == 0) {
                            stateColor = RGB(80, 255, 80);
                            isAnim = true;
                        }
                    }
                } else if (i == currentStateIndex && !isError) {
                    stateColor = RGB(220, 200, 0); 
                } else {
                    stateColor = RGB(150, 40, 40); 
                }
                
                if (i == currentStateIndex && !isError && i != 7) {
                    SetTextColor(hMemDC, RGB(0, 0, 0));
                } else {
                    SetTextColor(hMemDC, RGB(255, 255, 255));
                }
                
                int drawX = isAnim ? cX - 5 : cX;
                int drawY = isAnim ? cY - 5 : cY;
                int drawW = isAnim ? cW + 10 : cW;
                int drawH = isAnim ? cH + 10 : cH;
                
                HBRUSH hCircleBrush = CreateSolidBrush(stateColor);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hCircleBrush);
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
                HPEN hOldPen = (HPEN)SelectObject(hMemDC, hPen);
                
                Ellipse(hMemDC, drawX, drawY, drawX + drawW, drawY + drawH);
                
                std::string sName = getStateName(i);
                std::wstring wsName(sName.begin(), sName.end());
                
                RECT textRect = {drawX + 5, drawY, drawX + drawW - 5, drawY + drawH};
                DrawTextW(hMemDC, wsName.c_str(), -1, &textRect, DT_CENTER | DT_WORDBREAK | DT_VCENTER | DT_SINGLELINE);
                
                SelectObject(hMemDC, hOldPen);
                SelectObject(hMemDC, hOldBrush);
                DeleteObject(hPen);
                DeleteObject(hCircleBrush);
            }
            
            SelectObject(hMemDC, hOldFont);
            
            // Blit the ENTIRE surface. WS_CLIPCHILDREN prevents controls from being drawn over.
            BitBlt(hdc, 0, 0, rectClient.right, rectClient.bottom, hMemDC, 0, 0, SRCCOPY);
            
            // Cleanup GDI objects
            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hMemBmp);
            DeleteDC(hMemDC);
            
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
            
            SetBkMode(hdc, TRANSPARENT); // Prevents ugly background boxes on static text
            
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
            } else {
                SetTextColor(hdc, textMain);
            }
            
            return (LRESULT)hbrBgMain;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"DFAGUIClass";
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassW(&wc);
    
    // WS_CLIPCHILDREN prevents the main window from painting over child controls
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Real-Time DFA Validator",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE, 
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 800,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) {
        return 0;
    }
    
    ShowWindow(hwnd, SW_MAXIMIZE);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}