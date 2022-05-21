#include "FileOp.h"




NTSTATUS F_MCreateFile(HANDLE * hFile, const char * fileName,ULONG OpenFileMethod)
{
	OBJECT_ATTRIBUTES object_attr = { 0 };
	ANSI_STRING filePath = { 0 };
	UNICODE_STRING filePathName = { 0 };
	NTSTATUS status = 0;
	IO_STATUS_BLOCK sb = { 0 };

	RtlInitAnsiString(&filePath, fileName);
	status = RtlAnsiStringToUnicodeString(&filePathName, &filePath, TRUE);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("字符串转化失败 0x%X\n", status));
		return status;
	}

	InitializeObjectAttributes(&object_attr, &filePathName, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateFile(&(*hFile), GENERIC_ALL, &object_attr, &sb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		OpenFileMethod, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("创建或打开文件失败 0x%X\n", status));
		return status;
	}

	RtlFreeUnicodeString(&filePathName);
	return status;
}

NTSTATUS F_CreateFile(HANDLE * hFile, const char * fileName)
{
	return F_MCreateFile(hFile, fileName, FILE_OPEN_IF);
}

NTSTATUS F_CreateFileNew(HANDLE * hFile, const char * fileName)
{
	return F_MCreateFile(hFile, fileName, FILE_OVERWRITE_IF);
}

NTSTATUS F_GetFileSize(HANDLE hFile, LARGE_INTEGER * size)
{
	IO_STATUS_BLOCK sb = { 0 };
	FILE_STANDARD_INFORMATION fsi = {0};
	NTSTATUS status = 0;

	status = ZwQueryInformationFile(hFile, &sb, &fsi, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("获取文件长度失败 0x%X\n", status));
		return status;
	}

	size->QuadPart =fsi.EndOfFile.QuadPart;
	return status;
}

NTSTATUS F_DeleteFile(HANDLE hFile)
{
	IO_STATUS_BLOCK sb = { 0 };
	FILE_DISPOSITION_INFORMATION dinfo = {0};
	NTSTATUS status = 0;

	dinfo.DeleteFile = TRUE;
	status = ZwSetInformationFile(hFile, &sb, &dinfo, sizeof(FILE_DISPOSITION_INFORMATION), FileDispositionInformation);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("删除文件失败 0x%X\n", status));
		return status;
	}

	return F_CloseFile(hFile);
	
}

NTSTATUS F_CloseFile(HANDLE hFile)
{
	return ZwClose(hFile);
}

NTSTATUS F_ReadFile(HANDLE hFile, char * buf, ULONG size, PLARGE_INTEGER offset)
{
	IO_STATUS_BLOCK sb = { 0 };
	NTSTATUS status = 0;
	LARGE_INTEGER m_offset = {0};

	if (offset == NULL)
	{
		m_offset.HighPart = -1;
		m_offset.LowPart = FILE_USE_FILE_POINTER_POSITION;
		offset = &m_offset;
	}

	status = ZwReadFile(hFile, NULL, NULL, NULL, &sb, (PVOID)buf, size, offset, NULL);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("读取文件失败 0x%X\n", status));
		return status;
	}

	return status;
}

NTSTATUS F_WriteFile(HANDLE hFile, char * buf, ULONG size, PLARGE_INTEGER offset)
{
	IO_STATUS_BLOCK sb = { 0 };
	NTSTATUS status = 0;
	
	status = ZwWriteFile(hFile, NULL, NULL, NULL, &sb, (PVOID)buf, size, offset, NULL);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("写入文件失败 0x%X\n", status));
		return status;
	}

	return status;
}

NTSTATUS F_WriteFileAppend(HANDLE hFile, char * buf, ULONG size)
{
	
	NTSTATUS status = 0;
	LARGE_INTEGER offset = {0};

	

	status = F_GetFileSize(hFile, &offset);
	
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	return F_WriteFile(hFile, buf, size, &offset);
}




