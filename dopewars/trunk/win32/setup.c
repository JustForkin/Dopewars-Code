/* setup.c        Simple Win32 installer for dopewars                   */
/* Copyright (C)  2001  Ben Webb                                        */
/*                Email: ben@bellatrix.pcl.ox.ac.uk                     */
/*                WWW: http://dopewars.sourceforge.net/                 */

/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU General Public License          */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */

/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */

/* You should have received a copy of the GNU General Public License    */
/* along with this program; if not, write to the Free Software          */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston,               */
/*                   MA  02111-1307, USA.                               */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib/zlib.h"
#include <shlobj.h>

#include "contid.h"
#include "guifunc.h"
#include "util.h"

typedef enum {
  DL_INTRO=0, DL_LICENCE,DL_SHORTCUTS,DL_INSTALLDIR,DL_DOINSTALL,
  DL_NUM
} DialogType;

InstData *idata;
HWND mainDlg[DL_NUM];
DialogType CurrentDialog;
HINSTANCE hInst=NULL;

DWORD WINAPI DoInstall(LPVOID lpParam);
static void GetWinText(char **text,HWND hWnd);

void InstallService(InstData *idata) {
  SC_HANDLE scManager,scService;
  HKEY key;
  bstr *str;
  static char keyprefix[] = "SYSTEM\\ControlSet001\\Services\\";
  NTService *service;

  service = idata->service;
  if (!service) return;

  scManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

  if (!scManager) {
    DisplayError("Cannot connect to service manager",TRUE,FALSE);
    return;
  }

  str = bstr_new();
  bstr_assign(str,idata->installdir);
  bstr_appendpath(str,service->exe);

  scService = CreateService(scManager,service->name,service->display,
                            SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
                            str->text,NULL,NULL,NULL,NULL,NULL);
  if (!scService) {
    DisplayError("Cannot create service",TRUE,FALSE);
    bstr_free(str,TRUE);
    return;
  }

  bstr_assign(str,keyprefix);
  bstr_append(str,service->name);
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,str->text,0,KEY_WRITE,&key)
      ==ERROR_SUCCESS) {
    RegSetValueEx(key,"Description",0,REG_SZ,service->description,
                  strlen(service->description));
    RegCloseKey(key);
  }

  CloseServiceHandle(scService);
  CloseServiceHandle(scManager);
}

BOOL CheckCreateDir(void) {
/* Checks that the install directory exists, and creates it if it does not.
   Returns TRUE if the directory is OK. */

  char *instdir;
  GetWinText(&idata->installdir,GetDlgItem(mainDlg[DL_INSTALLDIR],ED_INSTDIR));
  instdir=idata->installdir;
  if (SetCurrentDirectory(instdir)) {
    return TRUE;
  } else {
    if (MessageBox(mainDlg[CurrentDialog],
                   "The install directory does not exist.\n"
                   "Create it?","Install Directory",MB_YESNO)==IDYES) {
      if (CreateWholeDirectory(instdir)) {
        return TRUE;
      } else {
        DisplayError("Could not create directory",FALSE,FALSE);
      }
    }
    return FALSE;
  }
}

void ShowNewDialog(DialogType NewDialog) {
  RECT DeskRect,OurRect;
  int newX,newY;
  HWND hWnd;
  HANDLE hThread;
  DWORD threadID;
  if (NewDialog<0 || NewDialog>=DL_NUM) return;

  if (NewDialog > CurrentDialog && CurrentDialog==DL_INSTALLDIR) {
    if (!CheckCreateDir()) return;
  }

  hWnd=mainDlg[NewDialog];
  if (GetWindowRect(hWnd,&OurRect) &&
      GetWindowRect(GetDesktopWindow(),&DeskRect)) {
    newX = (DeskRect.left+DeskRect.right+OurRect.left-OurRect.right)/2;
    newY = (DeskRect.top+DeskRect.bottom+OurRect.top-OurRect.bottom)/2;
    SetWindowPos(hWnd,HWND_TOP,newX,newY,0,0,SWP_NOSIZE);
  }
  ShowWindow(hWnd,SW_SHOW);

  if (CurrentDialog!=DL_NUM) ShowWindow(mainDlg[CurrentDialog],SW_HIDE);
  CurrentDialog=NewDialog;

  if (NewDialog==DL_DOINSTALL) {
    hThread = CreateThread(NULL,0,DoInstall,NULL,0,&threadID);
  }
}

