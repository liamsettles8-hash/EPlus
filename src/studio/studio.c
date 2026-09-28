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
#define FIREBASE_API_KEY "AIzaSyBtVou8AIqNyJQaZJkIIkvTIsk6z5-QwX4"
#define FIREBASE_PROJECT_ID "eplus-9896a"

static HWND mainWnd, editor, console;
static HFONT font = NULL;
static int darkMode = 1;
static int fontSize = 18;
static COLORREF bgColor, panelColor, textColor, inputColor;
static HBRUSH bgBrush = NULL, panelBrush = NULL, inputBrush = NULL;
static HANDLE childStdinWrite = NULL;
static HWND extPanel=NULL, extCanvas=NULL, extWindow=NULL, authWindow=NULL, signupWindow=NULL, settingsWindow=NULL, guideWindow=NULL;
static int extMode=0, extDrawing=0, extBrush=8;
static COLORREF extColor=RGB(30,30,30);
static WNDPROC oldConsoleProc = NULL;
static volatile LONG runActive = 0;
static int guestMode=0, loggedIn=0;
static char firebaseIdToken[4096]={0},firebaseUid[256]={0},firebaseEmail[320]={0};

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
    int start=n;
    while(start>0 && all[start-1]!=L'\n')start--;
    while(start<n && (all[start]==L'\r'||all[start]==L'\n'))start++;
    int len=n-start;if(len<0)len=0;
    wchar_t *line=(wchar_t*)malloc(((size_t)len+2)*sizeof(wchar_t));if(!line){free(all);return;}
    memcpy(line,all+start,(size_t)len*sizeof(wchar_t));line[len]=L'\n';line[len+1]=0;
    int bytes=WideCharToMultiByte(CP_UTF8,0,line,len+1,NULL,0,NULL,NULL);
    if(bytes>0){
        char *b=(char*)malloc((size_t)bytes);
        if(b){WideCharToMultiByte(CP_UTF8,0,line,len+1,b,bytes,NULL,NULL);DWORD written=0;WriteFile(childStdinWrite,b,(DWORD)bytes,&written,NULL);free(b);}
    }
    SendMessageW(console,EM_SETSEL,n,n);
    SendMessageW(console,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
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

static int http_request_json(const wchar_t*host,const wchar_t*path,const wchar_t*method,const char*body,const wchar_t*headers,char**out,DWORD*outLen){
 *out=NULL;*outLen=0;HINTERNET s=WinHttpOpen(L"EPlusStudio/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!s)return 0;
 HINTERNET c=WinHttpConnect(s,host,INTERNET_DEFAULT_HTTPS_PORT,0);if(!c){WinHttpCloseHandle(s);return 0;}HINTERNET r=WinHttpOpenRequest(c,method,path,NULL,WINHTTP_NO_REFERER,L"application/json",WINHTTP_FLAG_SECURE);if(!r){WinHttpCloseHandle(c);WinHttpCloseHandle(s);return 0;}
 const wchar_t*h=headers?headers:L"Content-Type: application/json\r\n";BOOL ok=WinHttpSendRequest(r,h,(DWORD)-1L,(LPVOID)(body?body:""),body?(DWORD)strlen(body):0,body?(DWORD)strlen(body):0,0);if(ok)ok=WinHttpReceiveResponse(r,NULL);
 DWORD st=0,ss=sizeof(st);if(ok)WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&st,&ss,WINHTTP_NO_HEADER_INDEX);
 char*b=NULL;DWORD n=0;if(ok)for(;;){DWORD a=0;if(!WinHttpQueryDataAvailable(r,&a)||!a)break;char*nb=(char*)realloc(b,(size_t)n+a+1);if(!nb){free(b);b=NULL;break;}b=nb;DWORD g=0;if(!WinHttpReadData(r,b+n,a,&g)||!g)break;n+=g;}if(b)b[n]=0;*out=b;*outLen=n;WinHttpCloseHandle(r);WinHttpCloseHandle(c);WinHttpCloseHandle(s);return ok&&st>=200&&st<300&&b!=NULL;
}
static int json_string(const char*j,const char*k,char*out,size_t cap){char nd[160];sprintf_s(nd,sizeof(nd),"\"%s\"",k);const char*p=strstr(j,nd);if(!p)return 0;p+=strlen(nd);while(*p&&(*p==':'||isspace((unsigned char)*p)))p++;if(*p!='\"')return 0;p++;size_t n=0;while(*p&&*p!='\"'&&n+1<cap){if(*p=='\\'&&p[1])p++;out[n++]=*p++;}out[n]=0;return 1;}
static void json_escape(const char*in,char*out,size_t cap){size_t n=0;for(const unsigned char*p=(const unsigned char*)in;*p&&n+2<cap;p++){if(*p=='\\'||*p=='\"'){out[n++]='\\';out[n++]=(char)*p;}else if(*p=='\n'){out[n++]='\\';out[n++]='n';}else if(*p=='\r'){out[n++]='\\';out[n++]='r';}else if(*p=='\t'){out[n++]='\\';out[n++]='t';}else out[n++]=(char)*p;}out[n]=0;}
static void save_cloud(HWND h){
 if(guestMode||!loggedIn||!firebaseIdToken[0]){MessageBoxW(h,L"Cloud Save requires a signed-in E#+ account.",L"E#+ Cloud Save",MB_OK|MB_ICONINFORMATION);return;}
 if(!strcmp(FIREBASE_API_KEY,"YOUR_FIREBASE_WEB_API_KEY")){MessageBoxW(h,L"Firebase is not configured in this build yet. Put your Firebase Web API key in studio.c.",L"E#+ Firebase",MB_OK|MB_ICONWARNING);return;}
 int n=GetWindowTextLengthW(editor);wchar_t*w=(wchar_t*)calloc((size_t)n+1,sizeof(wchar_t));if(!w)return;GetWindowTextW(editor,w,n+1);int bn=WideCharToMultiByte(CP_UTF8,0,w,n,NULL,0,NULL,NULL);char*src=(char*)malloc((size_t)bn+1);if(!src){free(w);return;}WideCharToMultiByte(CP_UTF8,0,w,n,src,bn,NULL,NULL);src[bn]=0;free(w);
 char*esc=(char*)malloc((size_t)bn*2+256);if(!esc){free(src);return;}json_escape(src,esc,(size_t)bn*2+256);free(src);
 size_t cap=strlen(esc)+600;char*body=(char*)malloc(cap);if(!body){free(esc);return;}sprintf_s(body,cap,"{\"fields\":{\"source\":{\"stringValue\":\"%s\"},\"email\":{\"stringValue\":\"%s\"}}}",esc,firebaseEmail);free(esc);
 char path[768];sprintf_s(path,sizeof(path),"/v1/projects/%s/databases/(default)/documents/users/%s/projects/EPlusStudio",FIREBASE_PROJECT_ID,firebaseUid);wchar_t wp[768];MultiByteToWideChar(CP_UTF8,0,path,-1,wp,768);wchar_t hd[4600];swprintf_s(hd,_countof(hd),L"Content-Type: application/json\r\nAuthorization: Bearer %S\r\n",firebaseIdToken);
 char*resp=NULL;DWORD len=0;if(http_request_json(L"firestore.googleapis.com",wp,L"PATCH",body,hd,&resp,&len))MessageBoxW(h,L"Project saved to your E#+ account.",L"E#+ Cloud Save",MB_OK|MB_ICONINFORMATION);else MessageBoxW(h,L"Cloud Save failed. Check Firestore rules and internet access.",L"E#+ Cloud Save",MB_OK|MB_ICONERROR);free(body);free(resp);
}
static void enter_editor(HWND h,int guest){
 guestMode=guest;loggedIn=!guest;ShowWindow(authWindow,SW_HIDE);authWindow=NULL;ShowWindow(signupWindow,SW_HIDE);signupWindow=NULL;SetWindowTextW(h,guest?L"E#+ Studio — Guest":L"E#+ Studio");
 for(HWND c=GetWindow(h,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){int id=GetDlgCtrlID(c);if(id==SAVE||id==CHECK_UPDATES)ShowWindow(c,guest?SW_HIDE:SW_SHOW);}
 if(!guest){HMENU bar=CreateMenu(),ex=CreatePopupMenu();AppendMenuW(bar,MF_POPUP,(UINT_PTR)ex,L"Extensions");AppendMenuW(ex,MF_STRING,301,L"2D Editor");AppendMenuW(ex,MF_STRING,302,L"3D Editor");SetMenu(h,bar);CreateThread(NULL,0,update_thread,h,0,NULL);}else SetMenu(h,NULL);
 ShowWindow(h,SW_SHOW);UpdateWindow(h);
}
static void show_signup(HWND owner);
static void show_auth(HWND owner);
static void firebase_auth(HWND w,int signup){
 wchar_t we[320],wp[320];GetWindowTextW(GetDlgItem(w,501),we,320);GetWindowTextW(GetDlgItem(w,502),wp,320);if(!we[0]||!wp[0]){MessageBoxW(w,L"Enter email and password.",L"E#+ Account",MB_OK|MB_ICONWARNING);return;}
 int eb=WideCharToMultiByte(CP_UTF8,0,we,-1,NULL,0,NULL,NULL),pb=WideCharToMultiByte(CP_UTF8,0,wp,-1,NULL,0,NULL,NULL);char*e=(char*)malloc(eb),*p=(char*)malloc(pb);if(!e||!p){free(e);free(p);return;}WideCharToMultiByte(CP_UTF8,0,we,-1,e,eb,NULL,NULL);WideCharToMultiByte(CP_UTF8,0,wp,-1,p,pb,NULL,NULL);
 char body[1400];sprintf_s(body,sizeof(body),"{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}",e,p);free(e);free(p);char path[512];sprintf_s(path,sizeof(path),"/v1/accounts:%s?key=%s",signup?"signUp":"signInWithPassword",FIREBASE_API_KEY);wchar_t wp2[512];MultiByteToWideChar(CP_UTF8,0,path,-1,wp2,512);
 char*resp=NULL;DWORD len=0;if(!http_request_json(L"identitytoolkit.googleapis.com",wp2,L"POST",body,L"Content-Type: application/json\r\n",&resp,&len)){MessageBoxW(w,L"Firebase login failed. Check the Firebase API key and Authentication settings.",L"E#+ Account",MB_OK|MB_ICONERROR);free(resp);return;}
 if(!json_string(resp,"idToken",firebaseIdToken,sizeof(firebaseIdToken))||!json_string(resp,"localId",firebaseUid,sizeof(firebaseUid))){char msg[512]={0};json_string(resp,"message",msg,sizeof(msg));free(resp);wchar_t wm[512];MultiByteToWideChar(CP_UTF8,0,msg,-1,wm,512);MessageBoxW(w,wm[0]?wm:L"Firebase returned an invalid response.",L"E#+ Account",MB_OK|MB_ICONERROR);return;}json_string(resp,"email",firebaseEmail,sizeof(firebaseEmail));free(resp);enter_editor(mainWnd,0);
}
static LRESULT CALLBACK auth_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_COMMAND){if(LOWORD(wp)==510){firebase_auth(w,0);return 0;}if(LOWORD(wp)==511){ShowWindow(w,SW_HIDE);show_signup(mainWnd);return 0;}if(LOWORD(wp)==512){enter_editor(mainWnd,1);return 0;}}if(m==WM_CLOSE)return 0;return DefWindowProcW(w,m,wp,lp);}
static LRESULT CALLBACK signup_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_COMMAND){if(LOWORD(wp)==520){firebase_auth(w,1);return 0;}if(LOWORD(wp)==521){ShowWindow(w,SW_HIDE);show_auth(mainWnd);return 0;}}if(m==WM_CLOSE)return 0;return DefWindowProcW(w,m,wp,lp);}
static void show_auth(HWND owner){
 if(authWindow){ShowWindow(authWindow,SW_SHOW);return;}WNDCLASSW c={0};c.lpfnWndProc=auth_proc;c.hInstance=GetModuleHandleW(NULL);c.hCursor=LoadCursor(NULL,IDC_ARROW);c.lpszClassName=L"EPlusAuth";RegisterClassW(&c);
 authWindow=CreateWindowW(L"EPlusAuth",L"E#+ Login",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,470,330,owner,NULL,GetModuleHandleW(NULL),NULL);
 CreateWindowW(L"STATIC",L"E#+ CLOUD LOGIN",WS_CHILD|WS_VISIBLE,30,25,390,30,authWindow,NULL,0,0);CreateWindowW(L"STATIC",L"Email",WS_CHILD|WS_VISIBLE,30,75,100,24,authWindow,NULL,0,0);CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,30,100,390,28,authWindow,(HMENU)501,0,0);CreateWindowW(L"STATIC",L"Password",WS_CHILD|WS_VISIBLE,30,140,100,24,authWindow,NULL,0,0);CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_PASSWORD,30,165,390,28,authWindow,(HMENU)502,0,0);CreateWindowW(L"BUTTON",L"Login",WS_CHILD|WS_VISIBLE,30,215,120,36,authWindow,(HMENU)510,0,0);CreateWindowW(L"BUTTON",L"Sign Up",WS_CHILD|WS_VISIBLE,160,215,120,36,authWindow,(HMENU)511,0,0);CreateWindowW(L"BUTTON",L"Continue as Guest",WS_CHILD|WS_VISIBLE,290,215,150,36,authWindow,(HMENU)512,0,0);SetForegroundWindow(authWindow);
}
static void show_signup(HWND owner){
 if(signupWindow){ShowWindow(signupWindow,SW_SHOW);return;}WNDCLASSW c={0};c.lpfnWndProc=signup_proc;c.hInstance=GetModuleHandleW(NULL);c.hCursor=LoadCursor(NULL,IDC_ARROW);c.lpszClassName=L"EPlusSignup";RegisterClassW(&c);
 signupWindow=CreateWindowW(L"EPlusSignup",L"E#+ Sign Up",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,470,330,owner,NULL,GetModuleHandleW(NULL),NULL);
 CreateWindowW(L"STATIC",L"CREATE E#+ ACCOUNT",WS_CHILD|WS_VISIBLE,30,25,390,30,signupWindow,NULL,0,0);CreateWindowW(L"STATIC",L"Email",WS_CHILD|WS_VISIBLE,30,75,100,24,signupWindow,NULL,0,0);CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,30,100,390,28,signupWindow,(HMENU)501,0,0);CreateWindowW(L"STATIC",L"Password",WS_CHILD|WS_VISIBLE,30,140,100,24,signupWindow,NULL,0,0);CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_PASSWORD,30,165,390,28,signupWindow,(HMENU)502,0,0);CreateWindowW(L"BUTTON",L"Create Account",WS_CHILD|WS_VISIBLE,30,215,180,36,signupWindow,(HMENU)520,0,0);CreateWindowW(L"BUTTON",L"Back to Login",WS_CHILD|WS_VISIBLE,230,215,180,36,signupWindow,(HMENU)521,0,0);SetForegroundWindow(signupWindow);
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

