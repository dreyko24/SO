/* Traduce informes HID Boot a la misma entrada Set 1 que usa PS/2. */
#include "usb_hid.h"
#include "vortex.h"

#define EXT(code) (0x80 | (code))

/* Tabla indexada por Usage ID de la página Keyboard/Keypad (0x07). */
static const uint8_t set1[256]={
    [0x04]=0x1e, [0x05]=0x30, [0x06]=0x2e, [0x07]=0x20, /* A-D */
    [0x08]=0x12, [0x09]=0x21, [0x0a]=0x22, [0x0b]=0x23,
    [0x0c]=0x17, [0x0d]=0x24, [0x0e]=0x25, [0x0f]=0x26,
    [0x10]=0x32, [0x11]=0x31, [0x12]=0x18, [0x13]=0x19,
    [0x14]=0x10, [0x15]=0x13, [0x16]=0x1f, [0x17]=0x14,
    [0x18]=0x16, [0x19]=0x2f, [0x1a]=0x11, [0x1b]=0x2d,
    [0x1c]=0x15, [0x1d]=0x2c,
    [0x1e]=0x02, [0x1f]=0x03, [0x20]=0x04, [0x21]=0x05, /* 1-0 */
    [0x22]=0x06, [0x23]=0x07, [0x24]=0x08, [0x25]=0x09,
    [0x26]=0x0a, [0x27]=0x0b,
    [0x28]=0x1c, [0x29]=0x01, [0x2a]=0x0e, [0x2b]=0x0f, /* Enter, Esc, Retroceso, Tab */
    [0x2c]=0x39, [0x2d]=0x0c, [0x2e]=0x0d, [0x2f]=0x1a,
    [0x30]=0x1b, [0x31]=0x2b, [0x32]=0x2b, [0x33]=0x27,
    [0x34]=0x28, [0x35]=0x29, [0x36]=0x33, [0x37]=0x34,
    [0x38]=0x35, [0x39]=0x3a, /* /, Bloq Mayús */
    [0x3a]=0x3b, [0x3b]=0x3c, [0x3c]=0x3d, [0x3d]=0x3e, /* F1-F12 */
    [0x3e]=0x3f, [0x3f]=0x40, [0x40]=0x41, [0x41]=0x42,
    [0x42]=0x43, [0x43]=0x44, [0x44]=0x57, [0x45]=0x58,
    [0x47]=0x46, /* Bloq Despl */
    [0x49]=EXT(0x52), [0x4a]=EXT(0x47), [0x4b]=EXT(0x49), /* Insert, Inicio, RePág */
    [0x4c]=EXT(0x53), [0x4d]=EXT(0x4f), [0x4e]=EXT(0x51),
    [0x4f]=EXT(0x4d), [0x50]=EXT(0x4b), [0x51]=EXT(0x50), [0x52]=EXT(0x48),
    [0x53]=0x45, [0x54]=EXT(0x35), [0x55]=0x37, [0x56]=0x4a, /* Teclado numérico */
    [0x57]=0x4e, [0x58]=EXT(0x1c), [0x59]=0x4f, [0x5a]=0x50,
    [0x5b]=0x51, [0x5c]=0x4b, [0x5d]=0x4c, [0x5e]=0x4d,
    [0x5f]=0x47, [0x60]=0x48, [0x61]=0x49, [0x62]=0x52,
    [0x63]=0x53, [0x64]=0x56, [0x65]=EXT(0x5d), /* Decimal, tecla ISO, Menú */
    [0xe0]=0x1d, [0xe1]=0x2a, [0xe2]=0x38, [0xe3]=EXT(0x5b), /* Modificadores */
    [0xe4]=EXT(0x1d), [0xe5]=0x36, [0xe6]=EXT(0x38), [0xe7]=EXT(0x5c),
};

/* Dos interfaces USB pueden mantener la misma tecla o botón pulsados. */
static unsigned key_holders[256], button_holders[3];

