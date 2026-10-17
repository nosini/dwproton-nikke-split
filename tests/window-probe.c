void *memset(void *p,int c,unsigned long long n){volatile unsigned char *b=p;for(unsigned long long i=0;i<n;i++)b[i]=c;return p;}
typedef void *HANDLE;
typedef HANDLE HWND; typedef HANDLE HDC; typedef HANDLE HINSTANCE; typedef HANDLE HBRUSH;
typedef unsigned int UINT; typedef unsigned long DWORD; typedef unsigned short ATOM;
typedef unsigned long long WPARAM; typedef long long LPARAM; typedef long long LRESULT;
typedef int BOOL; typedef unsigned short WORD;
#define API __declspec(dllimport)
#define CALLBACK __attribute__((ms_abi))
typedef LRESULT (CALLBACK *WNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef struct {UINT style; WNDPROC proc; int cls,win; HINSTANCE instance; HANDLE icon,cursor; HBRUSH background; const char *menu,*name;} WNDCLASSA;
typedef struct {HWND hwnd; UINT message; WPARAM wparam; LPARAM lparam; DWORD time; long x,y; DWORD private;} MSG;
API HINSTANCE GetModuleHandleA(const char*);
API ATOM RegisterClassA(const WNDCLASSA*);
API HWND CreateWindowExA(DWORD,const char*,const char*,DWORD,int,int,int,int,HWND,HANDLE,HINSTANCE,void*);
API BOOL ShowWindow(HWND,int); API BOOL DestroyWindow(HWND); API BOOL UpdateWindow(HWND);
API BOOL PeekMessageA(MSG*,HWND,UINT,UINT,UINT); API BOOL TranslateMessage(const MSG*);
API LRESULT DispatchMessageA(const MSG*); API LRESULT DefWindowProcA(HWND,UINT,WPARAM,LPARAM);
API void Sleep(DWORD); API void ExitProcess(UINT); API HANDLE GetStdHandle(DWORD);
API BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
API const char *GetCommandLineA(void);
static void print(const char *s) {DWORD n=0,w; while(s[n])n++; WriteFile(GetStdHandle((DWORD)-11),s,n,&w,0);}
static void pump(int count) {MSG m; for(int i=0;i<count;i++){while(PeekMessageA(&m,0,0,0,1)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProcA(h,m,w,l);}
void mainCRTStartup(void) {
 HINSTANCE module=GetModuleHandleA(0); WNDCLASSA wc={0}; wc.proc=proc; wc.instance=module; wc.name="StoreSplitProbe"; wc.background=(HBRUSH)6;
 RegisterClassA(&wc);
 HWND parent=CreateWindowExA(0,wc.name,"Split store popup probe",0x10cf0000,80,80,600,450,0,0,module,0);
 if(!parent){print("parent-failed\n");ExitProcess(1);} pump(100);
 HWND first=CreateWindowExA(0x80,wc.name,"first",0x80000000,120,120,220,180,parent,0,module,0);
 ShowWindow(first,4); UpdateWindow(first); pump(100);
 HWND second=CreateWindowExA(0x80,wc.name,"second",0x80000000,180,180,160,120,first,0,module,0);
 ShowWindow(second,4); UpdateWindow(second); pump(100);
 print("popups-mapped\n"); ShowWindow(first,0); pump(100); print("survived-first-hide\n");
 for(int i=0;i<3;i++){ShowWindow(first,4);pump(20);ShowWindow(first,0);pump(20);}
 print("survived-role-rebuilds\n");
 DestroyWindow(second);DestroyWindow(first);DestroyWindow(parent);pump(20);print("finished\n");ExitProcess(0);
}
