#include <ntifs.h>
#include <string.h>

NTSTATUS NTAPI ZwQuerySystemInformation(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
	HANDLE Section;                 // Not filled in
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR  FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

ULONG_PTR FindMoudleBaseA(char* szMoudleName, ULONG_PTR ulRetModuleSize) {
	RTL_PROCESS_MODULES SysModuleInfo = { 0 };
	ULONG ulRetNumber = 0;
	ULONG_PTR lpRetModuleBase = 0;
	ULONG RetModuleSize = 0;
	PRTL_PROCESS_MODULES pSysModuleInfo = 0;

	NTSTATUS ns = ZwQuerySystemInformation(11, &SysModuleInfo, sizeof(SysModuleInfo), &ulRetNumber);
	if (!NT_SUCCESS(ns) && ns == STATUS_INFO_LENGTH_MISMATCH) {
		pSysModuleInfo = ExAllocatePool(NonPagedPool, ulRetNumber);
		memset(pSysModuleInfo, 0, ulRetNumber);
		ns = ZwQuerySystemInformation(11, pSysModuleInfo, ulRetNumber, &ulRetNumber);
		if (!NT_SUCCESS(ns))
		{
			lpRetModuleBase = 0;
			RetModuleSize = 0;
			goto SafeExit;
		}
		if (_stricmp(szMoudleName, "ntkrnlpa.exe") == 0 || _stricmp(szMoudleName, "ntoskrnl.exe") == 0)
		{
			lpRetModuleBase = pSysModuleInfo->Modules[0].ImageBase;
			RetModuleSize = pSysModuleInfo->Modules[0].ImageSize;
			goto SafeExit;
		}
		for (int i = 0; i < pSysModuleInfo->NumberOfModules; i++)
		{

			if (_stricmp(szMoudleName, (pSysModuleInfo->Modules[i].FullPathName + pSysModuleInfo->Modules[i].OffsetToFileName)) == 0)
			{
				lpRetModuleBase = pSysModuleInfo->Modules[i].ImageBase;
				RetModuleSize = pSysModuleInfo->Modules[i].ImageSize;
				break;
			}
		}



	}

SafeExit:
	if (pSysModuleInfo != 0)
	{
		ExFreePool(pSysModuleInfo);
	}
	if (ulRetModuleSize)
	{
		*(PULONG)ulRetModuleSize = RetModuleSize;
	}

	return lpRetModuleBase;
}

