//gcc -o cl ControlAppCl.c -finput-charset=utf-8 -fexec-charset=cp932 -lws2_32 -lgdi32

#include <wchar.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>

#define BUTTON1 100
#define EDIT1 101
#define EDIT2 102
#define EDIT3 103
#define WM_SOCKET_RECV (WM_USER + 1)

HINSTANCE hInstance;
WCHAR buff[1024];
WCHAR clear[1024];
WCHAR history[1024];
WCHAR re[1024];
HWND g_hwnd = NULL;

SOCKET g_sendSock = INVALID_SOCKET;
SOCKET g_recvSock = INVALID_SOCKET;
CRITICAL_SECTION g_cs;

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
        server.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        
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
        server.sin_port = htons(62455);
        server.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        
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

int WINAPI WinMain(
    HINSTANCE hInst, HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine, int nCmdShow){

    hInstance = hInst;
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
        szAppName, "ChatAppCl (Client)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        780, 300,
        NULL, NULL,
        hInstance, NULL);

    if (!hwnd) return 0;
    
    g_hwnd = hwnd;
    InitializeCriticalSection(&g_cs);
    
    // 受信・送信スレッドを開始
    _beginthreadex(NULL, 0, ReceiveThread, NULL, 0, NULL);
    _beginthreadex(NULL, 0, SendThread, NULL, 0, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while(GetMessage(&msg,NULL,0,0)>0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    DeleteCriticalSection(&g_cs);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM IParam){
    static TCHAR szText1[] = TEXT("Send message");
    static TCHAR szText2[] = TEXT("Receive message");
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
                120,220,120,30,
                hwnd, (HMENU)BUTTON1,hInstance,NULL);
            hedt1=CreateWindow(
                "EDIT","",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
                10,30,360,100,hwnd,(HMENU)EDIT1,hInstance,NULL);
            hedt2=CreateWindow(
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                10,170,360,30,hwnd,(HMENU)EDIT2,hInstance,NULL);
            hedt3=CreateWindow(
                "EDIT","",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
                390,30,360,200,hwnd,(HMENU)EDIT3,hInstance,NULL);
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
            
        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            TextOut(hdc, 5, 5, szText1, lstrlen(szText1));
            TextOut(hdc, 385, 5, szText2, lstrlen(szText2));
            EndPaint(hwnd, &ps);
            return 0;
            
        case WM_DESTROY:
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