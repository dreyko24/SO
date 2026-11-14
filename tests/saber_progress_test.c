#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/saber_progress.h"
int main(void){
    uint8_t image[SABER_PROGRESS_SIZE];
    VortexSaberProgress original={3,12,28,0x80000001u,7},restored={0};
    saber_encode(&original,image);
    assert(!saber_decode(image,sizeof image,&restored));
    assert(restored.best==12&&restored.completed==3&&restored.articles==0x80000001u&&restored.games_won==7);
    assert(saber_level(&restored)>1);
    assert(saber_read_count(restored.articles)==2);
    image[13]^=0x80;assert(saber_decode(image,sizeof image,&restored));
    image[13]^=0x80;
    assert(saber_decode(image,12,&restored));
    image[0]='X';assert(saber_decode(image,sizeof image,&restored));
    /* Compatibilidad de datos previos VSP1 tras actualizar Vortex. */
    uint8_t old[SABER_PROGRESS_OLD_SIZE]={0};
    memcpy(old,"VSP1",4);
    saber_put32(old+4,3);saber_put32(old+8,12);
    saber_put32(old+12,28);saber_put32(old+16,0x80000001u);
    saber_put32(old+20,saber_hash(old,20));
    assert(!saber_decode(old,sizeof old,&restored));
    assert(restored.games_won==0&&restored.completed==3);
    /* Datos reales heredados del Vortex Saber anterior:
     * seis materias, 3/3, 2/3, etc., y juego de parejas 4/4. */
    const uint8_t legacy6[]="3212034\n";
    VortexSaberProgress converted={0};
    assert(!saber_import_legacy_text(legacy6,8,&converted));
    assert(converted.completed==5&&converted.best==3&&converted.total_correct==11&&converted.games_won==1);
    saber_encode(&converted,image);
    memset(&restored,0,sizeof restored);
    assert(!saber_decode(image,sizeof image,&restored));
    assert(restored.completed==5&&restored.total_correct==11&&restored.games_won==1);
    const uint8_t legacy4[]="32124\n";
    assert(!saber_import_legacy_text(legacy4,6,&converted));
    assert(converted.total_correct==8&&converted.completed==4);
    assert(saber_import_legacy_text(legacy4,4,&converted)<0);
    assert(saber_import_legacy_text((const uint8_t*)"321X34\n",7,&converted)<0);
    puts("VORTEX SABER PROGRESS: VERSION V1/V2, HASH, JUEGOS Y NIVEL OK");
    return 0;
}
