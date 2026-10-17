int _fltused=0;
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Chromium's <select> dropdown on Windows: a non-activating WS_POPUP created
 * by the browser process with the browser's child HWND as parent. Here that
 * child lives in another process's top-level, as in NIKKE's store, so the
 * popup's owner is foreign. Exit 0 if the child keeps keyboard focus and the
 * popup never becomes active while it stays open. With --wait-click, a click
 * on the popup must also reach it, without taking focus; with --wait-key, a
 * key press must reach the focused child; with --wait-wheel, a mouse wheel
 * turn over the popup must reach it. --reopen hides and shows it again.
 * --local creates child and popup in the probe's own process instead, as a
 * CEF browser embedded in the game does; only its renderers are helpers.
 * --chromium uses the styles of NIKKE store's dropdown: WS_THICKFRAME and
 * no owner, positioned over the game window. */
void *memset(void *p,int c,size_t n) {volatile unsigned char *b=p;for(size_t i=0;i<n;i++)b[i]=c;return p;}
void *memcpy(void *d,const void *s,size_t n) {volatile unsigned char *o=d;const unsigned char *i=s;while(n--)*o++=*i++;return d;}
static void print(const char *s) {DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void pump(int n) {MSG m;for(int i=0;i<n;i++){while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}}
static int has_arg(const char *cmd,const char *arg){for(;*cmd;cmd++){const char *a=arg,*c=cmd;while(*a&&*a==*c){a++;c++;}if(!*a)return 1;}return 0;}
static HWND child,popup;static int child_lost,popup_activated,popup_focused,popup_clicked,child_key,popup_wheel;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(h==child&&m==WM_KILLFOCUS&&popup){child_lost=1;print("child-lost-focus\n");}
 if(h==popup&&m==WM_ACTIVATE&&LOWORD(w)!=WA_INACTIVE){popup_activated=1;print("popup-activated\n");}
 if(h==popup&&m==WM_SETFOCUS){popup_focused=1;print("popup-focused\n");}
 if(h==popup&&m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(h==popup&&m==WM_LBUTTONDOWN){popup_clicked=1;print("popup-clicked\n");}
 if(h==child&&m==WM_KEYDOWN&&popup){child_key=1;print("child-key\n");}
 if(m==WM_MOUSEWHEEL&&popup){if(h==popup)popup_wheel=1;print(h==popup?"popup-wheel\n":"wheel-elsewhere\n");}
 if(h==popup&&m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);HBRUSH b=CreateSolidBrush(RGB(30,90,255));FillRect(dc,&ps.rcPaint,b);DeleteObject(b);EndPaint(h,&ps);return 0;}
 return DefWindowProcA(h,m,w,l);}