int CALLBACK BrowseCallback(HWND hwnd,UINT msg,LPARAM lParam,LPARAM lpData) {
  switch(msg) {
    case BFFM_INITIALIZED:
      SendMessage(hwnd,BFFM_SETSELECTION,TRUE,(LPARAM)idata->installdir);
      break;
  }
  return 0;
}

void SelectInstDir(HWND parent) {
  BROWSEINFO bi = { 0 };
  TCHAR path[MAX_PATH];
  LPITEMIDLIST pidl;
  IMalloc *imalloc=0;

  if (SUCCEEDED(SHGetMalloc(&imalloc))) {
    bi.lpszTitle = "Pick a directory";
    bi.pszDisplayName = path;
    bi.ulFlags = BIF_STATUSTEXT|BIF_RETURNONLYFSDIRS;
    bi.lpfn = BrowseCallback;
    pidl = SHBrowseForFolder(&bi);
    if (pidl) {
      if (SHGetPathFromIDList(pidl,path)) {
        bfree(idata->installdir);
        idata->installdir = bstrdup(path);
        SendDlgItemMessage(mainDlg[DL_INSTALLDIR],ED_INSTDIR,WM_SETTEXT,
                           0,(LPARAM)idata->installdir);
      }
      imalloc->lpVtbl->Free(imalloc,pidl);
    }
    imalloc->lpVtbl->Release(imalloc);
  }
}

void ConditionalExit(HWND hWnd) {
  if (MessageBox(hWnd,"This will exit without installing any new software on "
                 "your machine.\nAre you sure you want to quit?\n\n(You can "
                 "continue the installation at a\nlater date simply by "
                 "running this program again.)","Exit Install",
                 MB_YESNO|MB_ICONQUESTION)==IDYES) {
    PostQuitMessage(0);
  }
}

void UpdateStartMenuFolder(void) {
  char *buf;
  HWND folderlist;
  LRESULT lres;
  int selind;

  folderlist = GetDlgItem(mainDlg[DL_SHORTCUTS],LB_FOLDLIST);
  if (!folderlist) return;

  lres=SendMessage(folderlist,LB_GETCURSEL,0,0);
  if (lres==LB_ERR) return;

  selind=(int)lres;
  lres=SendMessage(folderlist,LB_GETTEXTLEN,(WPARAM)selind,0);
  if (lres==LB_ERR) return;

  buf = bmalloc(lres+1);
  lres=SendMessage(folderlist,LB_GETTEXT,(WPARAM)selind,(LPARAM)buf);
  if (lres!=LB_ERR) {
    SendDlgItemMessage(mainDlg[DL_SHORTCUTS],ED_FOLDER,WM_SETTEXT,
                       0,(LPARAM)buf);
  }
  bfree(buf);
}

BOOL CALLBACK MainDlgProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch(msg) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      if (HIWORD(wParam)==BN_CLICKED && lParam) {
        switch(LOWORD(wParam)) {
          case BT_CANCEL: ConditionalExit(hWnd); break;
          case BT_NEXT:   ShowNewDialog(CurrentDialog+1); break;
          case BT_BACK:   ShowNewDialog(CurrentDialog-1); break;
          case BT_FINISH: PostQuitMessage(0); break;
          case BT_BROWSE: SelectInstDir(hWnd); break;
        }
      } else if (HIWORD(wParam)==LBN_SELCHANGE && lParam &&
                 LOWORD(wParam)==LB_FOLDLIST) {
        UpdateStartMenuFolder();
      }
      break;
    case WM_CLOSE:
      ConditionalExit(hWnd);
      return TRUE;
  }
  return FALSE;
}

