#ifndef VORTEX_USB_STORAGE_H
#define VORTEX_USB_STORAGE_H
/* Controlador UHCI real + USB Mass Storage Bulk-Only/SCSI + importacion FAT32
 * SOLO LECTURA. No inventa una carpeta USB si no existe un dispositivo.
 * Soporta controladoras UHCI 1.1; XHCI/EHCI requieren controladores propios.
 */
int usb_storage_probe(void);      /* 1 si se importo FAT32 desde USB, 0 sin unidad, negativo error */
int usb_storage_ready(void);
const char *usb_storage_message(void);
void usb_storage_poll(void);
#endif
