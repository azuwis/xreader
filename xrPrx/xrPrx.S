	.set noreorder

#include "pspstub.s"

	STUB_START "xrPrx",0x40090000,0x00050005
	STUB_FUNC  0x5C1DE263,xrPlayerSetSpeed
	STUB_FUNC  0x9947E189,FindProc
	STUB_FUNC  0xF03EF61F,xrGetBrightness
	STUB_FUNC  0x97CCB320,xrSetBrightness
	STUB_FUNC  0xB6D18151,xrKernelLoadExecVSHMsX
	STUB_END
