#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <ctype.h>
#include <winhttp.h>
#include <shellapi.h>

#define ED 101
#define CONSOLE_OUT 102
#define RUN 103
#define NEW 104
#define OPEN 105
#define SAVE 106
#define GUIDE 107
#define CLEAR 108
#define EXTENSIONS 111
#define SETTINGS 109
#define DARK 201
#define LIGHT 202
#define FONT_SMALL 203
#define FONT_NORMAL 204
#define FONT_LARGE 205
#define SET_APPLY 206
#define CHECK_UPDATES 110
#define WM_APP_UPDATE_RESULT (WM_APP + 20)
#define WM_APP_RUN_DONE (WM_APP + 21)

#define EPLUS_VERSION L"1.5.0"
#define EPLUS_REPO_OWNER L"liamsettles8-hash"
#define EPLUS_REPO_NAME L"EPlus"
#define EPLUS_RELEASE_ASSET L"EPlusStudio-Setup.exe"
#define EPLUS_RELEASE_ASSET_A "EPlusStudio-Setup.exe"

static HWND mainWnd, editor, console;
static HFONT font = NULL;
static int darkMode = 1;
static int fontSize = 18;
static COLORREF bgColor, panelColor, textColor, inputColor;
static HBRUSH bgBrush = NULL, panelBrush = NULL, inputBrush = NULL;
static HANDLE childStdinWrite = NULL;
static HWND extPanel=NULL, extCanvas=NULL;
static int extMode=0, extDrawing=0, extBrush=8;
static COLORREF extColor=RGB(30,30,30);
static WNDPROC oldConsoleProc = NULL;
static volatile LONG runActive = 0;

static void colors(void){
    if(darkMode){ bgColor=RGB(18,20,25); panelColor=RGB(28,31,38); textColor=RGB(235,238,245); inputColor=RGB(22,25,31); }
    else { bgColor=RGB(245,247,250); panelColor=RGB(255,255,255); textColor=RGB(30,33,40); inputColor=RGB(250,251,253); }
}
static void brushes(void){
    if(bgBrush) DeleteObject(bgBrush); if(panelBrush) DeleteObject(panelBrush); if(inputBrush) DeleteObject(inputBrush);
    bgBrush=CreateSolidBrush(bgColor); panelBrush=CreateSolidBrush(panelColor); inputBrush=CreateSolidBrush(inputColor);
}
static void make_font(void){
    if(font) DeleteObject(font);
    font=CreateFontW(-fontSize,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH|FF_MODERN,L"Consolas");
    if(editor) SendMessageW(editor,WM_SETFONT,(WPARAM)font,TRUE);
    if(console) SendMessageW(console,WM_SETFONT,(WPARAM)font,TRUE);
}
static void set_text(HWND h,const wchar_t*s){SetWindowTextW(h,s);}
static void append_console(const wchar_t*s){int n=GetWindowTextLengthW(console);SendMessageW(console,EM_SETSEL,n,n);SendMessageW(console,EM_REPLACESEL,FALSE,(LPARAM)s);}
static void send_console_input(void){
    if(!childStdinWrite)return;
    int n=GetWindowTextLengthW(console);if(n<=0)return;
    wchar_t *all=(wchar_t*)malloc(((size_t)n+1)*sizeof(wchar_t));if(!all)return;
    GetWindowTextW(console,all,n+1);
    int start=n;while(start>0&&all[start-1]!=L'\n')start--;
    while(start<n&&(all[start]==L'\r'||all[start]==L'\n'))start++;
    int len=n-start;if(len<0)len=0;
    wchar_t *line=(wchar_t*)malloc(((size_t)len+2)*sizeof(wchar_t));if(!line){free(all);return;}
    memcpy(line,all+start,(size_t)len*sizeof(wchar_t));line[len]=L'\n';line[len+1]=0;
    int bytes=WideCharToMultiByte(CP_UTF8,0,line,len+1,NULL,0,NULL,NULL);
    if(bytes>0){char *b=(char*)malloc((size_t)bytes);if(b){WideCharToMultiByte(CP_UTF8,0,line,len+1,b,bytes,NULL,NULL);DWORD written=0;WriteFile(childStdinWrite,b,(DWORD)bytes,&written,NULL);free(b);}}
    SendMessageW(console,EM_SETSEL,n,n);SendMessageW(console,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
    free(line);free(all);
}
static LRESULT CALLBACK console_proc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_KEYDOWN&&w==VK_RETURN&&childStdinWrite){send_console_input();return 0;}
    return oldConsoleProc?CallWindowProcW(oldConsoleProc,h,m,w,l):DefWindowProcW(h,m,w,l);
}

