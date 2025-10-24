//gcc -o cl ControlAppCl.c -finput-charset=utf-8 -mwindows -fexec-charset=cp932 -lws2_32 -lgdi32 -lgdiplus

#include <wchar.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <gdiplus.h>

#define BUTTON1 100
#define EDIT1 101
#define EDIT2 102
#define EDIT3 103
#define WM_SOCKET_RECV (WM_USER + 1)
#define WM_ANIMATE_GIF (WM_USER + 2)

HINSTANCE hInstance;
WCHAR buff[1024];
WCHAR clear[1024];
WCHAR history[1024];
WCHAR re[1024];
HWND g_hwnd = NULL;

SOCKET g_sendSock = INVALID_SOCKET;
SOCKET g_recvSock = INVALID_SOCKET;
CRITICAL_SECTION g_cs;

// GIF関連
GpImage* g_pImage = NULL;
UINT g_frameCount = 0;
UINT g_currentFrame = 0;
PropertyItem* g_pPropertyItem = NULL;
ULONG_PTR g_gdiplusToken;

LRESULT CALLBACK WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM IParam);

// サーバーに接続して受信を待機するスレッド
unsigned __stdcall ReceiveThread(void* param) {
    WSADATA wsaData;
    struct sockaddr_in server;
    SOCKET sock;
    char recvbuf[1024];
    
    Sleep(1000); // サーバー起動を待つ
    
    WSAStartup(MAKEWORD(2,0), &wsaData);
    
    while (1) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            Sleep(1000);
            continue;
        }
        
        server.sin_family = AF_INET;
        server.sin_port = htons(12345);
        server.sin_addr.S_un.S_addr = inet_addr("10.181.20.28");
        
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
            closesocket(sock);
            Sleep(1000);
            continue;
        }
        
        EnterCriticalSection(&g_cs);
        g_recvSock = sock;
        LeaveCriticalSection(&g_cs);
        
        while (1) {
            memset(recvbuf, 0, sizeof(recvbuf));
            int result = recv(sock, recvbuf, sizeof(recvbuf), 0);
            
            if (result <= 0) break;
            
            EnterCriticalSection(&g_cs);
            MultiByteToWideChar(CP_ACP, 0, recvbuf, -1, re, sizeof(re) / sizeof(WCHAR));
            LeaveCriticalSection(&g_cs);
            
            if (g_hwnd) {
                PostMessage(g_hwnd, WM_SOCKET_RECV, 0, 0);
            }
        }
        
        EnterCriticalSection(&g_cs);
        closesocket(sock);
        g_recvSock = INVALID_SOCKET;
        LeaveCriticalSection(&g_cs);
        
        Sleep(1000);
    }
    
    WSACleanup();
    return 0;
}

// サーバーに接続して送信可能にするスレッド
unsigned __stdcall SendThread(void* param) {
    WSADATA wsaData;
    struct sockaddr_in server;
    SOCKET sock;
    
    Sleep(1500); // サーバー起動を待つ
    
    WSAStartup(MAKEWORD(2,0), &wsaData);
    
    while (1) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            Sleep(1000);
            continue;
        }
        
        server.sin_family = AF_INET;
        server.sin_port = htons(12346);
        server.sin_addr.S_un.S_addr = inet_addr("10.181.20.28");
        
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
            closesocket(sock);
            Sleep(1000);
            continue;
        }
        
        EnterCriticalSection(&g_cs);
        if (g_sendSock != INVALID_SOCKET) {
            closesocket(g_sendSock);
        }
        g_sendSock = sock;
        LeaveCriticalSection(&g_cs);
        
        // 接続維持（切断されるまで待機）
        char dummy[1];
        while (recv(sock, dummy, 1, 0) > 0);
        
        EnterCriticalSection(&g_cs);
        closesocket(sock);
        g_sendSock = INVALID_SOCKET;
        LeaveCriticalSection(&g_cs);
        
        Sleep(1000);
    }
    
    WSACleanup();
    return 0;
}

