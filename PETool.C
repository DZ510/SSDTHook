#include "PETool.h"
#include "MyGetSysModule.h"

ULONG_PTR MyGetProcAddres(PUCHAR pImageBase, char* pszDestFuncName)
{
	PIMAGE_DOS_HEADER pImageDos = (PIMAGE_DOS_HEADER)pImageBase;
	PIMAGE_NT_HEADERS pImageNt = (PIMAGE_NT_HEADERS)(pImageBase + pImageDos->e_lfanew);
	PIMAGE_DATA_DIRECTORY pDir = &pImageNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY pExportInfo = pImageBase + pDir->VirtualAddress;
	
	PUINT32 lpFunctionTable = pImageBase + pExportInfo->AddressOfFunctions;
	PUINT16 lpOrdinalsTable = pImageBase + pExportInfo->AddressOfNameOrdinals;
	PUINT32 lpNameTable = pImageBase + pExportInfo->AddressOfNames;

	for (size_t i = 0; i < pExportInfo->NumberOfFunctions; i++)
	{
		if (_stricmp(pszDestFuncName, pImageBase + lpNameTable[i]) == 0)
		{
			UINT16 index = lpOrdinalsTable[i];
			if (index != -1)
			{
				return pImageBase + lpFunctionTable[index];
			}
			
		}
	}
	return 0;
}

VOID FixImportTable(PUCHAR pImageBase)
{
	PIMAGE_DOS_HEADER pImageDos = (PIMAGE_DOS_HEADER)pImageBase;
	PIMAGE_NT_HEADERS pImageNt = (PIMAGE_NT_HEADERS)(pImageBase + pImageDos->e_lfanew);
	PIMAGE_DATA_DIRECTORY pDir = &pImageNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	PIMAGE_IMPORT_DESCRIPTOR pImportInfo = pImageBase + pDir->VirtualAddress;

	while (pImportInfo->Name && pImportInfo->Characteristics)
	{
		char* szImportMoudlNmae = pImageBase + pImportInfo->Name;
		PIMAGE_THUNK_DATA pITD = pImageBase + pImportInfo->FirstThunk;

		ULONG_PTR ulDsetMoudleBase = FindMoudleBaseA(szImportMoudlNmae, 0);
		for (int nIndex = 0; pITD->u1.AddressOfData; nIndex++, pITD++)
		{
			if (pITD->u1.AddressOfData & 0x80000000)
			{
				DbgPrintEx(77, 0, "[FixImportTable]:%s,%d", szImportMoudlNmae, nIndex);
				continue;
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME pFunctionNameinfo = pImageBase + pITD->u1.ForwarderString;
				if (_stricmp(szImportMoudlNmae, "hal.dll") == 0 ||
					_stricmp(szImportMoudlNmae, "ntkrnlpa.exe") == 0 ||
					_stricmp(szImportMoudlNmae, "ntkrpamp.exe") == 0 ||
					_stricmp(szImportMoudlNmae, "ntoskrnl.exe") == 0
					)
				{

					UNICODE_STRING WzUniDestFunName = { 0 };
					ANSI_STRING WzAnsiDestFunName = { 0 };
					RtlInitAnsiString(&WzAnsiDestFunName, pFunctionNameinfo->Name);
					RtlAnsiStringToUnicodeString(&WzUniDestFunName, &WzAnsiDestFunName, TRUE);
#ifdef _W64
					*(PULONG)(pITD) = MmGetSystemRoutineAddress(&WzUniDestFunName);
#else
					*(PULONG64)(pITD) = MmGetSystemRoutineAddress(&WzUniDestFunName);
#endif // _W64				
				}
				else
				{
#ifdef _W64
					*(PULONG)(pITD) = MyGetProcAddres(ulDsetMoudleBase, pFunctionNameinfo->Name);
#else
					*(PULONG64)(pITD) = MyGetProcAddres(ulDsetMoudleBase, pFunctionNameinfo->Name);
#endif // _W64
				}
			}


		}


		
		ULONG_PTR DestMoudleBase = FindMoudleBaseA(szImportMoudlNmae,NULL);



		pImportInfo++;

	}


}




