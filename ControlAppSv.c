//gcc -o sv ControlAppSv.c -finput-charset=utf-8 -fexec-charset=cp932 -lws2_32 -lgdi32

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

// 受信スレッド
unsigned __stdcall ReceiveThread(void* param) {
    WSADATA wsaData;
    SOCKET sock0, sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    char recvbuf[1024];
    
    WSAStartup(MAKEWORD(2,0), &wsaData);
    
    sock0 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock0 == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }
    
    // SO_REUSEADDRを設定
    int reuse = 1;
    setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(62455);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    
    if (bind(sock0, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock0);
        WSACleanup();
        return 1;
    }
    
    listen(sock0, 1);
    
    while (1) {
        len = sizeof(client);
        sock = accept(sock0, (struct sockaddr *)&client, &len);
        
        if (sock == INVALID_SOCKET) break;
        
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
    }
    
    closesocket(sock0);
    WSACleanup();
    return 0;
}

// 送信スレッド（接続待機用）
unsigned __stdcall SendThread(void* param) {
    WSADATA wsaData;
    SOCKET sock0, sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    
    WSAStartup(MAKEWORD(2,0), &wsaData);
    
    sock0 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock0 == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }
    
    int reuse = 1;
    setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    
    if (bind(sock0, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock0);
        WSACleanup();
        return 1;
    }
    
    listen(sock0, 1);
    
    while (1) {
        len = sizeof(client);
        sock = accept(sock0, (struct sockaddr *)&client, &len);
        
        if (sock == INVALID_SOCKET) break;
        
        EnterCriticalSection(&g_cs);
        if (g_sendSock != INVALID_SOCKET) {
            closesocket(g_sendSock);
        }
        g_sendSock = sock;
        LeaveCriticalSection(&g_cs);
    }
    
    closesocket(sock0);
    WSACleanup();
    return 0;
}

void SendMessageToClient(WCHAR* message) {
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
    TCHAR szAppName[] = TEXT("ChatAppSv");
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
        szAppName, "ChatAppSv (Server)",
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
                    swprintf(history,1024,L"%ls\r\nServer: %ls",history,buff);
                    SetWindowTextW(hedt1,history);
                }else{
                    swprintf(history,1024,L"Server: %ls",buff);
                    SetWindowTextW(hedt1,history);
                }
                SetWindowTextW(hedt2,clear);
                SendMessage(hedt1, WM_VSCROLL, SB_BOTTOM, 0);

                SendMessageToClient(buff);
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
                    swprintf(history,1024,L"%ls\r\nClient: %ls",history,temp);
                    SetWindowTextW(hedt3,history);
                }else{
                    swprintf(history,1024,L"Client: %ls",temp);
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