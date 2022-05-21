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
		KdPrint(("�ַ���ת��ʧ�� 0x%X\n", status));
		return status;
	}

	InitializeObjectAttributes(&object_attr, &filePathName, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateFile(&(*hFile), GENERIC_ALL, &object_attr, &sb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		OpenFileMethod, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("��������ļ�ʧ�� 0x%X\n", status));
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
		KdPrint(("��ȡ�ļ�����ʧ�� 0x%X\n", status));
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
		KdPrint(("ɾ���ļ�ʧ�� 0x%X\n", status));
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
		KdPrint(("��ȡ�ļ�ʧ�� 0x%X\n", status));
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
		KdPrint(("д���ļ�ʧ�� 0x%X\n", status));
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
		NULL);//����֪����������ļ���������ĸ����̵ľ������

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

	IoCallDriver(DeviceObject, Irp);//��������豸������������󣬲��ңɣϣߣӣ����ãˣߣ̣ϣã��������ָ����һ����Ҳ���Ǹո����õ�
									//���û���ļ�ϵͳ�����������豸����û��Attacked�Ļ����͵����ļ�ϵͳ������IRP_MJ_SET_INFORMATION��������


									//�����NTFS.sys������NtfsFsdSetInformation���̣��ٻ����NtfsSetBasicInfo��������������������ô�����ļ���FCB���ļ�
									//���ƿ�ṹ��һЩ��Ϣ���������ô�����ļ������ԡ����֪������������IoCompleteRequest,�������ε�����ǰ���õĻص�����
									//�ص��������ͷŸշ����IRP�������¼�����Ϊ����״̬��
	KeWaitForSingleObject(&event1, Executive, KernelMode, TRUE, NULL);//һ�ȵ��¼�����������״̬�ͻ��������ִ�С�

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

	F_SKillStripFileAttributes(FileHandle);          //ȥ��ֻ�����ԣ�����ɾ��ֻ���ļ�

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

	DeviceObject = IoGetRelatedDeviceObject(fileObject);//���NTFS.sys�����������豸������û�и��ӵ��豸����Ļ����ͷ���NTFS.sys�������豸����
														//���򷵻ص�������豸�����highest level�豸����
	Irp = IoAllocateIrp(DeviceObject->StackSize, TRUE);//���û�и��ӣ�StackSizeΪ7

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

	irpSp = IoGetNextIrpStackLocation(Irp);            //�õ��ļ�ϵͳNTFS.sys�������豸IO_STACK_LOCATION
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

	//�ټ������������д��� ��MmFlushImageSection    ����ͨ������ṹ������Ƿ����ɾ���ļ���
	pSectionObjectPointer = fileObject->SectionObjectPointer;
	pSectionObjectPointer->ImageSectionObject = 0;
	pSectionObjectPointer->DataSectionObject = 0;


	IoCallDriver(DeviceObject, Irp);//��������ν���NTFS.sys������NtfsFsdSetInformation����->NtfsSetDispositionInfo����->MmFlushImageSection(),
									//MmFlushImageSection�����⺯�����������FILE_OBJECT�����SECTION_OBJECT_POINTER�ṹ�ı������������ļ�
									//���ڴ���û�б�ӳ�䡣Ҳ������û��ִ�С�����������������ˣ�Ҳ����˵�ļ�����ɾ���ˡ�����Ҳ����HOOK NTFS.sys������е�
									//��MmFlushImageSection���������������ļ������ǲ�������Ҫɾ�� �ģ��ǵĻ�������TRUE�����ˡ�
	KeWaitForSingleObject(&event1, Executive, KernelMode, TRUE, NULL);

	ObDereferenceObject(fileObject);

	return TRUE;
}

BOOLEAN F_ForDeleteRunFile(IN PCWSTR FileName)
{
	HANDLE hFileHandle = F_SkillIoOpenFile(FileName,
		FILE_READ_ATTRIBUTES,
		FILE_SHARE_DELETE);   //�õ��ļ����

	if (hFileHandle != NULL)
	{
		F_SKillDeleteFile(hFileHandle);
		ZwClose(hFileHandle);
		return TRUE;
	}

	return FALSE;
}