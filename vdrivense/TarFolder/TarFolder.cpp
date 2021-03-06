// TarFolder.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"


//////////////////////////////////////////////////////////////////////
//

CAtlDllModule _AtlModule;
CTarShellModule _ShellModule;


// DEBUGGING NOTES:
//  Remember to register the DLL using the regsvr32 utility or similar.
//  To debug in MS Visual Studio, kill all running Explorer.exe processes and set the
//  following debug settings for the project:
//   Command:                    C:\windows\explorer.exe


// These are the GUIDs that identify the TarFolder shell junction and friends.
// Make sure to choose unique GUIDs for every project.
const CLSID CLSID_ShellFolder     =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x54}};
const CLSID CLSID_SendTo          =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x55}};
const CLSID CLSID_Preview         =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x56}};
const CLSID CLSID_DropTarget      =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x57}};
const CLSID CLSID_ContextMenu     =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x58}};
const CLSID CLSID_PropertySheet   =     {0x2C3256E4,0x49AA,0x11D3,{0x82,0x29,0x00,0x50,0xAE,0x50,0x90,0x59}};