HANDLE F_SkillIoOpenFile(
	IN PCWSTR FileName,
	IN ACCESS_MASK DesiredAccess,
	IN ULONG ShareAccess
)
{
	NTSTATUS              ntStatus;
	UNICODE_STRING        uniFileName;
	OBJECT_ATTRIBUTES     objectAttributes;
	HANDLE                ntFileHandle;
	IO_STATUS_BLOCK       ioStatus;

	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
	{
		return 0;
	}

	RtlInitUnicodeString(&uniFileName, FileName);

	InitializeObjectAttributes(&objectAttributes, &uniFileName,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	ntStatus = IoCreateFile(&ntFileHandle,
		DesiredAccess,
		&objectAttributes,
		&ioStatus,
		0,
		FILE_ATTRIBUTE_NORMAL,
		ShareAccess,
		FILE_OPEN,
		0,
		NULL,
		0,
		0,
		NULL,
		IO_NO_PARAMETER_CHECKING);

	if (!NT_SUCCESS(ntStatus))
	{
		return 0;
	}

	return ntFileHandle;
}

NTSTATUS F_SkillSetFileCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	Irp->UserIosb->Status = Irp->IoStatus.Status;
	Irp->UserIosb->Information = Irp->IoStatus.Information;

	KeSetEvent(Irp->UserEvent, IO_NO_INCREMENT, FALSE);

	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}


BOOLEAN F_SKillStripFileAttributes(IN HANDLE    FileHandle)
{
	NTSTATUS          ntStatus = STATUS_SUCCESS;
	PFILE_OBJECT      fileObject;
	PDEVICE_OBJECT    DeviceObject;
	PIRP              Irp;
	KEVENT            event1;
	FILE_BASIC_INFORMATION    FileInformation;
	IO_STATUS_BLOCK ioStatus;
	PIO_STACK_LOCATION irpSp;

	ntStatus = ObReferenceObjectByHandle(FileHandle,
		DELETE,
		*IoFileObjectType,
		KernelMode,
		&fileObject,
		NULL);//我想知道的是这个文件句柄是在哪个进程的句柄表中

	if (!NT_SUCCESS(ntStatus))
	{
		return FALSE;
	}

	DeviceObject = IoGetRelatedDeviceObject(fileObject);
	Irp = IoAllocateIrp(DeviceObject->StackSize, TRUE);

	if (Irp == NULL)
	{
		ObDereferenceObject(fileObject);
		return FALSE;
	}

	KeInitializeEvent(&event1, SynchronizationEvent, FALSE);

	memset(&FileInformation, 0, 0x28);

	FileInformation.FileAttributes = FILE_ATTRIBUTE_NORMAL;
	Irp->AssociatedIrp.SystemBuffer = &FileInformation;
	Irp->UserEvent = &event1;
	Irp->UserIosb = &ioStatus;
	Irp->Tail.Overlay.OriginalFileObject = fileObject;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->RequestorMode = KernelMode;

	irpSp = IoGetNextIrpStackLocation(Irp);
	irpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
	irpSp->DeviceObject = DeviceObject;
	irpSp->FileObject = fileObject;
	irpSp->Parameters.SetFile.Length = sizeof(FILE_BASIC_INFORMATION);
	irpSp->Parameters.SetFile.FileInformationClass = FileBasicInformation;
	irpSp->Parameters.SetFile.FileObject = fileObject;

	IoSetCompletionRoutine(
		Irp,
		F_SkillSetFileCompletion,
		&event1,
		TRUE,
		TRUE,
		TRUE);

	IoCallDriver(DeviceObject, Irp);//调用这个设备对象的驱动对象，并且ＩＯ＿ＳｔＡＣＫ＿ＬＯＣＡｔｉｏｎ会指向下一个，也就是刚刚设置的
									//如果没有文件系统驱动建立的设备对象没有Attacked的话，就调用文件系统驱动的IRP_MJ_SET_INFORMATION分派例程


									//会调用NTFS.sys驱动的NtfsFsdSetInformation例程，再会进入NtfsSetBasicInfo（）函数，最后它会设置代表此文件的FCB（文件
									//控制块结构的一些信息，用来设置代表此文件的属性。最后不知道在哪里会调用IoCompleteRequest,它会依次调用先前设置的回调函数
									//回调函数会释放刚分配的IRP和设置事件对象为受信状态。
	KeWaitForSingleObject(&event1, Executive, KernelMode, TRUE, NULL);//一等到事件对象变成受信状态就会继续向下执行。

	ObDereferenceObject(fileObject);

	return TRUE;
}

