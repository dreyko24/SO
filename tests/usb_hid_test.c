#include <assert.h>
#include <stdio.h>
#include "../kernel/input_events.h"
#include "../kernel/usb_hid.h"

static struct KeyEvent { uint8_t code; int extended, pressed; } keys[32];
static struct MouseEvent { int dx, dy, buttons, wheel; } mouse[16];
static unsigned key_count, mouse_count;
static uint32_t now;

uint32_t kticks(void){ return now; }

void usb_hid_key_event(uint8_t scancode,int extended,int pressed){
    assert(key_count<sizeof keys/sizeof keys[0]);
    keys[key_count++]=(struct KeyEvent){scancode,extended,pressed};
}

void usb_hid_mouse_event(int dx,int dy,int buttons,int wheel){
    assert(mouse_count<sizeof mouse/sizeof mouse[0]);
    mouse[mouse_count++]=(struct MouseEvent){dx,dy,buttons,wheel};
}

static void expect_key(unsigned i,uint8_t code,int extended,int pressed){
    assert(i<key_count);
    assert(keys[i].code==code && keys[i].extended==extended && keys[i].pressed==pressed);
}

static void expect_mouse(unsigned i,int dx,int dy,int buttons,int wheel){
    assert(i<mouse_count);
    assert(mouse[i].dx==dx && mouse[i].dy==dy);
    assert(mouse[i].buttons==buttons && mouse[i].wheel==wheel);
}

static void keyboard_transitions(void){
    UsbHidState state={0};
    const uint8_t held[]={2,0,4,4,0,0,0,0}; /* Shift + A, incluida una repetición en el array. */
    const uint8_t rollover[]={2,0,1,1,1,1,1,1};
    const uint8_t replaced[]={0,0,0x4f,0,0,0,0,0}; /* Flecha derecha. */
    key_count=0;
    usb_hid_report(&state,USB_HID_KEYBOARD,held,7);
    assert(key_count==0);
    usb_hid_report(&state,USB_HID_KEYBOARD,held,sizeof held);
    assert(key_count==2);
    expect_key(0,0x2a,0,1);
    expect_key(1,0x1e,0,1);
    usb_hid_report(&state,USB_HID_KEYBOARD,rollover,sizeof rollover);
    usb_hid_report(&state,USB_HID_KEYBOARD,held,sizeof held);
    assert(key_count==2);
    usb_hid_report(&state,USB_HID_KEYBOARD,replaced,sizeof replaced);
    assert(key_count==5);
    expect_key(2,0x1e,0,0);
    expect_key(3,0x2a,0,0);
    expect_key(4,0x4d,1,1);
    usb_hid_release(&state,USB_HID_KEYBOARD);
    usb_hid_release(&state,USB_HID_KEYBOARD);
    assert(key_count==6);
    expect_key(5,0x4d,1,0);
}

static void shared_key_release(void){
    UsbHidState first={0}, second={0};
    const uint8_t held[]={0x40,0,0x04,0,0,0,0,0}; /* AltGr + A. */
    key_count=0;
    usb_hid_report(&first,USB_HID_KEYBOARD,held,sizeof held);
    usb_hid_report(&second,USB_HID_KEYBOARD,held,sizeof held);
    usb_hid_release(&first,USB_HID_KEYBOARD);
    assert(key_count==2);
    expect_key(0,0x38,1,1);
    usb_hid_release(&second,USB_HID_KEYBOARD);
    assert(key_count==4);
    expect_key(2,0x1e,0,0);
    expect_key(3,0x38,1,0);
}

static void repeat_while_held(void){
    UsbHidState state={0};
    const uint8_t held[]={0,0,0x2a,0,0,0,0,0}; /* Retroceso. */
    const uint8_t rollover[]={0,0,1,1,1,1,1,1};
    const uint8_t caps_lock[]={0,0,0x39,0,0,0,0,0};
    key_count=0;
    now=UINT32_MAX-30;
    usb_hid_report(&state,USB_HID_KEYBOARD,held,sizeof held);
    now+=25;
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==1);
    usb_hid_report(&state,USB_HID_KEYBOARD,rollover,sizeof rollover);
    now+=75;
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==1);
    usb_hid_report(&state,USB_HID_KEYBOARD,held,sizeof held);
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==2);
    expect_key(1,0x0e,0,1);
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==2);
    usb_hid_report(&state,USB_HID_KEYBOARD,caps_lock,sizeof caps_lock);
    now+=100;
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==4);
    expect_key(2,0x0e,0,0);
    expect_key(3,0x3a,0,1);
    usb_hid_release(&state,USB_HID_KEYBOARD);
    now+=100;
    usb_hid_repeat(&state,USB_HID_KEYBOARD);
    assert(key_count==5);
}

