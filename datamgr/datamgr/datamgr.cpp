#pragma warning(disable:4996)
#include <Windows.h>
#include <ShlObj.h>
#include <atlbase.h>
#include <string>
#include <list>
#include <iostream>
#include <sys/stat.h>

///////////////////////////////////////////////////////////////////////////////
// Macros

#ifndef lengthof
   #define lengthof(x)  (sizeof(x)/sizeof(x[0]))
#endif  // lengthof

#ifndef offsetof
  #define offsetof(type, field)  ((int)&((type*)0)->field)
#endif  // offsetof

#ifndef IsBitSet
   #define IsBitSet(val, bit)  (((val)&(bit))!=0)
#endif // IsBitSet


#ifdef _DEBUG
   #define HR(expr)  { HRESULT _hr; if(FAILED(_hr=(expr))) { _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, #expr); _CrtDbgBreak(); return _hr; } }  
#else
   #define HR(expr)  { HRESULT _hr; if(FAILED(_hr=(expr))) return _hr; }
#endif // _DEBUG

#include "datamgr.h"
#include <tinystr.h>
#include <tinyxml.h>
#include "linkTree.h"
#include <json/json.h>
#include "Utility.h"

/*
// Json request
{
	Server: "127.0.0.1",
	Port: 60684,
	Version: "1.0.0.1", 
	Method: "SampleRequest",
	Param: {param1: value1, param2: value2, ...}
}

// Json response
{
	Ack: "SampleRequest",
	statusCode: 0,
	Status: "Code is 0, so everything is ok.",
	Return: {ret1: value1, ret2: value2, ...}
}
*/

///////////////////////////////////////////////////////////////////////////////
// Debug helper functions
static std::string WideStringToAnsi(const wchar_t * wstr){
	if (!wstr) return "";
	char szAnsi [MAX_PATH] = "";
	WideCharToMultiByte(CP_ACP, 0, wstr, wcslen(wstr), szAnsi, lengthof(szAnsi), NULL, NULL);
	return szAnsi;
}
static void OutputLog(const char * format, ...){
	va_list va;
	va_start(va, format);
	char szMsg [0x400] = "";
	_vsnprintf_s(szMsg, lengthof(szMsg), format, va);
	OutputDebugStringA(szMsg); OutputDebugStringA("\n");
}
#define WSTR2ASTR(w) (WideStringToAnsi(w).c_str())
#define OUTPUTLOG OutputLog

///////////////////////////////////////////////////////////////////////////////
// cache directory
#define VDRIVE_LOCAL_CACHE_ROOT ((pArchive && pArchive->context) \
								? (pArchive->context->cachedir) \
								: _T(""))

///////////////////////////////////////////////////////////////////////////////
// Global Context
static Edoc2Context * gspEdoc2Context = NULL;

///////////////////////////////////////////////////////////////////////////////
// TAR archive manipulation

/**
* Initialize 
*/
HRESULT DMInit(){
	HRESULT hr = E_FAIL;
	if (gspEdoc2Context) return S_OK;

	gspEdoc2Context = new Edoc2Context();
	memset(gspEdoc2Context, 0, sizeof(Edoc2Context));

	struct Edoc2Context & context = *gspEdoc2Context;

	// Setup locale.
	GetUserDefaultLocaleName(context.localeName, lengthof(context.localeName));

	// Setup local cache directory
	if (GetTempPathW(lengthof(context.cachedir), context.cachedir) <= 0){
		hr = AtlHresultFromLastError();
		goto bail;
	}
	wcscat_s(context.cachedir, lengthof(context.cachedir), _T("\\vdrivecache\\"));
	if (!PathFileExists(context.cachedir)){
		SHCreateDirectory(GetActiveWindow(), context.cachedir);
	}

	// Setup Service address
	wcscpy_s(context.service, lengthof(context.service), _T("http://192.168.253.242"));
	// Setup Username & passworld
	wcscpy_s(context.username, lengthof(context.username), _T("admin"));
	wcscpy_s(context.password, lengthof(context.password), _T("edoc2"));
	// Cleanup AccessToken
	context.AccessToken [0] = _T('\0');

	hr = S_OK;
bail:
	return hr;
}

HRESULT DMCleanup()
{
	if (gspEdoc2Context){
		delete gspEdoc2Context;
		gspEdoc2Context = NULL;
	}
	return S_OK;
}

/**
 * Opens an existing .tar archive.
 * Here we basically memorize the filename of the .tar achive, and we only
 * open the file later on when it is actually needed the first time.
 */
