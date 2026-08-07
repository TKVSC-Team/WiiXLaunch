OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc)
ENTRY(WiiXLaunch_Cemu_Init)

SECTIONS
{
    . = 0x0;

    .text : {
        *(.text.WiiXLaunch_Cemu_Init)
        *(.text*)
        *(.init)
        *(.fini)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
        *(.sdata*)
        *(.got2*)
    }

    .bss : {
        *(.bss*)
        *(.sbss*)
    }

    PROVIDE(__end__ = .);
    PROVIDE(end = .);

    .rela.text : { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
    .rela.data : { *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*) }
    .rela.rodata : { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
    }
}