void SendMessageToServer(WCHAR* message) {
    char sendbuf[1024];
    
    WideCharToMultiByte(CP_ACP, 0, message, -1, sendbuf, sizeof(sendbuf), NULL, NULL);
    
    EnterCriticalSection(&g_cs);
    if (g_sendSock != INVALID_SOCKET) {
        send(g_sendSock, sendbuf, strlen(sendbuf) + 1, 0);
    }
    LeaveCriticalSection(&g_cs);
}

// GIF読み込み
BOOL LoadGif(const WCHAR* filename) {
    GpStatus status;
    
    status = GdipLoadImageFromFile(filename, &g_pImage);
    if (status != Ok || g_pImage == NULL) {
        return FALSE;
    }
    
    // フレーム数取得
    UINT count;
    GUID* pDimensionIDs;
    
    GdipImageGetFrameDimensionsCount(g_pImage, &count);
    pDimensionIDs = (GUID*)malloc(sizeof(GUID) * count);
    GdipImageGetFrameDimensionsList(g_pImage, pDimensionIDs, count);
    GdipImageGetFrameCount(g_pImage, &pDimensionIDs[0], &g_frameCount);
    free(pDimensionIDs);
    
    // フレーム遅延時間取得
    UINT size;
    GdipGetPropertyItemSize(g_pImage, PropertyTagFrameDelay, &size);
    g_pPropertyItem = (PropertyItem*)malloc(size);
    GdipGetPropertyItem(g_pImage, PropertyTagFrameDelay, size, g_pPropertyItem);
    
    g_currentFrame = 0;
    return TRUE;
}

