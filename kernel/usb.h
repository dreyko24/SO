/* Deteccion PCI no invasiva de controladores USB.
 * La enumeracion UHCI y la importacion de archivos se hacen en usb_storage.c.
 */
#ifndef VORTEX_USB_H
#define VORTEX_USB_H
int usb_controllers(void);
int usb_first_controller_type(void); /* 0 UHCI, 0x10 OHCI, 0x20 EHCI, 0x30 XHCI, -1 ninguno */
const char *usb_controller_name(int type);
#endif
