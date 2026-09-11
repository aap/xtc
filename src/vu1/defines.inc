
.equ UNPACK_V1_32,	0x60
.equ UNPACK_V1_16,	0x61
.equ UNPACK_V1_8,	0x62
.equ UNPACK_V2_32,	0x64
.equ UNPACK_V2_16,	0x65
.equ UNPACK_V2_8,	0x66
.equ UNPACK_V3_32,	0x68
.equ UNPACK_V3_16,	0x69
.equ UNPACK_V3_8,	0x6a
.equ UNPACK_V4_32,	0x6c
.equ UNPACK_V4_16,	0x6d
.equ UNPACK_V4_8,	0x6e
.equ UNPACK_V4_5,	0x6f

.equ UNPACK_MSK,	0x0010
.equ UNPACK_USN,	0x4000

;; NB: keep in synch with xtc.h
.equ XTCP_POSITION,	1
.equ XTCP_TEXCOORD,	2
.equ XTCP_COLOR,	3
.equ XTCP_NORMAL,	4
.equ XTCP_SKINDATA,	5


;; FCAND masks -- if register is 0, the whole poly is out
.equ posXn, (0x041041)
.equ negXn, (0x082082)
.equ posYn, (0x104104)
.equ negYn, (0x208208)
.equ posZn, (0x410410)
.equ negZn, (0x820820)
.equ posXn3, (posXn&0x3FFFF)
.equ negXn3, (negXn&0x3FFFF)
.equ posYn3, (posYn&0x3FFFF)
.equ negYn3, (negYn&0x3FFFF)
.equ posZn3, (posZn&0x3FFFF)
.equ negZn3, (negZn&0x3FFFF)
.equ posXn2, (posXn&0xFFF)
.equ negXn2, (negXn&0xFFF)
.equ posYn2, (posYn&0xFFF)
.equ negYn2, (negYn&0xFFF)
.equ posZn2, (posZn&0xFFF)
.equ negZn2, (negZn&0xFFF)
.equ posXn1, (posXn&0x3F)
.equ negXn1, (negXn&0x3F)
.equ posYn1, (posYn&0x3F)
.equ negYn1, (negYn&0x3F)
.equ posZn1, (posZn&0x3F)
.equ negZn1, (negZn&0x3F)


;; FCOR masks -- if register is 1, the whole poly is in
.equ posXo, (0xFBEFBE)
.equ negXo, (0xF7DF7D)
.equ posYo, (0xEFBEFB)
.equ negYo, (0xDF7DF7)
.equ posZo, (0xBEFBEF)
.equ negZo, (0x7DF7DF)
.equ posXo3, (posXo|0xFC0000)
.equ negXo3, (negXo|0xFC0000)
.equ posYo3, (posYo|0xFC0000)
.equ negYo3, (negYo|0xFC0000)
.equ posZo3, (posZo|0xFC0000)
.equ negZo3, (negZo|0xFC0000)
.equ posXo2, (posXo|0xFFF000)
.equ negXo2, (negXo|0xFFF000)
.equ posYo2, (posYo|0xFFF000)
.equ negYo2, (negYo|0xFFF000)
.equ posZo2, (posZo|0xFFF000)
.equ negZo2, (negZo|0xFFF000)
.equ posXo1, (posXo|0xFFFFC0)
.equ negXo1, (negXo|0xFFFFC0)
.equ posYo1, (posYo|0xFFFFC0)
.equ negYo1, (negYo|0xFFFFC0)
.equ posZo1, (posZo|0xFFFFC0)
.equ negZo1, (negZo|0xFFFFC0)
