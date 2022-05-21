#include <ntifs.h>

PVOID   MdlMapVirtual(PVOID virtualAddress, ULONG_PTR len, MODE MapMode,PMDL *RetPMdl);
VOID    MdlUnMapVirtual(PVOID mapAddress, PMDL pmdl);
PVOID   MdlNonMapVirtual(PVOID virtualAddress, ULONG_PTR len, PMDL *RetPMdl);
VOID    MdlNonUnMapVirtual(PVOID mapAddress, PMDL pmdl);