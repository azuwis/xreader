#ifndef _USB_H_
#define _USB_H_

#include "common/datatype.h"

extern bool usb_open(void);
extern void usb_close(void);
extern bool usb_activate(void);
extern bool usb_deactivate(void);
extern bool usb_isactive(void);
extern bool usb_cableconnected(void);
extern bool usb_connectionestablished(void);

#endif