// GIFアニメーション更新
void UpdateGifFrame(HWND hwnd) {
    if (g_pImage && g_frameCount > 1) {
        g_currentFrame = (g_currentFrame + 1) % g_frameCount;
        
        GUID pageGuid = FrameDimensionTime;
        GdipImageSelectActiveFrame(g_pImage, &pageGuid, g_currentFrame);
        
        // 次のフレームの遅延時間を取得（10ms単位）
        long* pDelay = (long*)g_pPropertyItem->value;
        int delay = pDelay[g_currentFrame] * 10;
        if (delay < 10) delay = 100; // 最小遅延
        
        SetTimer(hwnd, 1, delay, NULL);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

int WINAPI WinMain(
    HINSTANCE hInst, HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine, int nCmdShow){

    hInstance = hInst;
    
    // GDI+初期化
    GdiplusStartupInput gdiplusStartupInput;
    gdiplusStartupInput.GdiplusVersion = 1;
    gdiplusStartupInput.DebugEventCallback = NULL;
    gdiplusStartupInput.SuppressBackgroundThread = FALSE;
    gdiplusStartupInput.SuppressExternalCodecs = FALSE;
    
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    
    TCHAR szAppName[] = TEXT("ChatAppCl");
    WNDCLASS wc;
    HWND hwnd;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szAppName;
    MSG msg;

    if (!RegisterClass(&wc)) return 0;

    hwnd = CreateWindow(
        szAppName, "ChatAppClient",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1500, 800,
        NULL, NULL,
        hInstance, NULL);

    if (!hwnd) return 0;
    
    g_hwnd = hwnd;
    InitializeCriticalSection(&g_cs);
    
    // GIF読み込み
    if (LoadGif(L"cat.gif")) {
        if (g_frameCount > 1) {
            SetTimer(hwnd, 1, 100, NULL);
        }
    }
    
    // 受信・送信スレッドを開始
    _beginthreadex(NULL, 0, ReceiveThread, NULL, 0, NULL);
    _beginthreadex(NULL, 0, SendThread, NULL, 0, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while(GetMessage(&msg,NULL,0,0)>0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // クリーンアップ
    if (g_pPropertyItem) free(g_pPropertyItem);
    if (g_pImage) GdipDisposeImage(g_pImage);
    DeleteCriticalSection(&g_cs);
    GdiplusShutdown(g_gdiplusToken);
    
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM IParam){
    static TCHAR szText1[] = TEXT("送ったメッセージ");
    static TCHAR szText2[] = TEXT("受け取ったメッセージ");
    static TCHAR szText3[] = TEXT("送るメッセージ");
    static TCHAR szText4[] = TEXT("C言語チャットアプリで遊ぼう");
    static TCHAR szText5[] = TEXT("2つのパソコンでメッセージをやり取りできるよ!");
    static TCHAR szText6[] = TEXT("いろんなメッセージを送ってみよう!");
    static HWND hBtn1;
    static HWND hedt1;
    static HWND hedt2;
    static HWND hedt3;
    HDC hdc;
    PAINTSTRUCT ps;
    
    switch (uMsg){
        case WM_CREATE:
            hBtn1=CreateWindow(
                "BUTTON","送信",
                WS_CHILD | WS_VISIBLE,
                1190,80,120,30,
                hwnd, (HMENU)BUTTON1,hInstance,NULL);
            hedt1=CreateWindow(
                "EDIT","",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
                10,30,500,700,hwnd,(HMENU)EDIT1,hInstance,NULL);
            hedt2=CreateWindow(
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                1050,30,400,30,hwnd,(HMENU)EDIT2,hInstance,NULL);
            hedt3=CreateWindow(
                "EDIT","",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
                530,30,500,700,hwnd,(HMENU)EDIT3,hInstance,NULL);
            break;
            
        case WM_COMMAND:
            if(LOWORD(wParam)==BUTTON1){
                WCHAR clearcheck[1024];
                GetWindowTextW(hedt2,clearcheck,1024);
                if(wcslen(clearcheck) <= 0){
                    break;
                }
                
                GetWindowTextW(hedt1,history,1024);
                GetWindowTextW(hedt2,buff,1024);
                
                if(wcslen(history) > 0){
                    swprintf(history,1024,L"%ls\r\nClient: %ls",history,buff);
                    SetWindowTextW(hedt1,history);
                }else{
                    swprintf(history,1024,L"Client: %ls",buff);
                    SetWindowTextW(hedt1,history);
                }
                SetWindowTextW(hedt2,clear);
                SendMessage(hedt1, WM_VSCROLL, SB_BOTTOM, 0);

                SendMessageToServer(buff);
            }
            break;
            
        case WM_SOCKET_RECV:
            {
                EnterCriticalSection(&g_cs);
                WCHAR temp[1024];
                wcscpy(temp, re);
                LeaveCriticalSection(&g_cs);
                
                GetWindowTextW(hedt3,history,1024);
                if(wcslen(history) > 0){
                    swprintf(history,1024,L"%ls\r\nServer: %ls",history,temp);
                    SetWindowTextW(hedt3,history);
                }else{
                    swprintf(history,1024,L"Server: %ls",temp);
                    SetWindowTextW(hedt3,history);
                }
                SendMessage(hedt3, WM_VSCROLL, SB_BOTTOM, 0);
            }
            break;
        
        case WM_TIMER:
            if (wParam == 1) {
                UpdateGifFrame(hwnd);
            }
            break;
            
        case WM_PAINT:
            {
                hdc = BeginPaint(hwnd, &ps);
                TextOut(hdc, 8, 5, szText1, lstrlen(szText1));
                TextOut(hdc, 528, 5, szText2, lstrlen(szText2));
                TextOut(hdc, 1048, 5, szText3, lstrlen(szText3));
                TextOut(hdc, 1050, 170, szText4, lstrlen(szText4));
                TextOut(hdc, 1050, 200, szText5, lstrlen(szText5));
                TextOut(hdc, 1050, 220, szText6, lstrlen(szText6));
                
                // GIF描画
                if (g_pImage) {
                    GpGraphics* graphics;
                    GdipCreateFromHDC(hdc, &graphics);
                    GdipDrawImageRect(graphics, g_pImage, 1050, 350, 450, 306);
                    GdipDeleteGraphics(graphics);
                }
                
                EndPaint(hwnd, &ps);
            }
            return 0;
            
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            EnterCriticalSection(&g_cs);
            if (g_sendSock != INVALID_SOCKET) closesocket(g_sendSock);
            if (g_recvSock != INVALID_SOCKET) closesocket(g_recvSock);
            LeaveCriticalSection(&g_cs);
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProc(hwnd,uMsg,wParam,IParam);
    }
    return 0;
}
