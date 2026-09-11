;; rw_layout.inc -- the RenderWare style pipes' VU1 data memory above the
;; buffers (defaultPipe, im2d, im3d).  The C side is the enum in
;; src/xtcpipe.h.  The std pipes have std_layout.h instead.
.equ light,		0x3d0
.equ matrix0,		0x3f0
.equ matrix1,		0x3f1
.equ matrix2,		0x3f2
.equ matrix3,		0x3f3
.equ xyzwScale,		0x3f4
.equ xyzwOffset,	0x3f5
.equ clipConsts,	0x3f6
.equ gifTag,		0x3fa
.equ colorScale,	0x3fb
.equ surfaceProps,	0x3fc
.equ codeSwitch,	0x3ff