void mainCRTStartup(void) {
 HINSTANCE module=GetModuleHandleA(0);WNDCLASSA wc={0};wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName="DropdownProbe";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursorA(0,(LPCSTR)IDC_ARROW);RegisterClassA(&wc);
 const char *cmd=GetCommandLineA();HWND owner=0;
 for(const char *p=cmd;*p;p++)if(p[0]=='='&&p[1]=='0'&&p[2]=='x'){UINT_PTR v=0;p+=3;while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')){v=v*16+(*p<='9'?*p-'0':*p-'a'+10);p++;}owner=(HWND)v;break;}
 if(!owner&&has_arg(cmd,"--local")){owner=CreateWindowExA(0,wc.lpszClassName,"Game",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,700,500,0,0,module,0);if(!owner){print("parent-failed\n");ExitProcess(1);}pump(150);}
 if(owner) {
  child=CreateWindowExA(0,wc.lpszClassName,"Browser child",WS_CHILD|WS_VISIBLE,20,20,500,350,owner,0,module,0);
  if(!child){print("child-failed\n");ExitProcess(2);}
  pump(50);SetFocus(child);pump(20);
  if(GetFocus()!=child){print("child-focus-failed\n");ExitProcess(3);}
  RECT r;GetWindowRect(child,&r);
  if(has_arg(cmd,"--chromium"))popup=CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,0,WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_THICKFRAME,r.left+60,r.top+80,240,180,0,0,module,0);
  else popup=CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,wc.lpszClassName,"",WS_POPUP,r.left+60,r.top+80,240,180,child,0,module,0);
  if(!popup){print("popup-failed\n");ExitProcess(4);}
  if(GetWindow(popup,GW_OWNER)!=(has_arg(cmd,"--chromium")?0:owner)){print("popup-owner-wrong\n");ExitProcess(5);}
  ShowWindow(popup,SW_SHOWNOACTIVATE);
  for(int i=0;i<300&&!child_lost&&!popup_activated&&!popup_focused;i++)pump(1);
  if(has_arg(cmd,"--reopen")){ShowWindow(popup,SW_HIDE);pump(50);ShowWindow(popup,SW_SHOWNOACTIVATE);print("popup-reopened\n");for(int i=0;i<100&&!child_lost&&!popup_activated&&!popup_focused;i++)pump(1);}
  if(has_arg(cmd,"--wait-click"))for(int i=0;i<1500&&!popup_clicked&&!child_lost;i++)pump(1);
  if(has_arg(cmd,"--wait-key"))for(int i=0;i<1500&&!child_key&&!child_lost;i++)pump(1);
  if(has_arg(cmd,"--wait-wheel"))for(int i=0;i<1500&&!popup_wheel&&!child_lost;i++)pump(1);
  if(GetFocus()!=child)child_lost=1;
  if(child_lost||popup_activated||popup_focused){print("dropdown-would-close\n");ExitProcess(8);}
  if(has_arg(cmd,"--wait-click")&&!popup_clicked){print("popup-click-missing\n");ExitProcess(9);}
  if(has_arg(cmd,"--wait-key")&&!child_key){print("child-key-missing\n");ExitProcess(10);}
  if(has_arg(cmd,"--wait-wheel")&&!popup_wheel){print("popup-wheel-missing\n");ExitProcess(11);}
  print("dropdown-stayed-open\n");DestroyWindow(popup);DestroyWindow(child);ExitProcess(0);
 }
 HWND parent=CreateWindowExA(0,wc.lpszClassName,"Game",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,700,500,0,0,module,0);if(!parent){print("parent-failed\n");ExitProcess(1);}pump(150);
 if(GetForegroundWindow()!=parent)print("parent-not-foreground\n");
 char command[2048];DWORD n=GetModuleFileNameA(0,command+1,1800);command[0]='"';n++;DWORD slash=1;for(DWORD i=1;i<n;i++)if(command[i]=='\\'||command[i]=='/')slash=i+1;n=slash;const char *helper="INTLWebViewHelper.exe";while(*helper)command[n++]=*helper++;command[n++]='"';command[n++]=' ';const char *arg="--owner=0x";while(*arg)command[n++]=*arg++;UINT_PTR v=(UINT_PTR)parent;for(int i=15;i>=0;i--)command[n++]="0123456789abcdef"[(v>>(i*4))&15];const char *flags[]={" --wait-click"," --wait-key"," --wait-wheel"," --reopen"," --chromium"};for(int f=0;f<5;f++)if(has_arg(cmd,flags[f]+1)){const char *mode=flags[f];while(*mode)command[n++]=*mode++;}command[n]=0;
 STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};si.cb=sizeof(si);if(!CreateProcessA(0,command,0,0,FALSE,0,0,0,&si,&pi)){print("spawn-failed\n");ExitProcess(6);}
 DWORD status=STILL_ACTIVE;for(int i=0;i<3000&&status==STILL_ACTIVE;i++){pump(1);GetExitCodeProcess(pi.hProcess,&status);}
 print("parent-finished\n");DestroyWindow(parent);ExitProcess(status);
}