static int savefile(const wchar_t*p){
    int n=GetWindowTextLengthW(editor); wchar_t*w=(wchar_t*)calloc((size_t)n+1,sizeof(wchar_t)); if(!w)return 0;
    GetWindowTextW(editor,w,n+1); FILE*f=_wfopen(p,L"wb"); if(!f){free(w);return 0;}
    int m=WideCharToMultiByte(CP_UTF8,0,w,n,NULL,0,NULL,NULL); char*b=(char*)malloc((size_t)m+1);
    if(!b){fclose(f);free(w);return 0;} WideCharToMultiByte(CP_UTF8,0,w,n,b,m,NULL,NULL); fwrite(b,1,(size_t)m,f); fclose(f); free(b);free(w); return 1;
}
static int openfile(const wchar_t*p){
    FILE*f=_wfopen(p,L"rb");if(!f)return 0;fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    char*b=(char*)malloc((size_t)n+1);if(!b){fclose(f);return 0;}fread(b,1,(size_t)n,f);b[n]=0;fclose(f);
    int m=MultiByteToWideChar(CP_UTF8,0,b,(int)n,NULL,0);wchar_t*w=(wchar_t*)malloc(((size_t)m+1)*sizeof(wchar_t));if(!w){free(b);return 0;}
    MultiByteToWideChar(CP_UTF8,0,b,(int)n,w,m);w[m]=0;set_text(editor,w);free(w);free(b);return 1;
}


typedef struct UpdateInfo {
    int available;
    wchar_t version[64];
    wchar_t downloadUrl[2048];
} UpdateInfo;

static int version_newer(const wchar_t *latest, const wchar_t *current){
    int a=0,b=0,c=0,x=0,y=0,z=0;
    swscanf_s(latest,L"%d.%d.%d",&a,&b,&c);
    swscanf_s(current,L"%d.%d.%d",&x,&y,&z);
    if(a!=x)return a>x; if(b!=y)return b>y; return c>z;
}