LPVOID GetResource(LPCTSTR resname,LPCTSTR restype) {
  HRSRC hrsrc;
  HGLOBAL hglobal;
  LPVOID respt;

  hrsrc = FindResource(NULL,resname,restype);
  if (!hrsrc) DisplayError("Could not find resource: ",TRUE,TRUE);

  hglobal = LoadResource(NULL,hrsrc);
  if (!hglobal) DisplayError("Could not load resource: ",TRUE,TRUE);

  respt = LockResource(hglobal);
  if (!respt) DisplayError("Could not lock resource: ",TRUE,TRUE);

  return respt;
}

InstData *ReadInstData() {
  InstFiles *lastinst=NULL,*lastextra=NULL;
  InstLink *lastmenu=NULL,*lastdesktop=NULL;
  char *instdata,*pt,*filename,*line2,*line3,*line4;
  DWORD filesize;
  InstData *idata;

  instdata = GetResource(MAKEINTRESOURCE(0),"INSTLIST");
  if (!instdata) return NULL;

  pt=instdata;

  idata = bmalloc(sizeof(InstData));
  idata->service = NULL;
  idata->totalsize = atol(pt);
  pt += strlen(pt)+1;

  idata->product = bstrdup(pt);
  pt += strlen(pt)+1;

  idata->installdir = bstrdup(pt);
  pt += strlen(pt)+1;

  idata->startmenudir = bstrdup(pt);
  pt += strlen(pt)+1;

  while (1) {
    filename=pt;
    pt += strlen(pt)+1;
    if (filename[0]) {
      filesize=atol(pt);
      pt += strlen(pt)+1;
      AddInstFiles(filename,filesize,&lastinst,&idata->instfiles);
    } else break;
  }
  while (1) {
    filename=pt;
    pt += strlen(pt)+1;
    if (filename[0]) {
      filesize=atol(pt);
      pt += strlen(pt)+1;
      AddInstFiles(filename,filesize,&lastextra,&idata->extrafiles);
    } else break;
  }
  while (1) {
    filename=pt;
    pt += strlen(pt)+1;
    if (filename[0]) {
      line2=pt; pt += strlen(pt)+1;
      line3=pt; pt += strlen(pt)+1;
      AddInstLink(filename,line2,line3,&lastmenu,&idata->startmenu);
    } else break;
  }
  while (1) {
    filename=pt;
    pt += strlen(pt)+1;
    if (filename[0]) {
      line2=pt; pt += strlen(pt)+1;
      line3=pt; pt += strlen(pt)+1;
      AddInstLink(filename,line2,line3,&lastdesktop,&idata->desktop);
    } else break;
  }
  filename=pt; pt += strlen(pt)+1;
  if (filename[0]) {
    line2=pt; pt += strlen(pt)+1;
    line3=pt; pt += strlen(pt)+1;
    line4=pt; pt += strlen(pt)+1;
    AddServiceDetails(filename,line2,line3,line4,&idata->service);
  }

  return idata;
}

#define BUFFER_SIZE (32*1024)

char *GetFirstFile(InstFiles *filelist,DWORD totalsize) {
  DWORD bufsiz;
  char *inbuf,*outbuf;
  int status;
  z_stream z;

  if (!filelist) return NULL;

  inbuf = GetResource(MAKEINTRESOURCE(1),"INSTFILE");
  if (!inbuf) return NULL;

  bufsiz = filelist->filesize;
  outbuf = bmalloc(bufsiz+1);

  z.zalloc = Z_NULL;
  z.zfree = Z_NULL;
  z.opaque = Z_NULL;
  z.next_in = inbuf;
  z.avail_in = totalsize;

  inflateInit(&z);
  z.next_out = outbuf;
  z.avail_out = bufsiz;

  while (1) {
    status = inflate(&z,Z_SYNC_FLUSH);
    if ((status!=Z_OK && status!=Z_STREAM_END) || z.avail_out==0) break;
  }
  inflateEnd(&z);

  outbuf[bufsiz]='\0';
  return outbuf;
}

