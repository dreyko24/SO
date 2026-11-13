# Traspaso: Vortex OS (vortex) v21
Base v19. Cambios: gfx clip_base (rectangulo sucio) + gui.c dmark/dwin/render parcial; sombras por bandas; oclusion; glyph rapido; fb_wc() en kernel.c y kernel64.c (PAT1=WC, PWT sin PCD en las paginas del framebuffer; se omite si la e1000 comparte pagina);
tools/vortex-proxy.py (DuckDuckGo lite + /go + /g/<id>; stdlib) y buscador=http://10.0.2.2:8080/search?q=%s por defecto en /Sistema/vortex.cfg; https:// escrito en la barra se envia a /go; iconos/widgets arrastrables (DObj); maximizar/ajustar ventanas.
Verificado en host: hosttest, nettest, hostgui (arrastre, maximizar, ajustar, buscador). NUNCA arrancado en VM por mi parte; la mejora de velocidad (WC) no se pudo medir aqui.

Ademas (v20b): net_pcnet_virtio.h (PCnet + virtio-net, sin probar en hardware/VM real), diagnostico PCI en net_info, disk.c (ATA PIO + KNGFS, probado solo con disco simulado HOSTDISK), fs_rename/copy/move/serialize, comandos cp/mv/ren/df/disco/guardar, Monitor, motores en vortex-proxy (ddg/brave/google/bing).
