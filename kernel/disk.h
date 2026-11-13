/* disk.h — disco ATA (IDE, PIO) y persistencia del sistema de archivos en el formato KNGFS */
#ifndef DISK_H
#define DISK_H
#include "vortex.h"
int      disk_init(void);                 /* busca el primer disco IDE o SATA (AHCI); 0 si lo hay */
uint32_t disk_sectors(void);              /* tamano en sectores de 512 B */
const char *disk_model(void);
const char *disk_kind(void);              /* "IDE", "SATA (AHCI)" o "ninguno" */
int      disk_read(uint32_t lba, void *buf, int n);
int      disk_write(uint32_t lba, const void *buf, int n);
/* persistencia: el disco solo se usa si ya tiene la firma KNGFS o si el usuario ejecuta "disco formatear si" */
int      vortexfs_mount(void);               /* 0: firma valida, el arbol se cargo desde el disco; <0: no hay firma/datos */
int      vortexfs_save(void);                /* 0 ok */
int      vortexfs_format(void);              /* escribe firma y arbol actual (BORRA el inicio del disco) */
int      vortexfs_active(void);              /* 1 si hay un disco montado y se guarda solo */
void     vortexfs_tick(void);                /* guardado automatico: llamar seguido */
uint32_t vortexfs_last_size(void);           /* bytes de la ultima copia guardada */
extern void *(*vortex_mmio_map)(uint64_t phys, uint64_t len);   /* mapea MMIO por encima de 16 GB (kernel de 64 bits) */
extern const uint8_t *(*vortex_disk_image)(uint32_t *len);   /* imagen del instalador (la pone el kernel de 64 bits) */
int      vortex_install(void);               /* instala el sistema en el disco: 0 ok, <0 error */
#endif
