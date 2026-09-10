; chunk.inc -- writing xtc chunk files with the assembler.
;
; A chunk is a memory image plus a table of the places in it that hold
; pointers; the loader adds its load address to each of those.  The
; assembler and linker can do all of the bookkeeping: every pointer
; field goes through `ptr', which emits the value in place and records
; the field's own address in the .chkreloc section, and tools/chk.ld
; links everything at address 0 into a flat file with the header,
; the data, the fixup table and the global table in that order
; (see common/chunk.c for the loader).
;
;	ee-dvp-as -Itools model.dsm -o model.o
;	ee-ld -T tools/chk.ld -o model.chk model.o
;
; Sections: everything is data (dvp-as puts .data in .vudata; the
; script collects both).  MIPS .align is a power of two: .align 4 is
; a qword.

; the header, filled in by the linker script's symbols
.macro chkheader
.section .chkhdr, "a"
.int 0x41424344		; ident 'ABCD'
.int 0			; shrink
.int __chk_fileEnd
.int __chk_dataEnd
.int __chk_relocTab
.int __chk_numRelocs
.int __chk_globalTab
.int __chk_globalEnd
.previous
.endm

; a pointer field: its value, and its location in the fixup table.
; a nil pointer is a plain `.int 0', the loader must not touch it.
.macro ptr target
ptr\@:
.int \target
.section .chkreloc, "a"
.int ptr\@
.previous
.endm

; a DMA ref tag whose address (the tag's second word) is fixed up too,
; so a chain can ref into the chunk's own data
.macro chkref qwc, target
ref\@:
DMAref \qwc, \target
.section .chkreloc, "a"
.int ref\@ + 4
.previous
.endm

; a pointer to something outside the chunk, resolved by name at load
; time (a pipeline, a texture): a zero field and an entry in the global
; table of {location, class, name}, the strings inline, padded to a word.
; both arguments are quoted strings (the assembler strips the quotes
; when it substitutes them, the macro puts them back).
.macro global cls, name
glob\@:
.int 0
.section .chkglob, "a"
.int glob\@
.asciz "\cls"
.asciz "\name"
.align 2
.previous
.endm

; VIF UNPACK with an explicit count and no inline data, for ref'd
; payloads (the assembler's own unpack wants the data in place)
.macro unpackref cmd, imm, num
.int ((\cmd)<<24)|((\num)<<16)|(\imm)
.endm