static LRESULT CALLBACK text_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_CLOSE){if(w==guideWindow)guideWindow=NULL;ShowWindow(w,SW_HIDE);return 0;}return DefWindowProcW(w,m,wp,lp);}
static void guide(HWND h){const wchar_t*t=L"E#+ CODING GUIDE\r\n\r\nprint words \"Hello!\"\r\nset name to \"Liam\"\r\nask user \"Name?\" and save answer as name\r\n\r\n2D:\r\n2d canvas\r\n2d button \"play\" \"PLAY\"\r\n\r\n3D:\r\nplayer create \"Player\"\r\n\r\nIMPORT:\r\nimport \"myImage.png\" as \"accountName\"";if(guideWindow){ShowWindow(guideWindow,SW_SHOW);return;}guideWindow=CreateWindowW(L"EPlusTextWindow",L"E#+ Guide",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,760,600,h,NULL,GetModuleHandleW(NULL),NULL);CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",t,WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY,10,10,720,530,guideWindow,NULL,0,0);}
static void apply_theme(HWND h);
static LRESULT CALLBACK settings_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_COMMAND){if(LOWORD(wp)==601){darkMode=1;apply_theme(mainWnd);}if(LOWORD(wp)==602){darkMode=0;apply_theme(mainWnd);}if(LOWORD(wp)==603){fontSize=16;make_font();}if(LOWORD(wp)==604){fontSize=18;make_font();}if(LOWORD(wp)==605){fontSize=21;make_font();}}if(m==WM_CLOSE){settingsWindow=NULL;ShowWindow(w,SW_HIDE);return 0;}return DefWindowProcW(w,m,wp,lp);}
static void show_settings(HWND h){if(settingsWindow){ShowWindow(settingsWindow,SW_SHOW);return;}WNDCLASSW c={0};c.lpfnWndProc=settings_proc;c.hInstance=GetModuleHandleW(NULL);c.lpszClassName=L"EPlusSettings";RegisterClassW(&c);settingsWindow=CreateWindowW(L"EPlusSettings",L"E#+ Settings",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,430,300,h,NULL,GetModuleHandleW(NULL),NULL);CreateWindowW(L"BUTTON",L"Dark Mode",WS_CHILD|WS_VISIBLE,30,35,160,40,settingsWindow,(HMENU)601,0,0);CreateWindowW(L"BUTTON",L"Light Mode",WS_CHILD|WS_VISIBLE,210,35,160,40,settingsWindow,(HMENU)602,0,0);CreateWindowW(L"BUTTON",L"Small Font",WS_CHILD|WS_VISIBLE,30,95,160,40,settingsWindow,(HMENU)603,0,0);CreateWindowW(L"BUTTON",L"Normal Font",WS_CHILD|WS_VISIBLE,210,95,160,40,settingsWindow,(HMENU)604,0,0);CreateWindowW(L"BUTTON",L"Large Font",WS_CHILD|WS_VISIBLE,30,155,160,40,settingsWindow,(HMENU)605,0,0);}
static void apply_theme(HWND h){colors();brushes();InvalidateRect(h,NULL,TRUE);InvalidateRect(editor,NULL,TRUE);InvalidateRect(console,NULL,TRUE);}
static LRESULT CALLBACK ext_window_proc(HWND w,UINT m,WPARAM wp,LPARAM lp){if(m==WM_COMMAND){if(LOWORD(wp)==303){ShowWindow(w,SW_HIDE);extWindow=NULL;}if(LOWORD(wp)==304||LOWORD(wp)==305)InvalidateRect(extCanvas,NULL,TRUE);}if(m==WM_CLOSE){ShowWindow(w,SW_HIDE);extWindow=NULL;}if(m==WM_SIZE&&extCanvas)MoveWindow(extCanvas,20,70,LOWORD(lp)-40,HIWORD(lp)-110,TRUE);return DefWindowProcW(w,m,wp,lp);}
static void show_native_extension(HWND h,int mode){if(guestMode){MessageBoxW(h,L"Extensions are disabled in Guest mode.",L"E#+ Guest Mode",MB_OK);return;}if(extWindow){ShowWindow(extWindow,SW_SHOW);return;}extMode=mode;extWindow=CreateWindowW(L"EPlusExtensionWindow",mode==1?L"E#+ 2D Editor":L"E#+ 3D Editor",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,980,680,h,NULL,GetModuleHandleW(NULL),NULL);CreateWindowW(L"BUTTON",L"New",WS_CHILD|WS_VISIBLE,20,20,100,36,extWindow,(HMENU)304,0,0);CreateWindowW(L"BUTTON",L"Clear",WS_CHILD|WS_VISIBLE,130,20,100,36,extWindow,(HMENU)305,0,0);CreateWindowW(L"BUTTON",L"Back to Code",WS_CHILD|WS_VISIBLE,240,20,130,36,extWindow,(HMENU)303,0,0);extCanvas=CreateWindowExW(WS_EX_CLIENTEDGE,L"EPlusExtensionCanvas",L"",WS_CHILD|WS_VISIBLE,20,70,920,530,extWindow,(HMENU)306,GetModuleHandleW(NULL),0);}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{colors();brushes();make_font();editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"# Welcome to E#+\r\nprint words \"Hello from E#+!\"\r\n",WS_CHILD|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL,10,60,500,500,h,(HMENU)ED,0,0);console=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"E#+ Console\r\n\r\n> Sign in or continue as Guest.\r\n",WS_CHILD|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL,520,60,500,500,h,(HMENU)CONSOLE_OUT,0,0);oldConsoleProc=(WNDPROC)SetWindowLongPtrW(console,GWLP_WNDPROC,(LONG_PTR)console_proc);const wchar_t*names[]={L"Run",L"New",L"Open",L"Save",L"Guide",L"Settings",L"Updates",L"Clear"};int ids[]={RUN,NEW,OPEN,SAVE,GUIDE,SETTINGS,CHECK_UPDATES,CLEAR};for(int i=0;i<8;i++){HWND b=CreateWindowW(L"BUTTON",names[i],WS_CHILD|WS_VISIBLE,10+i*105,12,98,34,h,(HMENU)(INT_PTR)ids[i],0,0);SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);}WNDCLASSW a={0};a.lpfnWndProc=auth_proc;a.hInstance=GetModuleHandleW(NULL);a.lpszClassName=L"EPlusAuth";RegisterClassW(&a);WNDCLASSW sg={0};sg.lpfnWndProc=signup_proc;sg.hInstance=GetModuleHandleW(NULL);sg.lpszClassName=L"EPlusSignup";RegisterClassW(&sg);WNDCLASSW ew={0};ew.lpfnWndProc=ext_window_proc;ew.hInstance=GetModuleHandleW(NULL);ew.lpszClassName=L"EPlusExtensionWindow";RegisterClassW(&ew);WNDCLASSW tw={0};tw.lpfnWndProc=text_proc;tw.hInstance=GetModuleHandleW(NULL);tw.lpszClassName=L"EPlusTextWindow";RegisterClassW(&tw);return 0;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{HDC dc=(HDC)w;SetTextColor(dc,textColor);SetBkColor(dc,inputColor);return (LRESULT)inputBrush;}
    case WM_ERASEBKGND:{HDC dc=(HDC)w;RECT r;GetClientRect(h,&r);FillRect(dc,&r,bgBrush);return 1;}
    case WM_SIZE:{int W=LOWORD(l),H=HIWORD(l);int left=(W-30)/2;MoveWindow(editor,10,60,left,H-70,TRUE);MoveWindow(console,left+20,60,W-left-30,H-70,TRUE);return 0;}
    case WM_APP_UPDATE_RESULT:{ UpdateInfo *u=(UpdateInfo*)l; if(u){ if(u->available && u->downloadUrl[0]){ wchar_t msg[512];swprintf_s(msg,512,L"E#+ Studio %s is available.\r\n\r\nUpdate now?",u->version);if(MessageBoxW(h,msg,L"E#+ Update Available",MB_YESNO|MB_ICONINFORMATION)==IDYES)install_update(h,u->downloadUrl); } free(u);} return 0;}
    case WM_COMMAND:switch(LOWORD(w)){case RUN:run_program();return 0;case NEW:set_text(editor,L"");return 0;case OPEN:file_dialog(0);return 0;case SAVE:save_cloud(h);return 0;case GUIDE:guide(h);return 0;case SETTINGS:show_settings(h);return 0;case CHECK_UPDATES:if(!guestMode)check_updates(h,0);return 0;case 301:show_native_extension(h,1);return 0;case 302:show_native_extension(h,2);return 0;case 303:if(extWindow)ShowWindow(extWindow,SW_HIDE);return 0;case CLEAR:set_text(console,L"E#+ Console\r\n");return 0;}break;
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
    register_extension_canvas(hi);ShowWindow(mainWnd,show);UpdateWindow(mainWnd);show_auth(mainWnd);MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return(int)msg.wParam;
}
