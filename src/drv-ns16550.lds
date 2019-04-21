/******************************************************************************/
/*                                                                            */
/* src/drv-ns16550.lds                                                        */
/*                                                                 2019/04/21 */
/* Copyright (C) 2019 Mochi.                                                  */
/*                                                                            */
/******************************************************************************/
OUTPUT_FORMAT( elf32-i386 )
ENTRY( main )

PHDRS {
    load PT_LOAD;
}

MEMORY {
    app (wxai) : ORIGIN = 0x40000000,
                 LENGTH = 0xBFFFFFFF
}

SECTIONS {
    .text   : { *( .text   ) } > app : load
    .rodata : { *( .rodata ) } > app : load
    .data   : { *( .data   ) } > app : load
    .bss    : { *( .bss    ) } > app : load
}


/******************************************************************************/
