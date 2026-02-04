/* E# 独立运行时链接器脚本 */

ENTRY(mainCRTStartup)

SECTIONS
{
    . = 0x140000000;  /* Windows x64 默认基址 */
    
    /* 代码段 */
    .text : {
        *(.text.startup)  /* 启动代码在最前面 */
        *(.text)
        *(.text.*)
    }
    
    /* 只读数据段 */
    .rdata : {
        *(.rdata)
        *(.rdata.*)
        *(.rodata)
        *(.rodata.*)
    }
    
    /* 数据段 */
    .data : {
        *(.data)
        *(.data.*)
    }
    
    /* BSS 段 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note)
        *(.note.*)
    }
}
