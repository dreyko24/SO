#ifndef VORTEX_USB_HID_H
#define VORTEX_USB_HID_H

#include <stdint.h>

enum { USB_HID_KEYBOARD=1, USB_HID_MOUSE=2 };

/* Un estado inicializado a cero por interfaz Boot; liberar antes de reutilizarlo. */
typedef struct {
    uint32_t repeat_at;
    uint8_t keys[6];
    uint8_t modifiers, repeat_usage, rollover, buttons;
} UsbHidState;

void usb_hid_report(UsbHidState *state,unsigned protocol,const uint8_t *report,unsigned bytes);
void usb_hid_release(UsbHidState *state,unsigned protocol);
void usb_hid_repeat(UsbHidState *state,unsigned protocol);

/* Eventos del teclado en Set 1 y del ratón en coordenadas de pantalla. */
void usb_hid_key_event(uint8_t scancode,int extended,int pressed);
void usb_hid_mouse_event(int dx,int dy,int buttons,int wheel);

#endif
