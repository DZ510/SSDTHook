#include <ntifs.h>
#include "FileOp.h"
#include "PETool.h"
#include "MyMdl.h"



VOID UnHookSSDT();
VOID HookSSDT();

typedef struct _KSERVICE_TABLE_DESCRIPTOR {
	PULONG_PTR Base;
	DWORD32 Count;
	ULONG Limit;
	PUCHAR Number;
} KSERVICE_TABLE_DESCRIPTOR, *PKSERVICE_TABLE_DESCRIPTOR;

typedef NTSTATUS(*SSDT_NtOpenProcess)(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);
SSDT_NtOpenProcess pOld_OpenProcessAddres = NULL;
EXTERN_C PKSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTable;

NTSTATUS NTAPI MyOpenProcess(
	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
)
{
	DbgPrintEx(77, 0, "[CALL OpenProcess %p]\r\n", ClientId);
	return NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);

}


VOID DriverUnload(PDRIVER_OBJECT pDrvier)
{
	UnHookSSDT();
	return STATUS_SUCCESS;
}



DWORD32 GetDestFuncServiceNumber(char* pDestFuncName)
{

	HANDLE hfile = NULL;

	F_CreateFile(&hfile, "\\??\\C:\\ntdll.dll");
	LARGE_INTEGER laFileSize = { 0 };
	F_GetFileSize(hfile, &laFileSize);
	PVOID pBufer = ExAllocatePool(NonPagedPool, laFileSize.QuadPart);
	memset(pBufer, 0, laFileSize.QuadPart);
	NTSTATUS nts = F_ReadFile(hfile, pBufer, laFileSize.QuadPart, NULL);
	if (!NT_SUCCESS(nts))
	{
		F_CloseFile(hfile);
		ExFreePool(pBufer);
		return -1;
	}
	PVOID pBuffer_Ex = LoadPE_NoFix(pBufer);
	if (pBuffer_Ex)
	{
		ExFreePool(pBufer);
	}
	PVOID memNtOpenProcess = MyGetProcAddres(pBuffer_Ex, "NtOpenProcess");
	DWORD32 FuncServiceIndex = *(PDWORD32)((unsigned char*)memNtOpenProcess + 1);
	ExFreePool(pBuffer_Ex);
	F_CloseFile(hfile);
	return FuncServiceIndex;
}


VOID UnHookSSDT() 
{
	
	if (pOld_OpenProcessAddres != NULL)
	{
		PVOID pServiceFunTable = KeServiceDescriptorTable->Base;
		PMDL *pmdl = NULL;
		ULONG_PTR* pMapServiceFunTable = MdlNonMapVirtual(pServiceFunTable, ((INT)(KeServiceDescriptorTable->Count * 4) / PAGE_SIZE + 1)*PAGE_SIZE, &pmdl);
		DWORD32 FuncServiceIndex = GetDestFuncServiceNumber("NtOpenProcess");
		pMapServiceFunTable[FuncServiceIndex] = pOld_OpenProcessAddres;
		MdlNonUnMapVirtual(pMapServiceFunTable, pmdl);
	}

}

VOID HookSSDT()
{
	PVOID pServiceFunTable = KeServiceDescriptorTable->Base;
	PMDL *pmdl = NULL;

	ULONG_PTR* pMapServiceFunTable = MdlNonMapVirtual(pServiceFunTable, ((INT)(KeServiceDescriptorTable->Count * 4)/PAGE_SIZE+1)*PAGE_SIZE, &pmdl);
	DWORD32 FuncServiceIndex = GetDestFuncServiceNumber("NtOpenProcess");
	if (FuncServiceIndex == -1)
	{
		DbgPrintEx(77, 0, "获取目标函数服务号失败！！");
		return;
	}
	pOld_OpenProcessAddres = pMapServiceFunTable[FuncServiceIndex];
	pMapServiceFunTable[FuncServiceIndex] = MyOpenProcess;
	MdlNonUnMapVirtual(pMapServiceFunTable, pmdl);

}





NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pReg) {
	pDriver->DriverUnload = DriverUnload;

	HookSSDT();


	return STATUS_SUCCESS;

}