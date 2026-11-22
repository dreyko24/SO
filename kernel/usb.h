/* Deteccion PCI no invasiva de controladores USB.
 * La enumeracion UHCI y la importacion de archivos se hacen en usb_storage.c.
 */
#ifndef VORTEX_USB_H
#define VORTEX_USB_H
#include <stdint.h>
int usb_controllers(void);
int usb_first_controller_type(void); /* 0 UHCI, 0x10 OHCI, 0x20 EHCI, 0x30 XHCI, -1 ninguno */
const char *usb_controller_name(int type);
#ifndef HOSTGUI
uint64_t usb_pci_enable_xhci(void); /* BAR0 MMIO asignada por el firmware, o 0 */
#endif
#endif
