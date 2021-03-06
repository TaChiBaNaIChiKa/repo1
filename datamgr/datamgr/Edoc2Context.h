#ifndef _EDOC2_CONTEXT_H__
#define _EDOC2_CONTEXT_H__

class Proto;

struct Edoc2Context {
    DWORD   dwUserId;
    BOOL    enableHttp;
    BOOL    enableHttpTransfer;
    DWORD   HttpTimeoutMs;
    DWORD   ViewPort;
    DWORD   JsonPort;
    DWORD   pageSize;
    BOOL    fastCheck;
    DWORD   dwRootMask;
    BOOL    fEnableSearchBar;
    DWORD   dwTransEncoding;
    HINSTANCE hInst;
    Proto*  proto;
    wchar_t modulepath    [1024];
    wchar_t configfile    [1024];
    wchar_t localeName    [32];
    wchar_t service       [128];
    wchar_t username      [32];
    wchar_t password      [32];
    wchar_t AccessToken   [128];
    wchar_t cachedir      [1024];
};
#endif