int _fltused=0;
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
__declspec(dllimport) void APIENTRY glClearColor(float,float,float,float);
__declspec(dllimport) void APIENTRY glClear(unsigned int);
void *memset(void *p,int c,size_t n) {volatile unsigned char *b=p;for(size_t i=0;i<n;i++)b[i]=c;return p;}
static void print(const char *s) {DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void pump(int n) {MSG m;for(int i=0;i<n;i++){while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}}
static int has_arg(const char *cmd,const char *arg){for(;*cmd;cmd++){const char *a=arg,*c=cmd;while(*a&&*a==*c){a++;c++;}if(!*a)return 1;}return 0;}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
void mainCRTStartup(void) {
 HINSTANCE module=GetModuleHandleA(0);WNDCLASSA wc={0};wc.style=CS_OWNDC;wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName="RenderProbe";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&wc);
 const char *cmd=GetCommandLineA(); HWND owner=0;int gdi=has_arg(cmd,"--gdi");
 for(const char *p=cmd;*p;p++)if(p[0]=='='&&p[1]=='0'&&p[2]=='x'){UINT_PTR v=0;p+=3;while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')){v=v*16+(*p<='9'?*p-'0':*p-'a'+10);p++;}owner=(HWND)v;break;}
 if(owner) {
  HWND child=has_arg(cmd,"--foreign-window")?owner:CreateWindowExA(0,wc.lpszClassName,"Renderer child",WS_CHILD|WS_VISIBLE,40,40,400,300,owner,0,module,0);
  if(gdi){BITMAPINFO info={0};void *bits;info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=400;info.bmiHeader.biHeight=-300;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;HDC src=CreateCompatibleDC(0);HBITMAP bitmap=CreateDIBSection(src,&info,DIB_RGB_COLORS,&bits,0,0);if(!bitmap)ExitProcess(6);SelectObject(src,bitmap);for(int j=0;j<400*300;j++)((DWORD*)bits)[j]=0x00ff0d1a;for(int i=0;i<500;i++){HDC dc=GetDC(child);HRGN clip=0;if(has_arg(cmd,"--gdi-clipped")){clip=CreateRectRgn(0,0,100,300);HRGN second=CreateRectRgn(200,0,300,300);CombineRgn(clip,clip,second,RGN_OR);DeleteObject(second);SelectClipRgn(dc,clip);}if(!BitBlt(dc,20,30,350,250,src,10,15,SRCCOPY)){print("gdi-paint-failed\n");ExitProcess(5);}GdiFlush();if(clip){SelectClipRgn(dc,0);DeleteObject(clip);}ReleaseDC(child,dc);pump(1);}DeleteDC(src);DeleteObject(bitmap);print("child-painted\n");DestroyWindow(child);ExitProcess(0);}
  HDC dc=GetDC(child);PIXELFORMATDESCRIPTOR pfd={0};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=24;
  int f=ChoosePixelFormat(dc,&pfd);if(!f||!SetPixelFormat(dc,f,&pfd)){print("pixel-format-failed\n");ExitProcess(2);}HGLRC ctx=wglCreateContext(dc);if(!ctx||!wglMakeCurrent(dc,ctx)){print("context-failed\n");ExitProcess(3);}
  for(int i=0;i<500;i++){glClearColor(1.0f,0.05f,0.1f,1.0f);glClear(0x4000);SwapBuffers(dc);pump(1);}
  print("child-rendered\n");wglMakeCurrent(0,0);wglDeleteContext(ctx);DestroyWindow(child);ExitProcess(0);
 }
 HWND parent=CreateWindowExA(0,wc.lpszClassName,gdi?"Cross-process software bitmap":"Cross-process OpenGL",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,600,450,0,0,module,0);if(!parent){print("parent-failed\n");ExitProcess(1);}pump(100);HWND target=parent;if(has_arg(cmd,"--foreign-window"))target=CreateWindowExA(0,wc.lpszClassName,"Parent-owned child",WS_CHILD|WS_VISIBLE,40,40,400,300,parent,0,module,0);
 char command[2048];DWORD n=GetModuleFileNameA(0,command+1,1800);command[0]='"';n++;DWORD slash=1;for(DWORD i=1;i<n;i++)if(command[i]=='\\'||command[i]=='/')slash=i+1;n=slash;const char *helper="INTLWebViewHelper.exe";while(*helper)command[n++]=*helper++;command[n++]='"';command[n++]=' ';const char *arg="--child=0x";while(*arg)command[n++]=*arg++;UINT_PTR v=(UINT_PTR)target;for(int i=15;i>=0;i--)command[n++]="0123456789abcdef"[(v>>(i*4))&15];if(gdi){const char *mode=has_arg(cmd,"--gdi-clipped")?" --gdi-clipped":" --gdi";while(*mode)command[n++]=*mode++;}if(has_arg(cmd,"--foreign-window")){const char *mode=" --foreign-window";while(*mode)command[n++]=*mode++;}command[n]=0;
 STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};si.cb=sizeof(si);if(!CreateProcessA(0,command,0,0,FALSE,0,0,0,&si,&pi)){print("spawn-failed\n");ExitProcess(4);}pump(400);if(gdi){HDC dc=GetDC(parent);COLORREF red=GetPixel(dc,90,100),gap=GetPixel(dc,190,100);ReleaseDC(parent,dc);if(red!=RGB(255,13,26)||(has_arg(cmd,"--gdi-clipped")&&gap!=RGB(255,255,255))){print("parent-pixels-failed\n");ExitProcess(7);}print("parent-pixels-ok\n");}print("parent-ready\n");pump(300);WaitForSingleObject(pi.hProcess,10000);DWORD status;GetExitCodeProcess(pi.hProcess,&status);print("parent-finished\n");DestroyWindow(parent);ExitProcess(status);
}
