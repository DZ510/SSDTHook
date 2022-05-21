#include "MyMdl.h"
PVOID MdlMapVirtual(PVOID virtualAddress, ULONG_PTR len, MODE MapMode,PMDL *RetPMdl)
{
	PMDL pmdl = IoAllocateMdl(virtualAddress, len, NULL, NULL, NULL);

	PULONG_PTR map = NULL;
	BOOLEAN isLock = FALSE;
	__try
	{
		MmProbeAndLockPages(pmdl, MapMode, IoReadAccess);
		isLock = TRUE;
		map = MmMapLockedPagesSpecifyCache(pmdl, MapMode, MmNonCached, NULL, FALSE, NormalPagePriority);
	}
	__except (1)
	{
		map = NULL;

		if (isLock)
		{
			MmUnlockPages(pmdl);
		}

		if (pmdl)
		{
			IoFreeMdl(pmdl);
		}


	}

	if (!map)
	{
		IoFreeMdl(pmdl);
		return NULL;
	}

	*RetPMdl = pmdl;
	return map;
}

VOID MdlUnMapVirtual(PVOID mapAddress, PMDL pmdl)
{
	MmUnmapLockedPages(mapAddress, pmdl);
	MmUnlockPages(pmdl);
	IoFreeMdl(pmdl);
}



PVOID MdlNonMapVirtual(PVOID virtualAddress, ULONG_PTR len, PMDL *RetPMdl)
{
	PMDL pmdl = IoAllocateMdl(virtualAddress, len, NULL, NULL, NULL);
	PULONG_PTR map = NULL;
	if (!pmdl) return NULL;
		
	MmBuildMdlForNonPagedPool(pmdl);
	map = MmMapLockedPagesSpecifyCache(pmdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
	
	if (!map)
	{
		IoFreeMdl(pmdl);
		return NULL;
	}

	*RetPMdl = pmdl;
	return map;
}

VOID MdlNonUnMapVirtual(PVOID mapAddress, PMDL pmdl)
{
	MmUnmapLockedPages(mapAddress, pmdl);
	IoFreeMdl(pmdl);
}


