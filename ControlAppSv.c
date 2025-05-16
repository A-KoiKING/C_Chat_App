//gcc -o sv ControlAppSv.c -finput-charset=utf-8 -fexec-charset=cp932 -lws2_32 -lgdi32

#include <wchar.h>
#include <winsock2.h>
#include <windows.h>
#define BUTTON1 100
#define EDIT1 101
#define EDIT2 102
#define EDIT3 103

HINSTANCE hInstance;
WCHAR buff[1024];
WCHAR clear[1024];
WCHAR history[1024];
WCHAR re[1024];

LRESULT CALLBACK WndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM IParam);

int WINAPI WinMain(
    HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine, int nCmdShow){

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
        szAppName, "ChatAppSv",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        780, 300,
        NULL, NULL,
        hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while(GetMessage(&msg,NULL,0,0)>0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void SendMessageToServer(WCHAR* message) {
    WSADATA wsaData;
    SOCKET sock0,sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    char sendbuf[1024];

    // WCHAR を char に変換
    WideCharToMultiByte(CP_ACP, 0, message, -1, sendbuf, sizeof(sendbuf), NULL, NULL);

    // winsock2の初期化
    WSAStartup(MAKEWORD(2,0), &wsaData);

    // ソケットの作成
    sock0 = socket(AF_INET, SOCK_STREAM, 0);

    // ソケットの設定
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);   //port番号12345で接続待ち
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    bind(sock0, (struct sockaddr *)&addr, sizeof(addr));

    // TCPクライアントからの接続要求を待てる状態にする
    listen(sock0, 1);
    len = sizeof(client);
    // TCPクライアントからの接続要求を受け付ける
    sock = accept(sock0, (struct sockaddr *)&client, &len);

    send(sock, sendbuf, sizeof(sendbuf), 0);

    // TCPセッションの終了
    closesocket(sock0);

    // winsock2の終了処理
    WSACleanup();
}

void ReceiveMessageFromServer(){
    WSADATA wsaData;
    SOCKET sock0,sock;
    struct sockaddr_in addr;
    struct sockaddr_in client;
    int len;
    char recvbuf[1024];

    // winsock2の初期化
    WSAStartup(MAKEWORD(2,0), &wsaData);

    // ソケットの作成
    sock0 = socket(AF_INET, SOCK_STREAM, 0);

    // ソケットの設定
    addr.sin_family = AF_INET;
    addr.sin_port = htons(62455);   //port番号62455で接続待ち
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    bind(sock0, (struct sockaddr *)&addr, sizeof(addr));

    // TCPクライアントからの接続要求を待てる状態にする
    listen(sock0, 1);
    len = sizeof(client);
    // TCPクライアントからの接続要求を受け付ける
    sock = accept(sock0, (struct sockaddr *)&client, &len);

    recv(sock, recvbuf, sizeof(recvbuf), 0);

    // 受信したデータを WCHAR に変換
    MultiByteToWideChar(CP_ACP, 0, recvbuf, -1, re, sizeof(re) / sizeof(WCHAR));

    // TCPセッションの終了
    closesocket(sock0);

    // winsock2の終了処理
    WSACleanup();
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
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL,
                10,30,360,100,hwnd,(HMENU)EDIT1,hInstance,NULL);
            hedt2=CreateWindow(
                "EDIT", "Hi!",
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                10,170,360,30,hwnd,(HMENU)EDIT2,hInstance,NULL);
            hedt3=CreateWindow(
                "EDIT","",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL,
                390,30,360,200,hwnd,(HMENU)EDIT3,hInstance,NULL);
            break;
        case WM_COMMAND:
            WCHAR clearcheck[1024];
            GetWindowTextW(hedt2,clearcheck,1024);
            if(wcslen(clearcheck) <= 0){
                break;
            }
            if(LOWORD(wParam)==BUTTON1){
                GetWindowTextW(hedt1,history,1024);
                GetWindowTextW(hedt2,buff,1024);
                if(wcslen(history) > 0){
                    swprintf(history,1024,L"%ls\r\n%ls",history,buff);
                    SetWindowTextW(hedt1,history);
                }else{
                    SetWindowTextW(hedt1,buff);
                }
                SetWindowTextW(hedt2,clear);
                SendMessage(hedt1, WM_VSCROLL, SB_BOTTOM, 0);

                SendMessageToServer(buff);
            }
            break;
        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            TextOut(hdc, 5, 5, szText1, lstrlen(szText1));
            TextOut(hdc, 385, 5, szText2, lstrlen(szText2));
            EndPaint(hwnd, &ps);
            return 0;
        case WM_LBUTTONUP:
            return 0;

        case WM_RBUTTONUP:
            return 0;
            
        default:
            ReceiveMessageFromServer();
            SetWindowTextW(hedt3,re);
            return DefWindowProc(hwnd,uMsg,wParam,IParam);
    }
}
