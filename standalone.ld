OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

ENTRY(Reset)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 128M
	SHMEM (rwx) : ORIGIN = 0x40005000, LENGTH = 16M
}

SECTIONS
{
    .text :
    {
        *(vectors)
        *(.text)
        *(.rodata*)
        *(.constdata*)
        _etext = .;
		
		
    } > RAM

    .data : 
    {
        _data = .;
        *(vtable)
        *(.data)
        _edata = .;
    } > RAM

    .bss :
    {
        _bss = .;
        *(.bss)
        _ebss = .;
    } > RAM
	
	/* shared objects used by img_split */
	/* to store the separate image blocks */
	.ImgSplitShdMem :
    {
       *(.ImgSplitShdMem)
    } >SHMEM
	
}