static int http_get_text(const wchar_t *host,const wchar_t *path,char **out,DWORD *outLen){
    *out=NULL; *outLen=0;
    HINTERNET ses=WinHttpOpen(L"EPlusStudio-Updater/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!ses)return 0;
    HINTERNET con=WinHttpConnect(ses,host,INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!con){WinHttpCloseHandle(ses);return 0;}
    HINTERNET req=WinHttpOpenRequest(con,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!req){WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    wchar_t hdrs[]=L"Accept: application/vnd.github+json\r\nUser-Agent: EPlusStudio-Updater\r\n";
    int ok=WinHttpSendRequest(req,hdrs,(DWORD)-1L,WINHTTP_NO_REQUEST_DATA,0,0,0) && WinHttpReceiveResponse(req,NULL);
    if(!ok){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    DWORD status=0,statusSize=sizeof(status);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX);
    if(status!=200){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    char *buf=NULL; DWORD used=0;
    for(;;){
        DWORD avail=0;if(!WinHttpQueryDataAvailable(req,&avail))break;if(!avail)break;
        char *nb=(char*)realloc(buf,(size_t)used+avail+1);if(!nb){free(buf);buf=NULL;break;}buf=nb;
        DWORD got=0;if(!WinHttpReadData(req,buf+used,avail,&got)||!got)break;used+=got;
    }
    if(buf)buf[used]=0;*out=buf;*outLen=used;
    WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);
    return buf!=NULL;
}

static int find_json_string(const char *json,const char *key,wchar_t *out,size_t cap){
    char needle[128];sprintf_s(needle,sizeof(needle),"\"%s\"",key);
    const char *p=strstr(json,needle);if(!p)return 0;p+=strlen(needle);
    while(*p&&(*p==':'||isspace((unsigned char)*p)))p++;if(*p!='\"')return 0;p++;
    const char *e=strchr(p,'\"');if(!e)return 0;
    size_t n=(size_t)(e-p);if(n>1023)n=1023;char tmp[1024];memcpy(tmp,p,n);tmp[n]=0;
    int m=MultiByteToWideChar(CP_UTF8,0,tmp,(int)n,out,(int)cap-1);if(!m)return 0;out[m]=0;return 1;
}

static int find_asset_url(const char *json,wchar_t *out,size_t cap){
    const char *p=json;const char *needle="\"browser_download_url\"";
    while((p=strstr(p,needle))){
        p+=strlen(needle);while(*p&&(*p==':'||isspace((unsigned char)*p)))p++;if(*p!='\"')continue;p++;
        const char *e=strchr(p,'\"');if(!e)return 0;
        size_t n=(size_t)(e-p);if(n>2047)n=2047;char tmp[2048];memcpy(tmp,p,n);tmp[n]=0;
        if(strstr(tmp,EPLUS_RELEASE_ASSET_A)){
            int m=MultiByteToWideChar(CP_UTF8,0,tmp,(int)n,out,(int)cap-1);if(!m)return 0;out[m]=0;return 1;
        }
        p=e+1;
    }
    return 0;
}

static int get_latest_update(UpdateInfo *u){
    ZeroMemory(u,sizeof(*u));
    if(!wcscmp(EPLUS_REPO_OWNER,L"YOUR_GITHUB_USERNAME"))return 0;
    wchar_t path[512];swprintf_s(path,512,L"/repos/%s/%s/releases/latest",EPLUS_REPO_OWNER,EPLUS_REPO_NAME);
    char *json=NULL;DWORD len=0;
    if(!http_get_text(L"api.github.com",path,&json,&len))return 0;
    wchar_t tag[64];int ok=find_json_string(json,"tag_name",tag,64);
    if(ok){
        wchar_t clean[64];wcscpy_s(clean,64,tag);if(clean[0]==L'v'||clean[0]==L'V')wcscpy_s(clean,64,clean+1);
        wcscpy_s(u->version,64,clean);u->available=version_newer(clean,EPLUS_VERSION);if(u->available)find_asset_url(json,u->downloadUrl,2048);
    }
    free(json);return ok;
}

static int download_file(const wchar_t *url,const wchar_t *dest){
    URL_COMPONENTSW uc={0};wchar_t host[256],path[4096];host[0]=0;path[0]=0;uc.dwStructSize=sizeof(uc);uc.lpszHostName=host;uc.dwHostNameLength=sizeof(host)/sizeof(wchar_t);uc.lpszUrlPath=path;uc.dwUrlPathLength=sizeof(path)/sizeof(wchar_t);
    if(!WinHttpCrackUrl(url,0,0,&uc))return 0;
    HINTERNET ses=WinHttpOpen(L"EPlusStudio-Updater/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!ses)return 0;
    HINTERNET con=WinHttpConnect(ses,host,uc.nPort,0);if(!con){WinHttpCloseHandle(ses);return 0;}
    DWORD flags=(uc.nScheme==INTERNET_SCHEME_HTTPS)?WINHTTP_FLAG_SECURE:0;HINTERNET req=WinHttpOpenRequest(con,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);if(!req){WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    wchar_t hdrs[]=L"User-Agent: EPlusStudio-Updater\r\n";int ok=WinHttpSendRequest(req,hdrs,(DWORD)-1L,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(req,NULL);if(!ok){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    DWORD status=0,ss=sizeof(status);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&ss,WINHTTP_NO_HEADER_INDEX);if(status!=200){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    FILE *f=_wfopen(dest,L"wb");if(!f){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return 0;}
    char buf[32768];DWORD got=0;while(WinHttpReadData(req,buf,sizeof(buf),&got)&&got){if(fwrite(buf,1,got,f)!=got){ok=0;break;}}
    fclose(f);WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return ok;
}

static void install_update(HWND h, const wchar_t *url){
    wchar_t tmp[MAX_PATH],installer[MAX_PATH];GetTempPathW(MAX_PATH,tmp);swprintf_s(installer,MAX_PATH,L"%sEPlusStudio-Setup.exe",tmp);
    set_text(console,L"E#+ Updater\r\n\r\n> Downloading the latest installer...\r\n");
    if(!download_file(url,installer)){append_console(L"> Update download failed.\r\n");MessageBoxW(h,L"E#+ could not download the update.",L"E#+ Update",MB_OK|MB_ICONERROR);return;}
    append_console(L"> Update downloaded. E#+ Studio will close and the installer will start.\r\n");
    ShellExecuteW(h,L"open",installer,NULL,NULL,SW_SHOWNORMAL);PostMessageW(h,WM_CLOSE,0,0);
}

static DWORD WINAPI update_thread(LPVOID p){
    HWND h=(HWND)p;UpdateInfo *u=(UpdateInfo*)calloc(1,sizeof(UpdateInfo));if(!u)return 0;get_latest_update(u);PostMessageW(h,WM_APP_UPDATE_RESULT,0,(LPARAM)u);return 0;
}

static void check_updates(HWND h,int automatic){
    UpdateInfo *u=(UpdateInfo*)calloc(1,sizeof(UpdateInfo));if(!u)return;
    if(!get_latest_update(u)){free(u);if(!automatic)MessageBoxW(h,L"Could not check for updates. Make sure your GitHub repository settings in studio.c are correct and you have an internet connection.",L"E#+ Update",MB_OK|MB_ICONWARNING);return;}
    if(!u->available){free(u);if(!automatic)MessageBoxW(h,L"You're up to date!\r\n\r\nE#+ Studio " EPLUS_VERSION L" is the latest release.",L"E#+ Update",MB_OK|MB_ICONINFORMATION);return;}
    wchar_t msg[512];swprintf_s(msg,512,L"E#+ Studio %s is available.\r\n\r\nYou have %s.\r\nNew version: %s\r\n\r\nDownload and install it now?",u->version,EPLUS_VERSION,u->version);
    int r=MessageBoxW(h,msg,L"E#+ Update Available",MB_YESNO|MB_ICONINFORMATION);if(r==IDYES&&u->downloadUrl[0])install_update(h,u->downloadUrl);else if(!u->downloadUrl[0])MessageBoxW(h,L"The release exists, but EPlusStudio-Setup.exe was not found in its assets.",L"E#+ Update",MB_OK|MB_ICONWARNING);
    free(u);
}

static void install_extension(HWND h,int which){
    const wchar_t *name=which==0?L"E#+ 3D Editor":L"E#+ 2D Editor";
    const wchar_t *file=which==0?L"EPlus3DEditor.html":L"EPlus2DEditor.html";
    const wchar_t *url=which==0?L"https://raw.githubusercontent.com/liamsettles8-hash/EPlus/main/extensions/EPlus3DEditor.html":L"https://raw.githubusercontent.com/liamsettles8-hash/EPlus/main/extensions/EPlus2DEditor.html";
    wchar_t base[MAX_PATH],dir[MAX_PATH],dest[MAX_PATH];
    if(!GetEnvironmentVariableW(L"APPDATA",base,MAX_PATH)){MessageBoxW(h,L"Could not find APPDATA.",L"E#+ Extensions",MB_OK|MB_ICONERROR);return;}
    swprintf_s(dir,MAX_PATH,L"%s\\EPlus\\Extensions",base);
    CreateDirectoryW(dir,NULL);
    swprintf_s(dest,MAX_PATH,L"%s\\%s",dir,file);
    set_text(console,L"E#+ Extensions\\r\\n\\r\\n> Downloading extension...\\r\\n");
    if(!download_file(url,dest)){append_console(L"> Extension download failed.\\r\\n");MessageBoxW(h,L"Could not download the extension.",L"E#+ Extensions",MB_OK|MB_ICONERROR);return;}
    append_console(L"> Extension installed.\\r\\n");
    ShellExecuteW(h,L"open",dest,NULL,NULL,SW_SHOWNORMAL);
    (void)name;
}
static LRESULT CALLBACK ext_canvas_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_LBUTTONDOWN&&extMode==1){extDrawing=1;SetCapture(w);return 0;}
    if(m==WM_LBUTTONUP){extDrawing=0;ReleaseCapture();return 0;}
    if(m==WM_MOUSEMOVE&&extDrawing&&extMode==1){
        HDC dc=GetDC(w);int x=(int)(short)LOWORD(lp),y=(int)(short)HIWORD(lp);
        HBRUSH b=CreateSolidBrush(extColor);HGDIOBJ old=SelectObject(dc,b);
        Ellipse(dc,x-extBrush,y-extBrush,x+extBrush,y+extBrush);
        SelectObject(dc,old);DeleteObject(b);ReleaseDC(w,dc);return 0;
    }
    if(m==WM_PAINT){
        PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        FillRect(dc,&r,CreateSolidBrush(extMode==1?RGB(245,245,245):RGB(16,19,25)));
        if(extMode==2){
            HPEN p=CreatePen(PS_SOLID,2,RGB(100,150,240));HGDIOBJ old=SelectObject(dc,p);
            int cx=r.right/2,cy=r.bottom/2;
            Rectangle(dc,cx-70,cy-70,cx+70,cy+70);
            MoveToEx(dc,cx-70,cy-70,NULL);LineTo(dc,cx-40,cy-100);LineTo(dc,cx+100,cy-100);LineTo(dc,cx+70,cy-70);
            MoveToEx(dc,cx+70,cy-70,NULL);LineTo(dc,cx+100,cy-100);LineTo(dc,cx+100,cy+40);LineTo(dc,cx+70,cy+70);
            SelectObject(dc,old);DeleteObject(p);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(220,225,235));TextOutW(dc,20,20,L"3D Scene Editor",15);
        }
        EndPaint(w,&ps);return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
static void register_extension_canvas(HINSTANCE hi){
    WNDCLASSW c={0};c.lpfnWndProc=ext_canvas_proc;c.hInstance=hi;c.hCursor=LoadCursor(NULL,IDC_CROSS);c.lpszClassName=L"EPlusExtensionCanvas";RegisterClassW(&c);
}
static void extensions_window(HWND h){
    HMENU bar=GetMenu(h);if(!bar)return;
    HMENU menu=GetSubMenu(bar,0);if(!menu)return;
    POINT p;GetCursorPos(&p);SetForegroundWindow(h);
    TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN,p.x,p.y,0,h,NULL);
    PostMessageW(h,WM_NULL,0,0);
}

static void file_dialog(int save){
    wchar_t p[MAX_PATH]=L"Program.eplus";OPENFILENAMEW o={0};o.lStructSize=sizeof(o);o.hwndOwner=mainWnd;
    o.lpstrFilter=L"E#+ files (*.eplus)\0*.eplus\0All files\0*.*\0";o.lpstrFile=p;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"eplus";
    o.Flags=save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST;if((save?GetSaveFileNameW(&o):GetOpenFileNameW(&o))){if(save)savefile(p);else openfile(p);}
}

static DWORD WINAPI run_worker(LPVOID param){
    (void)param;
    if(InterlockedCompareExchange(&runActive,1,0)!=0){append_console(L"E#+ is already running.\r\n");return 0;}
    wchar_t tmp[MAX_PATH],dir[MAX_PATH],eng[MAX_PATH],cmd[2*MAX_PATH];
    GetTempPathW(MAX_PATH,tmp);wcscat_s(tmp,MAX_PATH,L"EPlusStudio_Run.eplus");
    if(!savefile(tmp)){append_console(L"Could not create temporary file.\r\n");InterlockedExchange(&runActive,0);return 0;}
    GetModuleFileNameW(NULL,dir,MAX_PATH);wchar_t*slash=wcsrchr(dir,L'\\');if(slash)*slash=0;
    swprintf_s(eng,MAX_PATH,L"%s\\eplus-engine.exe",dir);
    if(GetFileAttributesW(eng)==INVALID_FILE_ATTRIBUTES){append_console(L"ERROR: eplus-engine.exe was not found next to EPlusStudio.exe.\r\n");InterlockedExchange(&runActive,0);return 0;}
    swprintf_s(cmd,2*MAX_PATH,L"\"%s\" \"%s\"",eng,tmp);

    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
    HANDLE r,w,inR,inW;
    if(!CreatePipe(&r,&w,&sa,0)){InterlockedExchange(&runActive,0);return 0;}
    SetHandleInformation(r,HANDLE_FLAG_INHERIT,0);
    if(!CreatePipe(&inR,&inW,&sa,0)){CloseHandle(r);CloseHandle(w);InterlockedExchange(&runActive,0);return 0;}
    SetHandleInformation(inW,HANDLE_FLAG_INHERIT,0);

    STARTUPINFOW si={sizeof(si)};
    PROCESS_INFORMATION pi={0};
    si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdOutput=w;
    si.hStdError=w;
    si.hStdInput=inR;

    wchar_t cl[2*MAX_PATH];
    wcscpy_s(cl,2*MAX_PATH,cmd);
    set_text(console,L"E#+ Console\r\n\r\n> Running...\r\n");

    if(!CreateProcessW(NULL,cl,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        append_console(L"ERROR: could not start engine.\r\n");
        CloseHandle(r);CloseHandle(w);CloseHandle(inR);CloseHandle(inW);InterlockedExchange(&runActive,0);return 0;
    }

    CloseHandle(w);CloseHandle(inR);
    childStdinWrite=inW;
    char b[4096];DWORD got;
    while(ReadFile(r,b,sizeof(b)-1,&got,NULL)&&got){
        b[got]=0;
        int m=MultiByteToWideChar(CP_UTF8,0,b,(int)got,NULL,0);
        wchar_t*x=(wchar_t*)malloc(((size_t)m+1)*sizeof(wchar_t));
        if(x){MultiByteToWideChar(CP_UTF8,0,b,(int)got,x,m);x[m]=0;append_console(x);free(x);}
    }

    WaitForSingleObject(pi.hProcess,INFINITE);
    DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);
    wchar_t st[100];swprintf_s(st,100,L"\r\n> Process exited with code %lu\r\n",code);
    append_console(st);
    if(childStdinWrite){CloseHandle(childStdinWrite);childStdinWrite=NULL;}
    CloseHandle(r);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    InterlockedExchange(&runActive,0);
    return 0;
}

static void run_program(void){
    CreateThread(NULL,0,run_worker,NULL,0,NULL);
}

static void guide(HWND h){
    const wchar_t*t=L"E#+ COMPLETE CODING GUIDE\r\n\r\n"
    L"01  PRINTING\r\n    print words \"Hello, world!\"\r\n\r\n"
    L"02  VARIABLES\r\n    set name to \"Liam\"\r\n    set age to 12\r\n\r\n"
    L"03  USING VARIABLES\r\n    print words \"Hello \" + name\r\n    print words \"Age: \" + age\r\n\r\n"
    L"04  INPUT\r\n    ask user \"What is your name? \" and save answer as name\r\n\r\n"
    L"05  CONDITIONS\r\n    if age is greater than 18 then\r\n        print words \"Adult\"\r\n    else\r\n        print words \"Not an adult\"\r\n    end\r\n\r\n"
    L"06  LOOPS\r\n    repeat 3 times\r\n        print words \"Hello!\"\r\n    end\r\n\r\n"
    L"07  COMMENTS\r\n    # This line is a comment\r\n\r\n"
    L"HOW E#+ THINKS\r\nE#+ is designed to read like English. You describe what you want\r\nthe program to do instead of relying on punctuation-heavy syntax.\r\n\r\nROADMAP\r\nFunctions, arrays, richer expressions, modules, standard library,\r\nand eventually a native E#+ compiler are planned language features.";
    MessageBoxW(h,t,L"E#+ Language Guide",MB_OK|MB_ICONINFORMATION);
}

static void settings_window(HWND owner){
    WNDCLASSW wc={0};wc.lpfnWndProc=NULL; /* use a temporary dialog-like window below */
    MessageBoxW(owner,L"E#+ Settings\r\n\r\nChoose a theme:\r\nYES = Dark\r\nNO = Light\r\nCANCEL = keep current",L"Settings",MB_YESNOCANCEL|MB_ICONQUESTION);
}

static void apply_theme(HWND h){
    colors();brushes();InvalidateRect(h,NULL,TRUE);InvalidateRect(editor,NULL,TRUE);InvalidateRect(console,NULL,TRUE);
}

static void show_settings(HWND h){
    int r=MessageBoxW(h,L"THEME\r\n\r\nYES  Dark mode\r\nNO   Light mode\r\nCANCEL  Keep current",L"E#+ Settings",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(r==IDYES){darkMode=1;apply_theme(h);}else if(r==IDNO){darkMode=0;apply_theme(h);}
    r=MessageBoxW(h,L"EDITOR FONT SIZE\r\n\r\nYES  Large (20)\r\nNO   Normal (18)\r\nCANCEL  Keep current",L"E#+ Settings",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(r==IDYES){fontSize=20;make_font();}else if(r==IDNO){fontSize=18;make_font();}
}

static void show_native_extension(HWND h,int mode){
    extMode=mode;ShowWindow(editor,SW_HIDE);ShowWindow(console,SW_HIDE);
    if(!extPanel){
        extPanel=CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"E#+ Native Editor",WS_CHILD|WS_VISIBLE,10,60,1000,500,h,NULL,0,0);
        CreateWindowW(L"BUTTON",L"Back to Code",WS_CHILD|WS_VISIBLE,10,10,120,34,extPanel,(HMENU)303,0,0);
        CreateWindowW(L"BUTTON",L"New",WS_CHILD|WS_VISIBLE,140,10,90,34,extPanel,(HMENU)304,0,0);
        CreateWindowW(L"BUTTON",L"Clear",WS_CHILD|WS_VISIBLE,240,10,90,34,extPanel,(HMENU)305,0,0);
        extCanvas=CreateWindowExW(WS_EX_CLIENTEDGE,L"EPlusExtensionCanvas",L"",WS_CHILD|WS_VISIBLE,10,55,900,400,extPanel,(HMENU)306,GetModuleHandleW(NULL),0);
    } else ShowWindow(extPanel,SW_SHOW);
    InvalidateRect(extCanvas,NULL,TRUE);
}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{
        colors();brushes();
        make_font();
        editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"# Welcome to E#+\r\nprint words \"Hello from E#+!\"\r\n\r\nset name to \"developer\"\r\nprint words \"Hello \" + name\r\n",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL,10,60,500,500,h,(HMENU)ED,0,0);
        console=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"E#+ Console\r\n\r\n> Ready. Press Run to execute your E#+ program.\r\n",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL,520,60,500,500,h,(HMENU)CONSOLE_OUT,0,0);
        oldConsoleProc=(WNDPROC)SetWindowLongPtrW(console,GWLP_WNDPROC,(LONG_PTR)console_proc);
        const wchar_t*names[]={L"Run",L"New",L"Open",L"Save",L"Guide",L"Settings",L"Updates",L"Clear"};int ids[]={RUN,NEW,OPEN,SAVE,GUIDE,SETTINGS,CHECK_UPDATES,CLEAR};
        for(int i=0;i<8;i++){HWND b=CreateWindowW(L"BUTTON",names[i],WS_CHILD|WS_VISIBLE,10+i*105,12,98,34,h,(HMENU)(INT_PTR)ids[i],0,0);SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);}
        HMENU bar=CreateMenu(),extensions=CreatePopupMenu();
        AppendMenuW(bar,MF_POPUP,(UINT_PTR)extensions,L"Extensions");
        AppendMenuW(extensions,MF_STRING,301,L"2D Editor");
        AppendMenuW(extensions,MF_STRING,302,L"3D Editor");
        AppendMenuW(extensions,MF_SEPARATOR,0,NULL);
        AppendMenuW(extensions,MF_STRING,303,L"Back to Code");
        SetMenu(h,bar);
        make_font();return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{HDC dc=(HDC)w;SetTextColor(dc,textColor);SetBkColor(dc,inputColor);return (LRESULT)inputBrush;}
    case WM_ERASEBKGND:{HDC dc=(HDC)w;RECT r;GetClientRect(h,&r);FillRect(dc,&r,bgBrush);return 1;}
    case WM_SIZE:{int W=LOWORD(l),H=HIWORD(l);int left=(W-30)/2;MoveWindow(editor,10,60,left,H-70,TRUE);MoveWindow(console,left+20,60,W-left-30,H-70,TRUE);if(extPanel)MoveWindow(extPanel,10,60,W-20,H-70,TRUE);if(extCanvas)MoveWindow(extCanvas,10,55,W-40,H-110,TRUE);return 0;}
    case WM_APP_UPDATE_RESULT:{ UpdateInfo *u=(UpdateInfo*)l; if(u){ if(u->available && u->downloadUrl[0]){ wchar_t msg[512];swprintf_s(msg,512,L"E#+ Studio %s is available.\r\n\r\nUpdate now?",u->version);if(MessageBoxW(h,msg,L"E#+ Update Available",MB_YESNO|MB_ICONINFORMATION)==IDYES)install_update(h,u->downloadUrl); } free(u);} return 0;}
    case WM_COMMAND:
        switch(LOWORD(w)){
        case RUN:run_program();return 0;case NEW:set_text(editor,L"");return 0;case OPEN:file_dialog(0);return 0;case SAVE:file_dialog(1);return 0;case GUIDE:guide(h);return 0;case SETTINGS:show_settings(h);return 0;case CHECK_UPDATES:check_updates(h,0);return 0;case EXTENSIONS:extensions_window(h);return 0;case 301:show_native_extension(h,1);return 0;case 302:show_native_extension(h,2);return 0;case 303:if(extPanel)ShowWindow(extPanel,SW_HIDE);ShowWindow(editor,SW_SHOW);ShowWindow(console,SW_SHOW);return 0;case 304:extDrawing=0;InvalidateRect(extCanvas,NULL,TRUE);return 0;case 305:InvalidateRect(extCanvas,NULL,TRUE);return 0;case CLEAR:set_text(console,L"E#+ Console\r\n");return 0;}
        break;
    case WM_DESTROY:
        if(font)DeleteObject(font);if(bgBrush)DeleteObject(bgBrush);if(panelBrush)DeleteObject(panelBrush);if(inputBrush)DeleteObject(inputBrush);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void firstlaunch(HWND h){
    wchar_t a[MAX_PATH],d[MAX_PATH],f[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"APPDATA",a,MAX_PATH);if(!n)return;
    swprintf_s(d,MAX_PATH,L"%s\\EPlus",a);CreateDirectoryW(d,NULL);swprintf_s(f,MAX_PATH,L"%s\\first-run.flag",d);
    if(GetFileAttributesW(f)!=INVALID_FILE_ATTRIBUTES)return;
    int r=MessageBoxW(h,L"Welcome to E#+ Studio!\r\n\r\nE#+ is an English-first programming language.\r\n\r\nWould you like the full coding guide now?\r\n\r\nChoose No to skip it. You can open Guide anytime.",L"Welcome to E#+",MB_YESNO|MB_ICONINFORMATION);
    HANDLE x=CreateFileW(f,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(x!=INVALID_HANDLE_VALUE)CloseHandle(x);if(r==IDYES)guide(h);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE hp,PWSTR cmd,int show){
    (void)hp;(void)cmd;WNDCLASSW c={0};c.lpfnWndProc=wnd;c.hInstance=hi;c.hCursor=LoadCursor(NULL,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);c.lpszClassName=L"EPlusStudio";RegisterClassW(&c);
    mainWnd=CreateWindowW(L"EPlusStudio",L"E#+ Studio 0.2",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1150,700,NULL,NULL,hi,NULL);if(!mainWnd)return 1;
    register_extension_canvas(hi);ShowWindow(mainWnd,show);UpdateWindow(mainWnd);firstlaunch(mainWnd);CreateThread(NULL,0,update_thread,mainWnd,0,NULL);MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return(int)msg.wParam;
}