BOOL OpenNextOutput(HANDLE *fout,InstFiles *filelist,InstFiles **listpt,
                    DWORD *fileleft,HANDLE logf) {
  char *filename,*sep;
  bstr *str;
  DWORD bytes_written;

  if (*fout) CloseHandle(*fout);
  *fout = INVALID_HANDLE_VALUE;

  str=bstr_new();

  if (*listpt) {
    if (!WriteFile(logf,(*listpt)->filename,strlen((*listpt)->filename)+1,
                   &bytes_written,NULL)) {
      printf("Write error\n");
    }

    bstr_setlength(str,0);
    bstr_append_long(str,(*listpt)->filesize);
    if (!WriteFile(logf,str->text,str->length+1,&bytes_written,NULL)) {
      printf("Write error\n");
    }
    *listpt=(*listpt)->next;
  } else *listpt = filelist;

  if (*listpt) {
    filename = (*listpt)->filename;
    sep = strrchr(filename,'\\');
    if (sep) {
      *sep = '\0';
      CreateWholeDirectory(filename);
      *sep = '\\';
    }
    *fout = CreateFile(filename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    *fileleft = (*listpt)->filesize;
    bstr_assign(str,"Installing file: ");
    bstr_append(str,filename);
    bstr_append(str," (size ");
    bstr_append_long(str,(*listpt)->filesize);
    bstr_append(str,")");
    SendDlgItemMessage(mainDlg[DL_DOINSTALL],ST_FILELIST,
                       WM_SETTEXT,0,(LPARAM)str->text);
    if (*fout==INVALID_HANDLE_VALUE) {
      bstr_assign(str,"Cannot create file ");
      bstr_append(str,filename);
      DisplayError(str->text,TRUE,FALSE);
    }
  }

  bstr_free(str,TRUE);

  return (*fout!=INVALID_HANDLE_VALUE);
}

HRESULT CreateLink(LPCSTR origPath,LPSTR linkArgs,LPSTR workDir,
                   LPSTR linkPath,LPSTR linkDesc) {
  HRESULT hres;
  IShellLink *psl;
  IPersistFile *ppf;
  WORD wsz[MAX_PATH];

  hres = CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,
                          &IID_IShellLink,(void **)&psl);
  if (SUCCEEDED(hres)) {
    psl->lpVtbl->SetPath(psl,origPath);
    if (workDir) psl->lpVtbl->SetWorkingDirectory(psl,workDir);
    if (linkDesc) psl->lpVtbl->SetDescription(psl,linkDesc);
    if (linkArgs) psl->lpVtbl->SetArguments(psl,linkArgs);
    hres = psl->lpVtbl->QueryInterface(psl,&IID_IPersistFile,(void **)&ppf);
    if (SUCCEEDED(hres)) {
      MultiByteToWideChar(CP_ACP,0,linkPath,-1,wsz,MAX_PATH);
      hres = ppf->lpVtbl->Save(ppf,wsz,TRUE);
      ppf->lpVtbl->Release(ppf);
    } else {
      DisplayError("Cannot write shortcut",FALSE,FALSE);
    }
    psl->lpVtbl->Release(psl);
  } else {
    DisplayError("Cannot create shortcut",FALSE,FALSE);
  }
  return hres;
}

void GetWinText(char **text,HWND hWnd) {
  int textlen;
  bfree(*text);
  *text=NULL;
  if (!hWnd) return;

  textlen=GetWindowTextLength(hWnd)+1;
  if (!textlen) return;

  *text=bmalloc(textlen);
  if (!GetWindowText(hWnd,*text,textlen)) {
    bfree(*text); *text=NULL;
  }
}

