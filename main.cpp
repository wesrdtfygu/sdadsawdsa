#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <fstream>
#include <deque>
#include <chrono>
#include <algorithm>

struct Config {
    bool lurch=true, socd=true, turboLoot=false, turboJump=false, superglide=false;
    int lurchDelay=15, lurchHold=10, lootDelay=40, jumpDelay=40;
    double fps=60.0;
} cfg;

struct AppState {
    bool a=false,d=false,w=false,s=false,space=false,ctrl=false,e=false;
    bool simA=false,simD=false;
    bool lastWasA=false;
    unsigned long long lurchCycles=0, turboLootTicks=0, turboJumpTicks=0, superglides=0;
    int tab=0;
    std::deque<std::wstring> log;
    std::chrono::steady_clock::time_point lastLurch{}, lastLoot{}, lastJump{}, lastSG{};
} st;

static HWND gWnd{};
static const wchar_t* kCfg=L"strafehelper_safe.cfg";

static void Log(const std::wstring& s) {
    SYSTEMTIME t{}; GetLocalTime(&t);
    wchar_t b[512]; swprintf_s(b,L"[%02d:%02d:%02d] %s",t.wHour,t.wMinute,t.wSecond,s.c_str());
    st.log.push_back(b); while(st.log.size()>18) st.log.pop_front();
}
static void Save() {
    std::ofstream f(kCfg);
    f<<"enable_lurch="<<cfg.lurch<<"\n"<<"enable_socd="<<cfg.socd<<"\n"
     <<"enable_turbo_loot="<<cfg.turboLoot<<"\n"<<"enable_turbo_jump="<<cfg.turboJump<<"\n"
     <<"enable_superglide="<<cfg.superglide<<"\n"<<"lurch_delay="<<cfg.lurchDelay<<"\n"
     <<"lurch_hold="<<cfg.lurchHold<<"\n"<<"loot_delay="<<cfg.lootDelay<<"\n"
     <<"jump_delay="<<cfg.jumpDelay<<"\n"<<"target_fps="<<cfg.fps<<"\n";
    Log(L"Config saved.");
}
static void Load() {
    std::ifstream f(kCfg); std::string line;
    while(std::getline(f,line)) {
        auto p=line.find('='); if(p==std::string::npos) continue;
        auto k=line.substr(0,p),v=line.substr(p+1); int n=atoi(v.c_str());
        if(k=="enable_lurch")cfg.lurch=n; else if(k=="enable_socd")cfg.socd=n;
        else if(k=="enable_turbo_loot")cfg.turboLoot=n; else if(k=="enable_turbo_jump")cfg.turboJump=n;
        else if(k=="enable_superglide")cfg.superglide=n; else if(k=="lurch_delay")cfg.lurchDelay=std::max(1,n);
        else if(k=="lurch_hold")cfg.lurchHold=std::max(1,n); else if(k=="loot_delay")cfg.lootDelay=std::max(1,n);
        else if(k=="jump_delay")cfg.jumpDelay=std::max(1,n); else if(k=="target_fps")cfg.fps=std::max(1.0,atof(v.c_str()));
    }
}
static bool Down(int vk){ return (GetAsyncKeyState(vk)&0x8000)!=0; }
static void Tick() {
    bool oa=st.a,od=st.d,os=st.space,oe=st.e;
    st.a=Down('A'); st.d=Down('D'); st.w=Down('W'); st.s=Down('S');
    st.space=Down(VK_SPACE); st.ctrl=Down(VK_LCONTROL); st.e=Down('E');
    if(st.a&&!oa) st.lastWasA=true; if(st.d&&!od) st.lastWasA=false;

    auto now=std::chrono::steady_clock::now();
    if(cfg.socd && st.a && st.d) { st.simA=st.lastWasA; st.simD=!st.lastWasA; }
    else { st.simA=st.a; st.simD=st.d; }

    if(cfg.lurch && (st.a||st.d) && st.space) {
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-st.lastLurch).count();
        if(ms>=cfg.lurchDelay) { st.simA=!st.simA; st.simD=!st.simA; st.lurchCycles++; st.lastLurch=now; }
    }
    if(cfg.turboLoot && st.e) {
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-st.lastLoot).count();
        if(ms>=cfg.lootDelay){st.turboLootTicks++;st.lastLoot=now;}
    }
    if(cfg.turboJump && st.space) {
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-st.lastJump).count();
        if(ms>=cfg.jumpDelay){st.turboJumpTicks++;st.lastJump=now;}
    }
    if(cfg.superglide && st.space && !os && st.ctrl) {
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-st.lastSG).count();
        if(ms>=500){st.superglides++;st.lastSG=now;Log(L"Superglide timing package simulated.");}
    }
}
static void Txt(HDC d,int x,int y,const std::wstring&s){TextOutW(d,x,y,s.c_str(),(int)s.size());}
static void Box(HDC d,int x,int y,int w,int h,const wchar_t* t,bool on){
    Rectangle(d,x,y,x+w,y+h); std::wstring s=on?L"[x] ":L"[ ] ";s+=t;Txt(d,x+10,y+9,s);
}
static void DrawConfig(HDC d){
    Txt(d,15,68,L"Features");
    MoveToEx(d,15,87,nullptr);LineTo(d,405,87);
    Box(d,15,92,17,17,L"",cfg.lurch); Txt(d,38,96,L"Enable Lurch Strafing");
    Box(d,15,115,17,17,L"",cfg.socd); Txt(d,38,119,L"Enable SnapTap (SOCD)");

    Txt(d,15,145,L"Turbo Functions");
    MoveToEx(d,15,163,nullptr);LineTo(d,405,163);
    Box(d,15,169,17,17,L"",cfg.turboLoot); Txt(d,38,173,L"Enable Turbo Loot");
    Box(d,15,192,17,17,L"",cfg.turboJump); Txt(d,38,196,L"Enable Turbo Jump");

    Txt(d,15,222,L"Superglide");
    MoveToEx(d,15,240,nullptr);LineTo(d,405,240);
    Box(d,15,246,17,17,L"",cfg.superglide); Txt(d,38,250,L"Enable Superglide");

    Txt(d,15,278,L"Input Backend");
    MoveToEx(d,15,295,nullptr);LineTo(d,405,295);
    SetTextColor(d,RGB(65,255,75)); Txt(d,15,305,L"[ OK ]  Safe simulator ready"); SetTextColor(d,RGB(215,215,220));
    Ellipse(d,18,331,37,350); Txt(d,42,334,L"WinHook (safe monitor)");
    Ellipse(d,160,331,179,350); Txt(d,184,334,L"Interception (disabled)");
}
static std::wstring Yn(bool b){return b?L"DOWN":L"up";}
static void DrawMonitor(HDC d){
    Txt(d,30,95,L"Physical input"); Txt(d,30,125,L"W: "+Yn(st.w)+L"   A: "+Yn(st.a)+L"   S: "+Yn(st.s)+L"   D: "+Yn(st.d));
    Txt(d,30,155,L"Space: "+Yn(st.space)+L"   Ctrl: "+Yn(st.ctrl)+L"   E: "+Yn(st.e));
    Txt(d,30,205,L"Simulated output state (display only)");
    Txt(d,30,235,L"A: "+Yn(st.simA)+L"   D: "+Yn(st.simD));
    std::wostringstream x;x<<L"Lurch cycles: "<<st.lurchCycles<<L"     Turbo loot ticks: "<<st.turboLootTicks
      <<L"     Turbo jump ticks: "<<st.turboJumpTicks<<L"     Superglides: "<<st.superglides;Txt(d,30,285,x.str());
    Txt(d,30,330,L"Hold movement keys to inspect how enabled logic would resolve them.");
}
static void DrawConsole(HDC d){int y=95;for(auto&s:st.log){Txt(d,30,y,s);y+=22;}}
static LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE: SetTimer(h,1,8,nullptr);Log(L"StrafeHelper Safe started.");return 0;
    case WM_TIMER: Tick();InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:{
        int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
        if(y>=15&&y<=52){if(x<240&&x>=160)st.tab=0;else if(x<330&&x>=240)st.tab=1;else if(x>=330)st.tab=2;InvalidateRect(h,nullptr,TRUE);return 0;}
        if(st.tab==0){
            if(x>=15&&x<=310){
                if(y>=92&&y<=109)cfg.lurch=!cfg.lurch;
                else if(y>=115&&y<=132)cfg.socd=!cfg.socd;
                else if(y>=169&&y<=186)cfg.turboLoot=!cfg.turboLoot;
                else if(y>=192&&y<=209)cfg.turboJump=!cfg.turboJump;
                else if(y>=246&&y<=263)cfg.superglide=!cfg.superglide;
            }
            InvalidateRect(h,nullptr,TRUE);
        } return 0;}
    case WM_PAINT:{PAINTSTRUCT p{};HDC d=BeginPaint(h,&p);SetBkMode(d,TRANSPARENT);
        HFONT font=CreateFontW(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(d,font);
        SetTextColor(d,RGB(215,215,220));
        Txt(d,50,20,L"StrafeHelper");
        Ellipse(d,24,18,42,36); MoveToEx(d,24,31,nullptr);LineTo(d,42,31);
        Txt(d,180,29,L"Config"); SetTextColor(d,RGB(125,125,130));Txt(d,260,29,L"Monitor");Txt(d,340,29,L"Console");
        SetTextColor(d,RGB(215,215,220));
        if(st.tab==0)DrawConfig(d);else if(st.tab==1)DrawMonitor(d);else DrawConsole(d);
        SelectObject(d,old);DeleteObject(font);EndPaint(h,&p);return 0;}
    case WM_DESTROY:Save();PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE i,HINSTANCE,PWSTR,int n){
    Load();WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=i;c.lpszClassName=L"StrafeHelperSafe";
    c.hCursor=LoadCursor(nullptr,IDC_ARROW);c.hbrBackground=CreateSolidBrush(RGB(20,20,25));RegisterClassW(&c);
    gWnd=CreateWindowExW(0,c.lpszClassName,L"StrafeHelper",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,435,420,nullptr,nullptr,i,nullptr);if(!gWnd)return 1;ShowWindow(gWnd,n);
    MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return(int)m.wParam;
}