BOOLEAN F_SKillDeleteFile(IN HANDLE    FileHandle)
{
	NTSTATUS          ntStatus = STATUS_SUCCESS;
	PFILE_OBJECT      fileObject;
	PDEVICE_OBJECT    DeviceObject;
	PIRP              Irp;
	KEVENT            event1;
	FILE_DISPOSITION_INFORMATION    FileInformation;
	IO_STATUS_BLOCK ioStatus;
	PIO_STACK_LOCATION irpSp;
	PSECTION_OBJECT_POINTERS pSectionObjectPointer;

	F_SKillStripFileAttributes(FileHandle);          //去掉只读属性，才能删除只读文件

	ntStatus = ObReferenceObjectByHandle(FileHandle,
		DELETE,
		*IoFileObjectType,
		KernelMode,
		&fileObject,
		NULL);

	if (!NT_SUCCESS(ntStatus))
	{
		return FALSE;
	}

	DeviceObject = IoGetRelatedDeviceObject(fileObject);//如果NTFS.sys驱动建立的设备对象上没有附加的设备对象的话，就返回NTFS.sys建立的设备对象
														//否则返回的是这个设备对象的highest level设备对象。
	Irp = IoAllocateIrp(DeviceObject->StackSize, TRUE);//如果没有附加，StackSize为7

	if (Irp == NULL)
	{
		ObDereferenceObject(fileObject);
		return FALSE;
	}

	KeInitializeEvent(&event1, SynchronizationEvent, FALSE);

	FileInformation.DeleteFile = TRUE;

	Irp->AssociatedIrp.SystemBuffer = &FileInformation;
	Irp->UserEvent = &event1;
	Irp->UserIosb = &ioStatus;
	Irp->Tail.Overlay.OriginalFileObject = fileObject;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->RequestorMode = KernelMode;

	irpSp = IoGetNextIrpStackLocation(Irp);            //得到文件系统NTFS.sys驱动的设备IO_STACK_LOCATION
	irpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
	irpSp->DeviceObject = DeviceObject;
	irpSp->FileObject = fileObject;
	irpSp->Parameters.SetFile.Length = sizeof(FILE_DISPOSITION_INFORMATION);
	irpSp->Parameters.SetFile.FileInformationClass = FileDispositionInformation;
	irpSp->Parameters.SetFile.FileObject = fileObject;


	IoSetCompletionRoutine(
		Irp,
		F_SkillSetFileCompletion,
		&event1,
		TRUE,
		TRUE,
		TRUE);

	//再加上下面这三行代码 ，MmFlushImageSection    函数通过这个结构来检查是否可以删除文件。
	pSectionObjectPointer = fileObject->SectionObjectPointer;
	pSectionObjectPointer->ImageSectionObject = 0;
	pSectionObjectPointer->DataSectionObject = 0;


	IoCallDriver(DeviceObject, Irp);//这里会依次进入NTFS.sys驱动的NtfsFsdSetInformation例程->NtfsSetDispositionInfo（）->MmFlushImageSection(),
									//MmFlushImageSection（）这函数是用来检查FILE_OBJECT对象的SECTION_OBJECT_POINTER结构的变量，检查这个文件
									//在内存有没有被映射。也就是有没有执行。如果上面那样设置了，也就是说文件可以删除了。我们也可以HOOK NTFS.sys导入表中的
									//的MmFlushImageSection（），来检查这个文件对象是不是我们要删除 的，是的话，返回TRUE就行了。
	KeWaitForSingleObject(&event1, Executive, KernelMode, TRUE, NULL);

	ObDereferenceObject(fileObject);

	return TRUE;
}

BOOLEAN F_ForDeleteRunFile(IN PCWSTR FileName)
{
	HANDLE hFileHandle = F_SkillIoOpenFile(FileName,
		FILE_READ_ATTRIBUTES,
		FILE_SHARE_DELETE);   //得到文件句柄

	if (hFileHandle != NULL)
	{
		F_SKillDeleteFile(hFileHandle);
		ZwClose(hFileHandle);
		return TRUE;
	}

	return FALSE;
}