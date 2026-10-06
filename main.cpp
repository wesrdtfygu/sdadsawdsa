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
    Txt(d,30,90,L"Features");
    Box(d,30,120,260,38,L"Lurch Strafing",cfg.lurch);
    Box(d,30,165,260,38,L"SnapTap / SOCD",cfg.socd);
    Box(d,30,210,260,38,L"Turbo Loot",cfg.turboLoot);
    Box(d,30,255,260,38,L"Turbo Jump",cfg.turboJump);
    Box(d,30,300,260,38,L"Superglide",cfg.superglide);
    Txt(d,330,90,L"Timing");
    std::wostringstream a;a<<L"Lurch delay: "<<cfg.lurchDelay<<L" ms   [-] [+]";Txt(d,330,125,a.str());
    std::wostringstream b;b<<L"Lurch hold:  "<<cfg.lurchHold<<L" ms   [-] [+]";Txt(d,330,165,b.str());
    std::wostringstream c;c<<L"Loot repeat: "<<cfg.lootDelay<<L" ms";Txt(d,330,205,c.str());
    std::wostringstream e;e<<L"Jump repeat: "<<cfg.jumpDelay<<L" ms";Txt(d,330,245,e.str());
    std::wostringstream q;q<<L"Target FPS: "<<cfg.fps;Txt(d,330,285,q.str());
    Rectangle(d,330,320,465,355);Txt(d,350,330,L"Save Config");
    Txt(d,30,380,L"Safe backend: physical-key monitor + local simulation only.");
    Txt(d,30,405,L"No SendInput, key suppression, Interception driver, or game injection.");
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
        if(y>=48&&y<=78){if(x<180)st.tab=0;else if(x<360)st.tab=1;else st.tab=2;InvalidateRect(h,nullptr,TRUE);return 0;}
        if(st.tab==0){
            if(x>=30&&x<=290){if(y>=120&&y<=158)cfg.lurch=!cfg.lurch;else if(y>=165&&y<=203)cfg.socd=!cfg.socd;
            else if(y>=210&&y<=248)cfg.turboLoot=!cfg.turboLoot;else if(y>=255&&y<=293)cfg.turboJump=!cfg.turboJump;
            else if(y>=300&&y<=338)cfg.superglide=!cfg.superglide;}
            if(x>=330&&x<=465&&y>=320&&y<=355)Save();
            if(y>=112&&y<=145){if(x>=510&&x<550)cfg.lurchDelay=std::max(1,cfg.lurchDelay-1);if(x>=550)cfg.lurchDelay++;}
            if(y>=150&&y<=185){if(x>=510&&x<550)cfg.lurchHold=std::max(1,cfg.lurchHold-1);if(x>=550)cfg.lurchHold++;}
            InvalidateRect(h,nullptr,TRUE);
        } return 0;}
    case WM_PAINT:{PAINTSTRUCT p{};HDC d=BeginPaint(h,&p);SetBkMode(d,TRANSPARENT);
        HFONT font=CreateFontW(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(d,font);
        Txt(d,30,18,L"StrafeHelper  |  SAFE EDITION");
        Rectangle(d,20,45,180,80);Txt(d,65,56,L"Config");Rectangle(d,180,45,360,80);Txt(d,215,56,L"State Monitor");
        Rectangle(d,360,45,520,80);Txt(d,405,56,L"Console");
        if(st.tab==0)DrawConfig(d);else if(st.tab==1)DrawMonitor(d);else DrawConsole(d);
        SelectObject(d,old);DeleteObject(font);EndPaint(h,&p);return 0;}
    case WM_DESTROY:Save();PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE i,HINSTANCE,PWSTR,int n){
    Load();WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=i;c.lpszClassName=L"StrafeHelperSafe";
    c.hCursor=LoadCursor(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&c);
    gWnd=CreateWindowExW(0,c.lpszClassName,L"StrafeHelper - Safe Edition",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,760,510,nullptr,nullptr,i,nullptr);if(!gWnd)return 1;ShowWindow(gWnd,n);
    MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return(int)m.wParam;
}