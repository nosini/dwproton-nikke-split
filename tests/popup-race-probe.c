void *memset(void *p,int c,unsigned long long n){volatile unsigned char *b=p;for(unsigned long long i=0;i<n;i++)b[i]=c;return p;}
typedef void *HANDLE;
typedef HANDLE HWND; typedef HANDLE HDC; typedef HANDLE HINSTANCE; typedef HANDLE HBRUSH;
typedef unsigned int UINT; typedef unsigned long DWORD; typedef unsigned short ATOM;
typedef unsigned long long WPARAM; typedef long long LPARAM; typedef long long LRESULT;
typedef int BOOL; typedef unsigned short WORD;
#define API __declspec(dllimport)
#define CALLBACK __attribute__((ms_abi))
typedef LRESULT (CALLBACK *WNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef DWORD (CALLBACK *THREADPROC)(void*);
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
API HANDLE CreateThread(void*,unsigned long long,THREADPROC,void*,DWORD,DWORD*);
API HANDLE CreateEventA(void*,BOOL,BOOL,const char*); API BOOL SetEvent(HANDLE);
API DWORD WaitForSingleObject(HANDLE,DWORD); API BOOL CloseHandle(HANDLE);
static void print(const char *s) {DWORD n=0,w; while(s[n])n++; WriteFile(GetStdHandle((DWORD)-11),s,n,&w,0);}
static void pump(int count) {MSG m; for(int i=0;i<count;i++){while(PeekMessageA(&m,0,0,0,1)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProcA(h,m,w,l);}
static HINSTANCE module; static HWND parent; static HANDLE ready, go;
static const char *class_name="StoreSplitRaceProbe";
/* The worker maps a nested popup pair owned by the main thread's window and
 * exits without hiding them. Wine then destroys those windows without the
 * hide that DestroyWindow does first, so their roles are still live, while
 * the main thread clears the owner's role. */
static DWORD CALLBACK worker(void *unused) {
 HWND first=CreateWindowExA(0x80,class_name,"first",0x80000000,120,120,220,180,parent,0,module,0);
 ShowWindow(first,4); UpdateWindow(first);
 HWND second=CreateWindowExA(0x80,class_name,"second",0x80000000,180,180,160,120,first,0,module,0);
 ShowWindow(second,4); UpdateWindow(second); pump(3);
 SetEvent(ready); WaitForSingleObject(go,(DWORD)-1);
 return 0;
}
void mainCRTStartup(void) {
 const char *arg=GetCommandLineA(); int iterations=0;
 while(*arg&&*arg!=' ')arg++;
 while(*arg==' ')arg++;
 while(*arg>='0'&&*arg<='9')iterations=iterations*10+*arg++-'0';
 if(!iterations)iterations=200;
 module=GetModuleHandleA(0); WNDCLASSA wc={0}; wc.proc=proc; wc.instance=module; wc.name=class_name; wc.background=(HBRUSH)6;
 RegisterClassA(&wc);
 parent=CreateWindowExA(0,wc.name,"Split store popup race probe",0x10cf0000,80,80,600,450,0,0,module,0);
 if(!parent){print("parent-failed\n");ExitProcess(1);} pump(50);
 ready=CreateEventA(0,0,0,0); go=CreateEventA(0,0,0,0);
 for(int i=0;i<iterations;i++){
  HANDLE thread=CreateThread(0,0,worker,0,0,0);
  if(!thread){print("thread-failed\n");ExitProcess(2);}
  WaitForSingleObject(ready,(DWORD)-1);
  SetEvent(go);
  /* Sweep the hide across the worker's exit. */
  for(volatile int spin=0;spin<(i%40)*2000;spin++);
  ShowWindow(parent,0);
  WaitForSingleObject(thread,(DWORD)-1); CloseHandle(thread);
  ShowWindow(parent,4); UpdateWindow(parent); pump(2);
 }
 print("survived-races\n");
 DestroyWindow(parent);pump(5);print("finished\n");ExitProcess(0);
}
