#ifndef _USB_H_
#define _USB_H_

#include "common/datatype.h"

extern bool usb_open();
extern void usb_close();
extern bool usb_activate();
extern bool usb_deactivate();
extern bool usb_isactive();
extern bool usb_cableconnected();
extern bool usb_connectionestablished();

#endif
