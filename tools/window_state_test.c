/* Ejercita el gestor REAL de ventanas con el simulador del escritorio.
 * El main del generador de capturas se renombra para reutilizar sus dispositivos falsos. */
#define main vortex_legacy_hostgui_main
#include "hostgui.c"
#undef main

static int failures;
#define CHECK(test,msg) do { if(!(test)){ fprintf(stderr,"WINDOW FAIL: %s\n",msg); failures++; } } while(0)

int main(void){
    fb=calloc(W*H,sizeof *fb);
    if(!fb || gui_init(fb,W,H,W*4)){ fprintf(stderr,"No se pudo iniciar GUI\n"); return 2; }
    Win *files=open_app(A_FILES), *notes=open_app(A_NOTES);
    CHECK(files&&notes&&files!=notes,"dos aplicaciones independientes");
    if(!files||!notes) return 2;
    int file_id=(int)(files-wins), note_id=(int)(notes-wins);
    CHECK(win_top()==note_id,"ultima ventana recibe el foco");
    gui_key(0x40D); /* Ctrl+M */
    CHECK(notes->minimized&&win_top()==file_id,"minimizar no cierra otra ventana");
    gui_key(0x502); /* Ctrl+Tab */
    CHECK(!notes->minimized&&win_top()==note_id,"Ctrl+Tab recupera una ventana minimizada");
    int x=notes->x,y=notes->y,w=notes->w,h=notes->h;
    win_toggle_max(notes);
    CHECK(notes->maxed && notes->x==DOCK_SIDE &&
          notes->y==BARH && notes->w==W-DOCK_SIDE && notes->h==H-BARH,
          "maximizado reserva exactamente la barra lateral");
    win_toggle_fullscreen(notes);
    CHECK(notes->fullscreen&&notes->x==0&&notes->y==0&&notes->w==W&&notes->h==H,
          "pantalla completa real sin reservar barra");
    CHECK(dock_hit(22,120)==-1,"el dock no captura eventos en pantalla completa");
    win_toggle_fullscreen(notes);
    CHECK(!notes->fullscreen&&notes->maxed&&notes->x==DOCK_SIDE,
          "salir de pantalla completa restaura estado maximizado");
    win_restore(notes);
    CHECK(!notes->maxed&&notes->x==x&&notes->y==y&&notes->w==w&&notes->h==h,
          "restaurar conserva geometria anterior");
    gui_key(0x501); /* F10: instancia nueva */
    int duplicate=win_top();
    CHECK(duplicate>=0&&duplicate!=note_id&&wins[duplicate].type==A_NOTES,
          "instancia independiente de la misma aplicacion");
    gui_key(0x417); /* Ctrl+W */
    CHECK(win_top()==note_id&&notes->used&&!wins[duplicate].used,
          "cerrar una instancia conserva la otra");
    CHECK(files->used,"otras aplicaciones continuan abiertas");
    /* Una actualizacion de Vortex Saber no debe borrar las notas historicas. */
    FsNode *doc=fs_resolve(fs_root(),"/Documentos");
    CHECK(doc&&doc->dir,"Documentos disponible para progreso educativo");
    if(doc){
        const char old_progress[]="32124\n";
        FsNode *f=fs_create(doc,"Vortex-Saber-progreso.txt",old_progress,6);
        CHECK(f!=0,"crear fichero de progreso anterior");
        saber_loaded=0;
        saber_load();
        CHECK(saber_best[0]==3&&saber_best[1]==2&&saber_best[2]==1&&
              saber_best[3]==2&&saber_best[4]==0&&saber_best[5]==0&&
              saber_best[NSABER]==4,"migrar progreso previo sin perder partida");
        saber_save();
        f=fs_find(doc,"Vortex-Saber-progreso.txt");
        CHECK(f&&f->size==NSABER+2,"guardar nuevo progreso de seis materias");
    }
    win_close(note_id);
    win_close(file_id);
    CHECK(win_top()==-1&&nord==0,"cerrar todas las ventanas libera el foco");
    free(fb);
    if(failures){ fprintf(stderr,"%d pruebas fallaron\n",failures); return 1; }
    puts("VORTEX WINDOW STATE OK");
    return 0;
}
