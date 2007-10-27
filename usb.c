#include "config.h"

#ifdef ENABLE_USB

#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <kubridge.h>
#include "usb.h"

extern bool usb_open()
{
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/chkreg.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/mgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/npdrm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/semawm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstor.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstormgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstorms.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstorboot.prx", 0, NULL), 0, NULL, 0, NULL);

	if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0) != 0)
		return false;

	if (sceUsbStart( PSP_USBSTOR_DRIVERNAME, 0, 0) != 0)
		return false;

	if (sceUsbstorBootSetCapacity( 0x800000 ) != 0)
		return false;

	return true;
}

extern void usb_close()
{
	if (usb_isactive())
		usb_deactivate();

	sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);

	sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
}

extern bool usb_activate()
{
	if (usb_isactive())
		return false;

	return (sceUsbActivate(0x1c8) == 0);
}

extern bool usb_deactivate()
{
	if (usb_isactive())
		return (sceUsbDeactivate(0x1c8) == 0);

	return false;
}

extern bool usb_isactive()
{
	return (sceUsbGetState() & PSP_USB_ACTIVATED);
}

extern bool usb_cableconnected()
{
	return (sceUsbGetState() & PSP_USB_CABLE_CONNECTED);
}

extern bool usb_connectionestablished()
{
	return (sceUsbGetState() & PSP_USB_CONNECTION_ESTABLISHED);
}

#endif
