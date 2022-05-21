#pragma once
#include <ntifs.h>
#include <ntimage.h>

VOID LoadPE(unsigned char* pPEfileData);
PVOID LoadPE_NoFix(unsigned char* pPEfileData);
VOID RelocImage(unsigned char* pImageBase);
VOID FixImportTable(PUCHAR pImageBase);
ULONG_PTR MyGetProcAddres(PUCHAR pImageBase, char* pszDestFuncName);
VOID UpdateCookie(unsigned char* pImageBase);
