	.set noreorder

#include "pspstub.s"

	STUB_START "xrPrx",0x40090000,0x00020005
	STUB_FUNC  0x5C1DE263,xrPlayerSetSpeed
	STUB_FUNC  0x9947E189,FindProc
	STUB_END