static void mouse_motion_wheel_and_detach(void){
    UsbHidState first={0}, second={0};
    const uint8_t move[]={0xf9,0xff,0x80};
    const uint8_t right_and_scroll[]={2,2,3,0xfe};
    mouse_count=0;
    usb_hid_report(&first,USB_HID_MOUSE,move,2);
    assert(mouse_count==0);
    usb_hid_report(&first,USB_HID_MOUSE,move,sizeof move);
    usb_hid_report(&second,USB_HID_MOUSE,right_and_scroll,sizeof right_and_scroll);
    usb_hid_release(&first,USB_HID_MOUSE);
    usb_hid_release(&second,USB_HID_MOUSE);
    usb_hid_release(&second,USB_HID_MOUSE);
    assert(mouse_count==4);
    expect_mouse(0,-1,-128,1,0);
    expect_mouse(1,2,3,3,-2);
    expect_mouse(2,0,0,2,0);
    expect_mouse(3,0,0,0,0);
}

static void combine_input_sources(void){
    VKeySources keys={0};
    assert(vkey_source(&keys,0x2a,0,INPUT_PS2));
    assert(!vkey_source(&keys,0x2a,0,INPUT_USB));
    assert(!vkey_source(&keys,0xaa,0,INPUT_USB));
    assert(vkey_source(&keys,0xaa,0,INPUT_PS2));
    assert(!vkey_source(&keys,0xaa,0,INPUT_PS2));
    assert(vkey_source(&keys,0x1d,1,INPUT_USB));
    assert(vkey_source(&keys,0x1d,1,INPUT_USB)); /* Repetición de la misma fuente. */
    assert(vkey_source(&keys,0x1d,0,INPUT_PS2)); /* Ctrl izquierdo y derecho. */
    assert(vkey_source(&keys,0x9d,1,INPUT_USB));
    assert(keys.held[0x1d]==INPUT_PS2);
}

static void release_after_overflow(void){
    VMouseQueue mouse_events={0};
    for(unsigned i=0;i<VMOUSE_QUEUE_SIZE-1;i++)
        vmouse_push(&mouse_events,(VMouseEvent){.dx=1,.buttons=1});
    vmouse_push(&mouse_events,(VMouseEvent){0}); /* Se pierde por falta de lugar. */
    VMouseEvent event;
    for(unsigned i=0;i<VMOUSE_QUEUE_SIZE-1;i++){
        assert(vmouse_pop(&mouse_events,&event)&&event.dx==1&&event.buttons==1);
    }
    assert(vmouse_pop(&mouse_events,&event)&&event.dx==0&&event.buttons==0);
    assert(!vmouse_pop(&mouse_events,&event));

    VKeySources keys={0};
    VRawKeyQueue key_events={0};
    assert(vkey_source(&keys,0x38,1,INPUT_USB));
    for(unsigned i=0;i<VKEY_RAW_SIZE-1;i++)vkey_raw_push(&key_events,0x38,1);
    assert(vkey_source(&keys,0xb8,1,INPUT_USB));
    vkey_raw_push(&key_events,0xb8,1); /* AltGr se soltó con el búfer lleno. */
    for(unsigned i=0;i<VKEY_RAW_SIZE-1;i++)assert(vkey_raw_pop(&key_events,&keys)==0x138);
    assert(vkey_raw_pop(&key_events,&keys)==0x1b8);
    assert(vkey_raw_pop(&key_events,&keys)==-1);
}

int main(void){
    keyboard_transitions();
    shared_key_release();
    repeat_while_held();
    mouse_motion_wheel_and_detach();
    combine_input_sources();
    release_after_overflow();
    puts("VORTEX USB HID: teclado, repetición, ratón y desconexión OK");
    return 0;
}