HRESULT DMOpen(LPCWSTR pwstrFilename, TAR_ARCHIVE** ppArchive)
{
   HRESULT hr = E_FAIL;
   TAR_ARCHIVE* pArchive = new TAR_ARCHIVE();
   if( pArchive == NULL ) return E_OUTOFMEMORY;
   // Retrieve the session context ptr.
   pArchive->context = gspEdoc2Context;
   *ppArchive = pArchive;
   return S_OK;
bail:
   if (pArchive != NULL) delete pArchive;
   *ppArchive = NULL;
   return hr;
}

/**
 * Close the archive.
 */
HRESULT DMClose(TAR_ARCHIVE* pArchive)
{
   if( pArchive == NULL ) return E_INVALIDARG;
   delete pArchive;
   return S_OK;
}

/**
 * Return the list of children of a sub-folder.
 */
HRESULT DMGetChildrenList(TAR_ARCHIVE* pArchive, RemoteId dwId, RFS_FIND_DATA ** retList, int * nListCount)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);
   OUTPUTLOG("%s(), pwstrPath=[%d]", __FUNCTION__, (dwId));
   *retList = NULL; *nListCount = 0;
   std::list<RFS_FIND_DATA> tmpList;

   // HarryWu, 2014.2.18
   // Test code.
   // 1) get sub folders for Enterprise Level
   // "http://192.168.253.242/EDoc2WebApi/api/Doc/FolderRead/GetTopPublicFolder?token=2ef01717-6f61-4592-a606-7292f3cb5a57"
   // 2) get sub foders for Personal Level
   // "http://192.168.253.242/EDoc2WebApi/api/Doc/FolderRead/GetTopPersonalFolder?token=2ef01717-6f61-4592-a606-7292f3cb5a57"
   // 3) get sub folder by id.
   // "http://192.168.253.242/EDoc2WebApi/api/Doc/FolderRead/GetChildFolderListByFolderId?token=2ef01717-6f61-4592-a606-7292f3cb5a57&folderId=16086"
   // 4) get sub files by id
   // "http://192.168.253.242/EDoc2WebApi/api/Doc/FileRead/GetChildFileListByFolderId?token=c8132990-f885-440f-9c5b-88fa685d2482&folderId=16415"

   if (dwId.category == VdriveCat && dwId.id == VdriveId){
	   std::list<RFS_FIND_DATA> publicList;
	   Utility::GetTopPublic(pArchive, publicList);
	   tmpList.merge(publicList, Utility::RfsComparation);
	   std::list<RFS_FIND_DATA> personalList;
	   Utility::GetTopPersonal(pArchive, personalList);
	   tmpList.merge(personalList, Utility::RfsComparation);
	   RFS_FIND_DATA recycleBin;
	   Utility::ConstructRecycleFolder(pArchive, recycleBin);
	   tmpList.push_back(recycleBin);
	   RFS_FIND_DATA searchBin;
	   Utility::ConstructSearchFolder(pArchive, searchBin);
	   tmpList.push_back(searchBin);
   }else if (dwId.category == RecycleCat){

   }else if (dwId.category == SearchCat){

   }else if (dwId.category == PublicCat || dwId.category == PersonCat){
	   std::list<RFS_FIND_DATA> childFolders;
	   Utility::GetChildFolders(pArchive, dwId, childFolders);
	   tmpList.merge(childFolders, Utility::RfsComparation);
	   std::list<RFS_FIND_DATA> childFiles;
	   Utility::GetChildFiles(pArchive, dwId, childFiles);
	   tmpList.merge(childFiles, Utility::RfsComparation);
   }else{
	   OUTPUTLOG("Invalid RemoteId{%d,%d}", dwId.category, dwId.id);
	   return E_INVALIDARG;
   }

   if (tmpList.size() == 0) return S_OK;

   if (S_OK != DMMalloc((LPBYTE *)retList, tmpList.size() * sizeof(RFS_FIND_DATA))){
	   return E_OUTOFMEMORY;
   }
   RFS_FIND_DATA *aList = (RFS_FIND_DATA *)(*retList);

   int index = 0;
   for(std::list<RFS_FIND_DATA>::iterator it = tmpList.begin(); 
	   it != tmpList.end(); it ++){ aList [index] = *it;
		// refine the attributes.
		RFS_FIND_DATA * pData = &aList[index];
		pData->dwFileAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;   
		pData->dwFileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
		if (!IsBitSet(pData->dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY))
			pData->dwFileAttributes |= FILE_ATTRIBUTE_VIRTUAL;
		index ++;
   }
   *nListCount = tmpList.size();
   return S_OK;
}

/**
 * Rename a file or folder in the archive.
 * Notice that we do not support rename of a folder, which contains files, yet.
 */
