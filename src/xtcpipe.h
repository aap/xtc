
enum {
	vuLight		= 0x3D0,
	vuMatrix	= 0x3F0,
	vuXyzwScale	= 0x3F4,
	vuXyzwOffset	= 0x3F5,
	vuClipConsts	= 0x3F6,
		//
		//
		//
	vuGifTag	= 0x3FA,
	vuColorScale	= 0x3FB,
	vuSurfaceProps	= 0x3FC,
		//
		//
	vuCodeSwitch	= 0x3FF
};

// the vertex GIF tag every pipe hands the microcode
enum { xtcpVertRegs = GIF_ST | GIF_RGBAQ<<4 | GIF_XYZF2<<8 };

extern int primSize[];
extern int primRepeat[];
