#include <wdm.h>
#include <tdikrnl.h>
#include <tdi.h>

PDEVICE_OBJECT DeviceToBeFiltered;

typedef struct _EXTENSION {
	PDEVICE_OBJECT TopOfDeviceStack;
} EXTENSION_OBJECT, *PEXTENSION_OBJECT;

VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
	PEXTENSION_OBJECT DeviceExtension = (PEXTENSION_OBJECT)DriverObject->DeviceObject->DeviceExtension;

	DbgPrint("Unloading Driver!\r\n");

	IoDetachDevice(DeviceExtension->TopOfDeviceStack);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS NotSupported(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PEXTENSION_OBJECT DeviceExtension = (PEXTENSION_OBJECT)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	ntStatus = IoCallDriver(DeviceExtension->TopOfDeviceStack, Irp);

	return ntStatus;
}

// most fun part of this project
NTSTATUS Dispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PEXTENSION_OBJECT devExt = (PEXTENSION_OBJECT)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION StackIrpPointer = NULL;

	StackIrpPointer = IoGetCurrentIrpStackLocation(Irp);
	if (!StackIrpPointer) {
		DbgPrint("Fatal Error: IRP stack pointer is NULL!\r\n");
		return STATUS_UNSUCCESSFUL;
	}

	if (DeviceObject == DeviceToBeFiltered) {
		if (StackIrpPointer->MinorFunction == TDI_CONNECT) {
			DbgPrint("A TDI connect via TCP was invoked!\r\n");
		}
		else {
			DbgPrint("TDI Request observed: function code %x\r\n", StackIrpPointer->MinorFunction);

			if (StackIrpPointer->MinorFunction == TDI_SEND) {
				DbgPrint("Application sent a TCP-Data packet of size %d.\r\n", StackIrpPointer->Parameters.DeviceIoControl.OutputBufferLength);
			}
		}

		IoSkipCurrentIrpStackLocation(Irp);
		ntStatus = IoCallDriver(devExt->TopOfDeviceStack, Irp);
	}

	return ntStatus;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PEXTENSION_OBJECT DeviceExtension;
	UNICODE_STRING FilteredDeviceName;
	unsigned int i = 0;

	DbgPrint("DriverEntry was called!\n");

	ntStatus = IoCreateDevice(DriverObject, sizeof(EXTENSION_OBJECT), NULL, FILE_DEVICE_NETWORK, FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceToBeFiltered);

	if (!NT_SUCCESS(ntStatus)) {
		IoDeleteDevice(DeviceToBeFiltered);
		DbgPrint("Attaching to TCP device failed!\r\n");
	}
	else {
		DeviceExtension = (PEXTENSION_OBJECT)DeviceToBeFiltered->DeviceExtension;

		for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
			DriverObject->MajorFunction[i] = NotSupported;
		}

		DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = Dispatch;

		DriverObject->DriverUnload = UnloadDriver;

		RtlInitUnicodeString(&FilteredDeviceName, L"\\Device\\Tcp");

		ntStatus = IoAttachDevice(DeviceToBeFiltered, &FilteredDeviceName, &DeviceExtension->TopOfDeviceStack);
		if (!NT_SUCCESS(ntStatus)) {
			UnloadDriver(DriverObject);
			DbgPrint("Attaching to TCP device failed!\r\n");
		}
	}

	return ntStatus;
}