HRESULT DMRename(TAR_ARCHIVE* pArchive, LPCWSTR pwstrFilename, LPCWSTR pwstrNewName)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);

   if (NULL == pwstrFilename || NULL == pwstrNewName) return E_INVALIDARG;

   OUTPUTLOG("%s(), pwstrFilename=[%s], pwstrNewName=[%s]", __FUNCTION__, WSTR2ASTR(pwstrFilename), WSTR2ASTR(pwstrNewName));

   { // Rename local link.
	   std::wstring sourcePath = VDRIVE_LOCAL_CACHE_ROOT;
	   sourcePath += pwstrFilename;
	   wchar_t szNew [MAX_PATH] = _T(""); wcscpy_s(szNew, lengthof(szNew), sourcePath.c_str());
	   wcsrchr(szNew, _T('\\'))[1] = 0;
	   wcscat_s(szNew, lengthof(szNew), pwstrNewName);
	   ::MoveFile(sourcePath.c_str(), szNew);
   }

   { // Rename File/Folder on remote

   }
   return S_OK;
}

/**
 * Delete a file or folder.
 */
HRESULT DMDelete(TAR_ARCHIVE* pArchive, const RFS_FIND_DATA * pWfd)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);

   LPCWSTR pwstrFilename = pWfd->cFileName;

   if (NULL == pwstrFilename) return E_INVALIDARG;

   OUTPUTLOG("%s(), pwstrFilename=[%s]", __FUNCTION__, WSTR2ASTR(pwstrFilename));
   
   {// Delete local link
	   std::wstring linkpath = VDRIVE_LOCAL_CACHE_ROOT;
	   linkpath += pwstrFilename;
	   
	   // HarryWu, 2014.2.1
	   // Notice, we must construct a double-null-end string to specify the full path 
	   // to delete.
	   wchar_t szFullPath [MAX_PATH] = _T(""); memset(szFullPath, 0, sizeof(szFullPath));
	   wcscpy_s(szFullPath, lengthof(szFullPath) - 1, linkpath.c_str());

	   SHFILEOPSTRUCT shfop; memset(&shfop, 0, sizeof(shfop));
	   shfop.hwnd = ::GetActiveWindow();
	   shfop.wFunc = FO_DELETE;
	   shfop.fFlags = FOF_NO_UI;
	   shfop.pFrom = szFullPath;
	   shfop.pTo = NULL;
	   SHFileOperation(&shfop);
   }

   {// Delete remote File/Folder
	   Utility::DeleteItem(pArchive, pWfd);
   }
   return S_OK;
}

/**
 * Create a new sub-folder in the archive.
 */
HRESULT DMCreateFolder(TAR_ARCHIVE* pArchive, LPCWSTR pwstrFilename)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);
   
   if (NULL == pwstrFilename ) return E_INVALIDARG;

   OUTPUTLOG("%s(), pwstrFilename=[%s]", __FUNCTION__, WSTR2ASTR(pwstrFilename));

   {// Write link info for this foler.
	   std::wstring linkpath = VDRIVE_LOCAL_CACHE_ROOT;
	   linkpath += pwstrFilename;
	   if (!PathFileExists(linkpath.c_str())){
		   SHCreateDirectory(GetActiveWindow(), linkpath.c_str());
	   }
	   Link link = {0};
	   link.dwType = FolderLink;
	   link.dwId = rand();
	   linkTree::WriteLink(linkpath.c_str(), &link, TRUE);
   }

   {// Create Folder On remote

   }

   return S_OK;
}

/**
* Return file information.
* Convert the archive file information to a Windows WIN32_FIND_DATA structure, which
* contains the basic information we must know about a virtual file/folder.
*/
HRESULT DMGetFileAttr(TAR_ARCHIVE* pArchive, LPCWSTR pstrFilename, RFS_FIND_DATA* pData)
{
	CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);

	if (NULL == pstrFilename) return E_INVALIDARG;

	std::wstring fullpath  = VDRIVE_LOCAL_CACHE_ROOT;
	fullpath += pstrFilename;

	if (!PathFileExists(fullpath.c_str())){
		OUTPUTLOG("%s(), pwstrPath=[%s], return ERROR_FILE_NOT_FOUND.", __FUNCTION__, WSTR2ASTR(pstrFilename));
		return AtlHresultFromWin32(ERROR_FILE_NOT_FOUND);
	}

	WIN32_FIND_DATA tempWfd = {0};
	HANDLE hFind = FindFirstFile(fullpath.c_str(), &tempWfd);
	if (INVALID_HANDLE_VALUE  == hFind){
		OUTPUTLOG("%s(), pwstrPath=[%s], return ERROR_FILE_NOT_FOUND.", __FUNCTION__, WSTR2ASTR(pstrFilename));
		return AtlHresultFromWin32(ERROR_FILE_NOT_FOUND);
	}
	// Attention, if not closed, the file/folder will be locked!
	FindClose(hFind);

	*pData = *(RFS_FIND_DATA *)&tempWfd;

	// refine the attributes.
	pData->dwFileAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;   
	pData->dwFileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
	if (!IsBitSet(pData->dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY))
		pData->dwFileAttributes |= FILE_ATTRIBUTE_VIRTUAL;

	Link link = {0};
	linkTree::ReadLink(fullpath.c_str(), &link, IsBitSet(pData->dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY));

	pData->nFileSizeLow = link.dwFileSize;

	OUTPUTLOG("%s(), pwstrPath=[%s], return Attr=0x%08x.", __FUNCTION__, WSTR2ASTR(pstrFilename), pData->dwFileAttributes);

	return S_OK;
}