void CreateLinks(char *linkdir,InstLink *linkpt) {
  bstr *linkpath,*origfile;

  linkpath=bstr_new();
  origfile=bstr_new();

  for (;linkpt;linkpt=linkpt->next) {
    bstr_assign(linkpath,linkdir);
    bstr_appendpath(linkpath,linkpt->linkfile);

    bstr_assign(origfile,idata->installdir);
    bstr_appendpath(origfile,linkpt->origfile);

    CreateLink(origfile->text,linkpt->args,idata->installdir,
               linkpath->text,NULL);
  }
}

void SetupShortcuts(HANDLE fout) {
  char *startmenu,*desktop;
  BOOL dodesktop;

  startmenu = GetStartMenuDir(idata);
  desktop = GetDesktopDir();

  dodesktop=(IsDlgButtonChecked(mainDlg[DL_SHORTCUTS],CB_DESKTOP)==BST_CHECKED);

  if (startmenu) {
    CreateDirectory(startmenu,NULL);

    CreateLinks(startmenu,idata->startmenu);
    WriteLinkList(fout,idata->startmenu);
  } else {
    WriteLinkList(fout,NULL);
  }

  if (dodesktop && desktop) {
    CreateLinks(desktop,idata->desktop);
    WriteLinkList(fout,idata->desktop);
  } else {
    WriteLinkList(fout,NULL);
  }

  bfree(startmenu); bfree(desktop);
}

void SetupUninstall() {
  HKEY key;
  DWORD disp;
  bstr *str,*uninstexe,*link;
  BOOL haveuninstall=FALSE;
  char *startmenu;
  InstFiles *listpt;

  for (listpt=idata->instfiles;listpt;listpt=listpt->next) {
    if (strcmp(listpt->filename,"uninstall.exe")==0) {
      haveuninstall=TRUE;
      break;
    }
  }

  if (!haveuninstall) return;

  str=bstr_new();
  uninstexe=bstr_new();
  link=bstr_new();

  bstr_assign(str,UninstallKey);
  bstr_appendpath(str,idata->product);
  
  if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,str->text,0,NULL,0,
                     KEY_ALL_ACCESS,NULL,&key,&disp)==ERROR_SUCCESS) {
    RegSetValueEx(key,"DisplayName",0,REG_SZ,idata->product,
                  strlen(idata->product));
    bstr_assign_windir(str);
    bstr_appendpath(str,UninstallEXE);
    bstr_append_c(str,' ');
    bstr_append(str,idata->product);
    RegSetValueEx(key,"UninstallString",0,REG_SZ,str->text,str->length);
    bstr_assign(str,idata->installdir);
    RegSetValueEx(key,"InstallDirectory",0,REG_SZ,str->text,str->length);
    RegCloseKey(key);
  } else {
    DisplayError("Cannot create registry key: ",TRUE,FALSE);
  }

  bstr_assign_windir(str);
  bstr_appendpath(str,"bw-uninstall.exe");
  DeleteFile(str->text);

  bstr_assign(uninstexe,idata->installdir);
  bstr_appendpath(uninstexe,"uninstall.exe");

  if (!MoveFile(uninstexe->text,str->text)) {
    DisplayError("Unable to create uninstall program: ",TRUE,FALSE);
  }
  DeleteFile(uninstexe->text);

  startmenu = GetStartMenuDir(idata);
  bstr_assign(link,startmenu);
  bstr_appendpath(link,"Uninstall ");
  bstr_append(link,idata->product);
  bstr_append(link,".LNK");

  CreateLink(str->text,idata->product,NULL,link->text,NULL);

  bstr_free(link,TRUE);
  bstr_free(uninstexe,TRUE);
  bstr_free(str,TRUE);
  bfree(startmenu);
}

