/* Incluye el logotipo oficial PNG como imagen de demostracion en la galeria. */
.section .rodata
.balign 4
.globl vortex_demo_png_start
.globl vortex_demo_png_end
vortex_demo_png_start:
.incbin "../docs/vortex-logo.png"
vortex_demo_png_end:
.section .note.GNU-stack,"",@progbits
