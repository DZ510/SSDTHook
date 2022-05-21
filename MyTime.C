#include "MyTime.h"
VOID KernelSleep(ULONG ms,BOOLEAN isAlert)
{
	LARGE_INTEGER inTime = {0};
	inTime.QuadPart = (ULONG64)-10000;
	inTime.QuadPart *= ms;
	KeDelayExecutionThread(KernelMode, isAlert, &inTime);
}