DWORD WINAPI DoInstall(LPVOID lpParam) {
  HANDLE fout,logf;
  DWORD bytes_written,fileleft;
  char *inbuf,*outbuf;
  int status,count;
  z_stream z;
  InstFiles *listpt;

  inbuf = GetResource(MAKEINTRESOURCE(1),"INSTFILE");
  if (!inbuf) return 0;

  GetWinText(&idata->startmenudir,GetDlgItem(mainDlg[DL_SHORTCUTS],ED_FOLDER));

  if (!SetCurrentDirectory(idata->installdir)) {
    DisplayError("Cannot access install directory",TRUE,TRUE);
  }

  logf = CreateFile("install.log",GENERIC_WRITE,0,NULL,
                    CREATE_ALWAYS,0,NULL);

  if (!WriteFile(logf,idata->startmenudir,strlen(idata->startmenudir)+1,
                 &bytes_written,NULL)) {
    printf("Write error\n");
  }

  fout = INVALID_HANDLE_VALUE;
  listpt=NULL;
  OpenNextOutput(&fout,idata->instfiles,&listpt,&fileleft,logf);

  outbuf = bmalloc(BUFFER_SIZE);

  z.zalloc = Z_NULL;
  z.zfree = Z_NULL;
  z.opaque = Z_NULL;
  z.next_in = inbuf;
  z.avail_in = idata->totalsize;

  inflateInit(&z);
  z.next_out = outbuf;
  z.avail_out = BUFFER_SIZE;

  while (1) {
    status = inflate(&z,Z_SYNC_FLUSH);
    if (status==Z_OK || status==Z_STREAM_END) {
      count = BUFFER_SIZE - z.avail_out;
      z.next_out = outbuf;
      while (count >= fileleft) {
        if (fileleft &&
            !WriteFile(fout,z.next_out,fileleft,&bytes_written,NULL)) {
          printf("Write error\n");
        }
        count-=fileleft;
        z.next_out+=fileleft;
        if (!OpenNextOutput(&fout,idata->instfiles,&listpt,
                            &fileleft,logf)) break;
      }
      if (fout==INVALID_HANDLE_VALUE) break;
      if (count && !WriteFile(fout,z.next_out,count,&bytes_written,NULL)) {
        printf("Write error\n");
      }
      fileleft-=count;
      z.next_out = outbuf;
      z.avail_out = BUFFER_SIZE;
    }
    if (status!=Z_OK) break;
  }

  inflateEnd(&z);
  CloseHandle(fout);

  outbuf[0]='\0';
  if (!WriteFile(logf,outbuf,1,&bytes_written,NULL)) {
    printf("Write error\n");
  }
  bfree(outbuf);

  WriteFileList(logf,idata->extrafiles);

  InstallService(idata);

  CoInitialize(NULL);
  SetupShortcuts(logf);
  SetupUninstall();
  CoUninitialize();

  WriteServiceDetails(logf,idata->service);

  CloseHandle(logf);

  SetFileAttributes("install.log",FILE_ATTRIBUTE_HIDDEN);

  ShowWindow(GetDlgItem(mainDlg[DL_DOINSTALL],ST_COMPLETE),SW_SHOW);
  ShowWindow(GetDlgItem(mainDlg[DL_DOINSTALL],ST_EXIT),SW_SHOW);
  EnableWindow(GetDlgItem(mainDlg[DL_DOINSTALL],BT_FINISH),TRUE);
  return 0;
}