static void emit_key(uint8_t scan,int pressed){
    usb_hid_key_event(scan&0x7f,!!(scan&0x80),pressed);
}

static void change_key(uint8_t usage,int pressed){
    uint8_t scan=set1[usage];
    if(!scan)return;
    if(pressed){
        if(!key_holders[scan]++)emit_key(scan,1);
    }else if(key_holders[scan] && !--key_holders[scan])emit_key(scan,0);
}

static int contains(const uint8_t *keys,unsigned count,uint8_t usage){
    for(unsigned i=0;i<count;i++)if(keys[i]==usage)return 1;
    return 0;
}

static int repeatable(uint8_t usage){
    return (usage>=0x04 && usage<=0x27) || usage==0x2a ||
           (usage>=0x2c && usage<=0x38) || (usage>=0x4b && usage<=0x52) ||
           (usage>=0x54 && usage<=0x64);
}

static void keyboard_report(UsbHidState *state,const uint8_t *report){
    const uint8_t *keys=report+2;
    unsigned n=sizeof state->keys;
    int rollover=0;
    for(unsigned i=0;i<n;i++)if(keys[i] && keys[i]<=3)rollover=1;
    state->rollover=rollover;
    if(!rollover){
        for(unsigned i=0;i<n;i++){
            uint8_t usage=state->keys[i];
            if(usage && !contains(state->keys,i,usage) && !contains(keys,n,usage))change_key(usage,0);
        }
        if(state->repeat_usage && !contains(keys,n,state->repeat_usage))state->repeat_usage=0;
    }
    uint8_t changed=state->modifiers^report[0];
    for(unsigned i=0;i<8;i++)if(changed&(1u<<i))change_key(0xe0+i,!!(report[0]&(1u<<i)));
    state->modifiers=report[0];
    if(rollover)return; /* Los modificadores siguen siendo válidos. */
    for(unsigned i=0;i<n;i++){
        uint8_t usage=keys[i];
        if(!usage || contains(keys,i,usage) || contains(state->keys,n,usage))continue;
        change_key(usage,1);
        if(repeatable(usage)){
            state->repeat_usage=usage;
            state->repeat_at=kticks()+50;
        }
    }
    for(unsigned i=0;i<n;i++)state->keys[i]=keys[i];
}

static void mouse_report(UsbHidState *state,int dx,int dy,int buttons,int wheel){
    int changed=state->buttons^buttons, combined=0;
    state->buttons=buttons;
    for(unsigned i=0;i<3;i++){
        int mask=1<<i;
        if(changed&mask){
            if(buttons&mask)button_holders[i]++;
            else if(button_holders[i])button_holders[i]--;
        }
        if(button_holders[i])combined|=mask;
    }
    if(dx || dy || changed || wheel)usb_hid_mouse_event(dx,dy,combined,wheel);
}

void usb_hid_report(UsbHidState *state,unsigned protocol,const uint8_t *report,unsigned bytes){
    if(protocol==USB_HID_KEYBOARD && bytes>=8)keyboard_report(state,report);
    if(protocol==USB_HID_MOUSE && bytes>=3)
        mouse_report(state,(int8_t)report[1],(int8_t)report[2],report[0]&7,bytes>=4?(int8_t)report[3]:0);
}

void usb_hid_release(UsbHidState *state,unsigned protocol){
    static const uint8_t empty[8];
    if(protocol==USB_HID_KEYBOARD)keyboard_report(state,empty);
    if(protocol==USB_HID_MOUSE)mouse_report(state,0,0,0,0);
}

void usb_hid_repeat(UsbHidState *state,unsigned protocol){
    if(protocol!=USB_HID_KEYBOARD || !state->repeat_usage || state->rollover)return;
    /* Ctrl, Alt izquierdo y Super participan en atajos del escritorio. */
    if(state->modifiers&0x9d)return;
    uint32_t now=kticks();
    if((int32_t)(now-state->repeat_at)<0)return;
    emit_key(set1[state->repeat_usage],1);
    state->repeat_at=now+4;
}
