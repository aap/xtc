/*
 * chk.ld -- link assembler-written data into an xtc chunk file.
 * The layout the loader in common/chunk.c expects: the 32 byte header
 * (tools/chunk.inc's chkheader), the data, the fixup table, the
 * global table.  Everything at address 0, so a `ptr' field holds the
 * file offset of its target, which is what the loader adds its base to.
 *
 *	ee-ld -T tools/chk.ld -o model.chk model.o
 */
OUTPUT_FORMAT("binary")
SECTIONS {
	. = 0;
	.chkhdr : { *(.chkhdr) }
	.data : { *(.data) *(.vudata) }
	. = ALIGN(16);
	__chk_dataEnd = .;
	__chk_relocTab = .;
	.chkreloc : { *(.chkreloc) }
	__chk_numRelocs = SIZEOF(.chkreloc) / 4;
	__chk_globalTab = .;
	.chkglob : { *(.chkglob) }
	__chk_globalEnd = .;
	__chk_fileEnd = .;
	/DISCARD/ : { *(.text) *(.bss) *(.reginfo) *(.mdebug*) *(.comment) }
}