/**
 * Change the file-attributes of a file.
 */
HRESULT DMSetFileAttr(TAR_ARCHIVE* pArchive, LPCWSTR pwstrFilename, DWORD dwAttributes)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);
   
   if (NULL == pwstrFilename) return E_INVALIDARG;

   OUTPUTLOG("%s(), pwstrFilename=[%s]", __FUNCTION__, WSTR2ASTR(pwstrFilename));

   std::wstring linkpath = VDRIVE_LOCAL_CACHE_ROOT;
   linkpath += pwstrFilename;

   if (!PathFileExists(linkpath.c_str()))
	   return AtlHresultFromLastError();

   if (!SetFileAttributes(linkpath.c_str(), dwAttributes))
	   return AtlHresultFromLastError();

   return S_OK;
}

/**
 * Create a new file in the archive.
 * This method expects a memory buffer containing the entire file contents.
 * HarryWu, 2014.2.2
 * Notice! an empty file can NOT be created here!
 */
HRESULT DMWriteFile(TAR_ARCHIVE* pArchive, LPCWSTR pwstrFilename, const LPBYTE pbBuffer, DWORD dwFileSize, DWORD dwAttributes)
{
   CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);
   
   if (NULL == pwstrFilename || !pbBuffer || !dwFileSize) return E_INVALIDARG;

   OUTPUTLOG("%s(), pwstrFilename=[%s], dwFileSize=[%d]", __FUNCTION__, WSTR2ASTR(pwstrFilename), dwFileSize);

   {// Write to link.
	   Link link = {0}; 
	   link.dwFileSize = dwFileSize;
	   link.dwId = rand();
	   link.dwVersion = 0x00000001;

	   std::wstring linkpath = VDRIVE_LOCAL_CACHE_ROOT;
	   linkpath += pwstrFilename;

	   linkTree::WriteLink(linkpath.c_str(), &link, FALSE);
   }
   // HarryWu, 2014.2.15
   // TODO: Post file content to server
   // pbBuffer, dwFileSize
   {

   }

   return S_OK;
}

/**
 * Reads a file from the archive.
 * The method returns the contents of the entire file in a memory buffer.
 */
HRESULT DMReadFile(TAR_ARCHIVE* pArchive, LPCWSTR pwstrFilename, LPBYTE* ppbBuffer, DWORD* pdwFileSize)
{
	CComCritSecLock<CComCriticalSection> lock(pArchive->csLock);

	if (NULL == pwstrFilename ) return E_INVALIDARG;

	OUTPUTLOG("%s(), pwstrFilename=[%s]", __FUNCTION__, WSTR2ASTR(pwstrFilename));

	*ppbBuffer = NULL; *pdwFileSize = 0;

	Link link = {0};
	{
		std::wstring linkpath = VDRIVE_LOCAL_CACHE_ROOT;
		linkpath += pwstrFilename;
		if (!linkTree::ReadLink(linkpath.c_str(), &link, FALSE))
			return AtlHresultFromLastError();
	}

	// HarryWu, 2014.2.15
	// TODO: Read file contents from remote.
	// ...
	{
		DMMalloc(ppbBuffer, link.dwFileSize);
		if (ppbBuffer == NULL) return E_OUTOFMEMORY;
		memset(*ppbBuffer, 'E', link.dwFileSize);
		*pdwFileSize = link.dwFileSize;
	}
	return S_OK;
}

HRESULT DMMalloc(LPBYTE * ppBuffer, DWORD dwBufSize)
{
	*ppBuffer = (LPBYTE)malloc(dwBufSize);
	if (*ppBuffer != NULL)
		return S_OK;
	else 
		return E_OUTOFMEMORY;
}

HRESULT DMRealloc(LPBYTE * ppBuffer, DWORD dwBufSize)
{
	*ppBuffer = (LPBYTE)realloc(*ppBuffer, dwBufSize);
	if (*ppBuffer != NULL)
		return S_OK;
	else 
		return E_OUTOFMEMORY;
}

HRESULT DMFree(LPBYTE pBuffer)
{
	free(pBuffer);
	return S_OK;
}

