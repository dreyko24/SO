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
    FsNode *welcome=fs_resolve(fs_root(),"/Documentos/bienvenida.txt");
    FsNode *version=fs_resolve(fs_root(),"/Sistema/version.txt");
    CHECK(welcome&&welcome->data&&strstr(welcome->data,"Bienvenido a Vortex OS")!=0,
          "el sistema de archivos inicial identifica Vortex OS");
    CHECK(version&&version->data&&strstr(version->data,"Vortex OS")!=0,
          "la version del sistema identifica Vortex OS");
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
    char launch[160];
    CHECK(!browser_chromium_command("https://www.google.com/maps",launch,sizeof launch) &&
          !strcmp(launch,"abrir vortex-chromium https://www.google.com/maps"),
          "abrir Chromium con URL https original");
    CHECK(!browser_chromium_command("kng://buscar?q=historia+del+peru",launch,sizeof launch) &&
          !strcmp(launch,"abrir vortex-chromium https://www.google.com/search?q=historia+del+peru"),
          "conservar una busqueda de Google");
    CHECK(browser_chromium_command("https://example.com/;reboot",launch,sizeof launch)<0,
          "bloquear caracteres de control de comandos en URL");
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
    CHECK(NSABER==6 && SABER_NQ==3,"Saber incluye seis materias y 18 preguntas");
    Win *school=open_app(A_SABER);
    CHECK(school!=0,"abrir plataforma educativa");
    if(school){
        const int answers[3]={0,1,2};
        for(int subject=4;subject<=5;subject++){
            saber_start(school,subject);
            CHECK(school->saber_topic==subject,"iniciar materia nueva");
            for(int q=0;q<3;q++){
                CHECK(SABER[subject].correcta[q]==answers[q],"clave de evaluacion correcta");
                saber_answer(school,answers[q]);
                CHECK(school->saber_hits==q+1,"puntaje refleja las respuestas");
                saber_next(school);
            }
            CHECK(saber_best[subject]==3,"dominio de la materia guardado");
        }
        FsNode *progress=fs_resolve(fs_root(),"/Documentos/Vortex-Saber-progreso.txt");
        CHECK(progress&&progress->size==NSABER+2&&progress->data[4]=='3'&&progress->data[5]=='3',
              "nuevo progreso educativo guardado para Geografia y Comunicacion");
        win_close((int)(school-wins));
    }
    win_close(note_id);
    win_close(file_id);
    CHECK(win_top()==-1&&nord==0,"cerrar todas las ventanas libera el foco");
    free(fb);
    if(failures){ fprintf(stderr,"%d pruebas fallaron\n",failures); return 1; }
    puts("VORTEX WINDOW STATE OK");
    return 0;
}
