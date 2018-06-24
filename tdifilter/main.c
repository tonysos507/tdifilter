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
//NTSTATUS Dispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
//{
//	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
//	PEXTENSION_OBJECT devExt = (PEXTENSION_OBJECT)DeviceObject->DeviceExtension;
//	PIO_STACK_LOCATION StackIrpPointer = NULL;
//
//	// add TDI
//	PTDI_REQUEST_KERNEL_CONNECT TDI_connectRequest;
//	PTA_ADDRESS TA_Address_data;
//	PTDI_ADDRESS_IP TDI_data;
//
//	StackIrpPointer = IoGetCurrentIrpStackLocation(Irp);
//	if (!StackIrpPointer) {
//		DbgPrint("Fatal Error: IRP stack pointer is NULL!\r\n");
//		return STATUS_UNSUCCESSFUL;
//	}
//
//	if (DeviceObject == DeviceToBeFiltered) {
//		if (StackIrpPointer->MinorFunction == TDI_CONNECT) {
//			//DbgPrint("A TDI connect via TCP was invoked!\r\n");
//			TDI_connectRequest = (PTDI_REQUEST_KERNEL_CONNECT)(&StackIrpPointer->Parameters);
//			TA_Address_data = ((PTRANSPORT_ADDRESS)(TDI_connectRequest->RequestConnectionInformation->RemoteAddress))->Address;
//			TDI_data = (PTDI_ADDRESS_IP)(TA_Address_data->Address);
//
//			IoSkipCurrentIrpStackLocation(Irp);
//			ntStatus = IoCallDriver(devExt->TopOfDeviceStack, Irp);
//
//			Address = TDI_data->in_addr;
//			Port = TDI_data->sin_port;
//			data.address[0] = ((char *)&Address)[0];
//			data.address[1] = ((char *)&Address)[1];
//			data.address[2] = ((char *)&Address)[2];
//			data.address[3] = ((char *)&Address)[3];
//			data.port[0] = ((char *)&Port)[0];
//			data.port[1] = ((char *)&Port)[1];
//			Port = data.port[0] + data.port[1];
//
//			DbgPrint("TCP address is %d.%d.%d.%d:%d \r\n", data.address[0], data.address[1], data.address[2], data.address[3], Port);
//		}
//		else {
//			DbgPrint("TDI Request observed: function code %x\r\n", StackIrpPointer->MinorFunction);
//
//			if (StackIrpPointer->MinorFunction == TDI_SEND) {
//				DbgPrint("Application sent a TCP-Data packet of size %d.\r\n", StackIrpPointer->Parameters.DeviceIoControl.OutputBufferLength);
//			}
//
//			IoSkipCurrentIrpStackLocation(Irp);
//			ntStatus = IoCallDriver(devExt->TopOfDeviceStack, Irp);
//		}
//
//	}
//
//	return ntStatus;
//}

NTSTATUS Dispatch1(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PEXTENSION_OBJECT DeviceExtension = (PEXTENSION_OBJECT)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION StackIrpPointer = NULL;

	PTDI_REQUEST_KERNEL_CONNECT TDI_connectRequest;
	PTA_ADDRESS TA_Address_data;
	PTDI_ADDRESS_IP TDI_data;

	typedef struct _NETWORK_ADDRESS
	{
		unsigned char address[4];
		unsigned char port[2];
	} NETWORK_ADDRESS;

	NETWORK_ADDRESS data;

	unsigned short Port = 0;
	unsigned long Address = 0;

	StackIrpPointer = IoGetCurrentIrpStackLocation(Irp);

	if (!StackIrpPointer) {
		DbgPrint("Fatal Error: IRP stack pointer is NULL! \r\n");
		return STATUS_UNSUCCESSFUL;
	}

	if (DeviceObject == DeviceToBeFiltered) {
		if (StackIrpPointer->MinorFunction == TDI_CONNECT) {
			TDI_connectRequest = (PTDI_REQUEST_KERNEL_CONNECT)(&StackIrpPointer->Parameters);
			TA_Address_data = ((PTRANSPORT_ADDRESS)(TDI_connectRequest->RequestConnectionInformation->RemoteAddress))->Address;
			TDI_data = (PTDI_ADDRESS_IP)(TA_Address_data->Address);
			Address = TDI_data->in_addr;
			Port = TDI_data->sin_port;
			data.address[0] = ((char *)&Address)[0];
			data.address[1] = ((char *)&Address)[1];
			data.address[2] = ((char *)&Address)[2];
			data.address[3] = ((char *)&Address)[3];
			data.port[0] = ((char *)&Port)[0];
			data.port[1] = ((char *)&Port)[1];
			Port = data.port[0] + data.port[1];

			DbgPrint("TCP address is %d.%d.%d.%d:%d \r\n", data.address[0], data.address[1], data.address[2], data.address[3], Port);

			IoSkipCurrentIrpStackLocation(Irp);
			ntStatus = IoCallDriver(DeviceExtension->TopOfDeviceStack, Irp);
		}
		else {
			IoSkipCurrentIrpStackLocation(Irp);
			ntStatus = IoCallDriver(DeviceExtension->TopOfDeviceStack, Irp);
		}
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

		DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = Dispatch1;

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