OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc)
ENTRY(WiiXLaunch_Cemu_Init)

SECTIONS
{
    . = 0x0;

    .text : {
        /* WiiXLaunch_Cemu_Init hand-computes its own runtime-vs-link-time
           delta (see the asm block in src/main.cpp) using @h/@l immediates
           that are DELIBERATELY left as raw link-time constants - the code
           adds the delta itself right after. These two markers let the
           deploy-time relocator (scripts/deploy.py) recognize and skip that
           narrow instruction range, since "fixing" those absolute refs like
           any other would double-apply the delta and zero out g_CodeCaveBase. */
        __wiixl_bootstrap_start = .;
        *(.text.WiiXLaunch_Cemu_Init)
        __wiixl_bootstrap_end = .;
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
