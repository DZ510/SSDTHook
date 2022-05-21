#ifndef __FILE_OP__H__
#define __FILE_OP__H__
#include <ntddk.h>




NTSTATUS F_CreateFile(HANDLE * hFile,const char * fileName);

NTSTATUS F_CreateFileNew(HANDLE * hFile, const char * fileName);

NTSTATUS F_GetFileSize(HANDLE hFile, LARGE_INTEGER *size);

//必须是文件的最后一个句柄 才能删除
NTSTATUS F_DeleteFile(HANDLE hFile);

NTSTATUS F_CloseFile(HANDLE hFile);

NTSTATUS F_ReadFile(HANDLE hFile, char * buf, ULONG size,PLARGE_INTEGER offset);

NTSTATUS F_WriteFile(HANDLE hFile, char * buf, ULONG size,PLARGE_INTEGER offset);

NTSTATUS F_WriteFileAppend(HANDLE hFile, char * buf, ULONG size);

BOOLEAN F_ForDeleteRunFile(IN PCWSTR FileName);
#endif