void FillFolderList(void) {
  HANDLE findfile;
  WIN32_FIND_DATA finddata;
  bstr *str;
  char *startdir;
  HWND folderlist;

  folderlist = GetDlgItem(mainDlg[DL_SHORTCUTS],LB_FOLDLIST);
  if (!folderlist) return;

  str=bstr_new();

  startdir=GetStartMenuTopDir();
  bstr_assign(str,startdir);
  bfree(startdir);
  bstr_appendpath(str,"Programs\\*");

  findfile = FindFirstFile(str->text,&finddata);
  if (findfile!=INVALID_HANDLE_VALUE) {
    while(1) {
      if (finddata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY &&
          strcmp(finddata.cFileName,".")!=0 &&
          strcmp(finddata.cFileName,"..")!=0) {
        SendMessage(folderlist,LB_ADDSTRING,0,(LPARAM)finddata.cFileName);
      }
      if (!FindNextFile(findfile,&finddata)) break;
    }
    FindClose(findfile);
  }
  bstr_free(str,TRUE);
}

BOOL CheckExistingInstall(InstData *idata) {
  bstr *str,*subkey;
  HKEY key;
  DWORD ind;
  FILETIME ftime;
  BOOL retval=TRUE;

  str=bstr_new();
  bstr_assign(str,UninstallKey);
  bstr_appendpath(str,idata->product);
  
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,str->text,0,KEY_READ,&key)
      ==ERROR_SUCCESS) {
    RegCloseKey(key);
    if (MessageBox(NULL,"This program appears to already be installed.\n"
                   "Are you sure you want to go ahead and install it anyway?",
                   idata->product,MB_YESNO)==IDNO) retval=FALSE;
  } else {
// TODO: Check for old versions to upgrade
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,UninstallKey,0,KEY_READ,&key)
        ==ERROR_SUCCESS) {
/*    for (ind=0;RegEnumKeyEx(key,ind,subkey,subkey->len,
                              NULL,NULL,NULL,&ftime)==ERROR_SUCCESS;ind++) {
      }*/
    }
  }
  return retval;
}

int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,
                     LPSTR lpszCmdParam,int nCmdShow) {
  MSG msg;
  int i;
  BOOL Handled;
  char *licence;

  InitCommonControls();

  hInst = hInstance;

  if (!hPrevInstance) RegisterSepClass(hInstance);

  for (i=0;i<DL_NUM;i++) {
    mainDlg[i] = CreateDialog(hInst,MAKEINTRESOURCE(i+1),NULL,MainDlgProc);
  }

  CheckDlgButton(mainDlg[DL_SHORTCUTS],CB_DESKTOP,BST_CHECKED);
  EnableWindow(GetDlgItem(mainDlg[DL_DOINSTALL],BT_FINISH),FALSE);

  FillFolderList();

  ShowWindow(GetDlgItem(mainDlg[DL_DOINSTALL],ST_COMPLETE),SW_HIDE);
  ShowWindow(GetDlgItem(mainDlg[DL_DOINSTALL],ST_EXIT),SW_HIDE);

  idata=ReadInstData();

  SendDlgItemMessage(mainDlg[DL_SHORTCUTS],ED_FOLDER,WM_SETTEXT,
                     0,(LPARAM)idata->startmenudir);
  SendDlgItemMessage(mainDlg[DL_INSTALLDIR],ED_INSTDIR,WM_SETTEXT,
                     0,(LPARAM)idata->installdir);
  
  licence=GetFirstFile(idata->instfiles,idata->totalsize);

  if (licence) {
    SendDlgItemMessage(mainDlg[DL_LICENCE],ED_LICENCE,WM_SETTEXT,
                       0,(LPARAM)licence);
    bfree(licence);
  }

  for (i=0;i<DL_NUM;i++) SetGuiFont(mainDlg[i]);

  if (CheckExistingInstall(idata)) {
    CurrentDialog=DL_NUM;
    ShowNewDialog(DL_INTRO);

    while (GetMessage(&msg,NULL,0,0)) {
      Handled=FALSE;
      for (i=0;i<DL_NUM && !Handled;i++) {
        Handled=IsDialogMessage(mainDlg[i],&msg);
      }
      if (!Handled) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }
  FreeInstData(idata,FALSE);

  return 0;
}