VOID RelocImage(unsigned char* pImageBase) {
	typedef struct _IMAGE_RELOC {
		UINT16 Offset : 12;
		UINT16 Type : 4;
	}IMAGE_RELOC,*PIMAGE_RELOC;


	PIMAGE_DOS_HEADER pImageDos = (PIMAGE_DOS_HEADER)pImageBase;
	PIMAGE_NT_HEADERS pImageNt = (PIMAGE_NT_HEADERS)(pImageBase + pImageDos->e_lfanew);
	PIMAGE_DATA_DIRECTORY pDir = &pImageNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	if (!pDir->VirtualAddress) return;
	
	PIMAGE_BASE_RELOCATION pImageReloc = (PIMAGE_BASE_RELOCATION)(pImageBase + pDir->VirtualAddress);
	ULONG diff = (ULONG)pImageBase - pImageNt->OptionalHeader.ImageBase;
	while (pImageReloc->VirtualAddress && pImageReloc->SizeOfBlock)
	{
		
		ULONG_PTR pRelocPageBase = pImageBase + pImageReloc->VirtualAddress;
		PIMAGE_RELOC pPageOffset = (unsigned char*)pImageReloc + sizeof(IMAGE_BASE_RELOCATION);
		DWORD32 PageOffsetCount = (pImageReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(UINT16);
		for (size_t i = 0; i < PageOffsetCount; i++,pPageOffset++)
		{
			if (pPageOffset->Type == IMAGE_REL_BASED_DIR64)
			{
				
				PUINT64 Addres = (PUINT64)((PUCHAR)pRelocPageBase + pPageOffset->Offset);
				*Addres += diff;

			}
			else if (pPageOffset->Type == IMAGE_REL_BASED_HIGHLOW)
			{
				PUINT32 Addres = (PUINT32)((PUCHAR)pRelocPageBase + pPageOffset->Offset);
				*Addres += diff;
			}
		}
		pImageReloc = (PIMAGE_BASE_RELOCATION)pPageOffset + PageOffsetCount;
	}

}
VOID UpdateCookie(unsigned char* pImageBase)
{
	PIMAGE_DOS_HEADER pImageDos = (PIMAGE_DOS_HEADER)pImageBase;
	PIMAGE_NT_HEADERS pImageNt = (PIMAGE_NT_HEADERS)(pImageBase + pImageDos->e_lfanew);
	PIMAGE_DATA_DIRECTORY pDir = &pImageNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
	PIMAGE_LOAD_CONFIG_DIRECTORY pLoadConfigDirectory = pImageNt + pDir->VirtualAddress;
	*((PULONG_PTR)pImageBase + pLoadConfigDirectory->SecurityCookie) += 10;

}



VOID LoadPE(unsigned char* pPeData) {
	PIMAGE_DOS_HEADER pFileDos = (PIMAGE_DOS_HEADER)pPeData;
	PIMAGE_NT_HEADERS pFileNt = (PIMAGE_NT_HEADERS)(pPeData + pFileDos->e_lfanew);

	PIMAGE_SECTION_HEADER pFileSection = IMAGE_FIRST_SECTION(pFileNt);
	
	DWORD32 SizeOfImage = pFileNt->OptionalHeader.SizeOfImage;

	unsigned char* pImageBuf = (unsigned char*)ExAllocatePool(NonPagedPool, SizeOfImage);
	memset(pImageBuf, 0, SizeOfImage);

	memcpy(pImageBuf, pPeData, pFileNt->OptionalHeader.SizeOfHeaders);

	for (size_t i = 0; i < pFileNt->FileHeader.NumberOfSections; i++)
	{
		if (pFileSection[i].SizeOfRawData != 0)
		{
			memcpy(pImageBuf + pFileSection[i].VirtualAddress, pPeData + pFileSection[i].PointerToRawData, pFileSection[i].SizeOfRawData);
		}
		
	}
	RelocImage(pImageBuf);
	FixImportTable(pImageBuf);
	//UpdateCookie(pImageBuf);
	PIMAGE_DOS_HEADER pImgDos = (PIMAGE_DOS_HEADER)pImageBuf;

	PIMAGE_NT_HEADERS pImgNts = (PIMAGE_NT_HEADERS)(pImageBuf + pImgDos->e_lfanew);

	PDRIVER_INITIALIZE EntryPoint = (PDRIVER_INITIALIZE)(pImageBuf + pImgNts->OptionalHeader.AddressOfEntryPoint);
	EntryPoint(0, 0);
}

PVOID LoadPE_NoFix(unsigned char * pPEfileData)
{
	PIMAGE_DOS_HEADER pFileDos = (PIMAGE_DOS_HEADER)pPEfileData;
	PIMAGE_NT_HEADERS pFileNt = (PIMAGE_NT_HEADERS)(pPEfileData + pFileDos->e_lfanew);

	PIMAGE_SECTION_HEADER pFileSection = IMAGE_FIRST_SECTION(pFileNt);

	DWORD32 SizeOfImage = pFileNt->OptionalHeader.SizeOfImage;

	unsigned char* pImageBuf = (unsigned char*)ExAllocatePool(NonPagedPool, SizeOfImage);
	memset(pImageBuf, 0, SizeOfImage);

	memcpy(pImageBuf, pPEfileData, pFileNt->OptionalHeader.SizeOfHeaders);

	for (size_t i = 0; i < pFileNt->FileHeader.NumberOfSections; i++)
	{
		if (pFileSection[i].SizeOfRawData != 0)
		{
			memcpy(pImageBuf + pFileSection[i].VirtualAddress, pPEfileData + pFileSection[i].PointerToRawData, pFileSection[i].SizeOfRawData);
		}

	}
	return pImageBuf;
}
