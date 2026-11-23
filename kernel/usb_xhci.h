#ifndef VORTEX_USB_XHCI_H
#define VORTEX_USB_XHCI_H

#include <stdint.h>

void usb_xhci_poll(void);
void usb_xhci_service(void); /* entrada desde el bucle de escritorio */
const char *usb_xhci_message(void);

unsigned usb_xhci_storage_next(unsigned after,int retry);
int usb_xhci_storage_open(unsigned slot);
uint32_t usb_xhci_storage_generation(unsigned slot); /* 0 si ya no está conectado */
int usb_xhci_storage_transfer(unsigned slot,int in,uint8_t *buffer,unsigned bytes);
int usb_xhci_storage_reset(unsigned slot);

#endif
