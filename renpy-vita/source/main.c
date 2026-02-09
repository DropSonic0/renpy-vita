#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Python.h>
#include <SDL2/SDL.h>

#ifdef __psp2__
#include <psp2/sysmodule.h>
#include <psp2/power.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/vshbridge.h> 
#include <gpu_es4/psp2_pvr_hint.h>

int _newlib_heap_size_user = 180 * 1024 * 1024;
unsigned int sceLibcHeapSize = 10 * 1024 * 1024;
#endif

#ifdef __PS3__
#include <sys/process.h>
#include <sys/memory.h>
#include <sysutil/sysutil.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <lv2/sysfs.h>

/* Set process parameters: Priority 1001, 16MB stack size */
SYS_PROCESS_PARAM(1001, 0x1000000);
#endif

#define MAX_PATH 256

PyMODINIT_FUNC initpygame_sdl2_color();
PyMODINIT_FUNC initpygame_sdl2_controller();
PyMODINIT_FUNC initpygame_sdl2_display();
PyMODINIT_FUNC initpygame_sdl2_draw();
PyMODINIT_FUNC initpygame_sdl2_error();
PyMODINIT_FUNC initpygame_sdl2_event();
PyMODINIT_FUNC initpygame_sdl2_gfxdraw();
PyMODINIT_FUNC initpygame_sdl2_image();
PyMODINIT_FUNC initpygame_sdl2_joystick();
PyMODINIT_FUNC initpygame_sdl2_key();
PyMODINIT_FUNC initpygame_sdl2_locals();
PyMODINIT_FUNC initpygame_sdl2_mouse();
PyMODINIT_FUNC initpygame_sdl2_power();
PyMODINIT_FUNC initpygame_sdl2_pygame_time();
PyMODINIT_FUNC initpygame_sdl2_rect();
PyMODINIT_FUNC initpygame_sdl2_render();
PyMODINIT_FUNC initpygame_sdl2_rwobject();
PyMODINIT_FUNC initpygame_sdl2_scrap();
PyMODINIT_FUNC initpygame_sdl2_surface();
PyMODINIT_FUNC initpygame_sdl2_transform();
PyMODINIT_FUNC init_renpy();
PyMODINIT_FUNC init_renpybidi();
PyMODINIT_FUNC initrenpy_audio_renpysound();
PyMODINIT_FUNC initrenpy_display_accelerator();
PyMODINIT_FUNC initrenpy_display_render();
PyMODINIT_FUNC initrenpy_display_matrix();
PyMODINIT_FUNC initrenpy_gl_gldraw();
PyMODINIT_FUNC initrenpy_gl_glenviron_shader();
PyMODINIT_FUNC initrenpy_gl_glrtt_copy();
PyMODINIT_FUNC initrenpy_gl_glrtt_fbo();
PyMODINIT_FUNC initrenpy_gl_gltexture();
PyMODINIT_FUNC initrenpy_gl2_gl2draw();
PyMODINIT_FUNC initrenpy_gl2_gl2mesh();
PyMODINIT_FUNC initrenpy_gl2_gl2mesh2();
PyMODINIT_FUNC initrenpy_gl2_gl2mesh3();
PyMODINIT_FUNC initrenpy_gl2_gl2model();
PyMODINIT_FUNC initrenpy_gl2_gl2polygon();
PyMODINIT_FUNC initrenpy_gl2_gl2shader();
PyMODINIT_FUNC initrenpy_gl2_gl2texture();
PyMODINIT_FUNC initrenpy_parsersupport();
PyMODINIT_FUNC initrenpy_pydict();
PyMODINIT_FUNC initrenpy_style();
PyMODINIT_FUNC initrenpy_styledata_style_activate_functions();
PyMODINIT_FUNC initrenpy_styledata_style_functions();
PyMODINIT_FUNC initrenpy_styledata_style_hover_functions();
PyMODINIT_FUNC initrenpy_styledata_style_idle_functions();
PyMODINIT_FUNC initrenpy_styledata_style_insensitive_functions();
PyMODINIT_FUNC initrenpy_styledata_style_selected_activate_functions();
PyMODINIT_FUNC initrenpy_styledata_style_selected_functions();
PyMODINIT_FUNC initrenpy_styledata_style_selected_hover_functions();
PyMODINIT_FUNC initrenpy_styledata_style_selected_idle_functions();
PyMODINIT_FUNC initrenpy_styledata_style_selected_insensitive_functions();
PyMODINIT_FUNC initrenpy_styledata_styleclass();
PyMODINIT_FUNC initrenpy_styledata_stylesets();
PyMODINIT_FUNC initrenpy_text_ftfont();
PyMODINIT_FUNC initrenpy_text_textsupport();
PyMODINIT_FUNC initrenpy_text_texwrap();
PyMODINIT_FUNC initrenpy_uguu_gl();
PyMODINIT_FUNC initrenpy_uguu_uguu();

char app_dir_path[0x100];
char app_program_path[0x100];
char python_home_buffer[0x400];
char python_snprintf_buffer[0x400];
char python_script_buffer[0x400];
char title_id[0xA];

#ifdef __PS3__
extern void ps3_init_logger(s32 fd);
extern void _log(const char *fmt, ...);
#define printf _log
#endif

void show_error_and_exit(const char* message)
{
    _log("FATAL ERROR: %s\n", message);
    Py_Exit(1);
}

int main(int argc, char* argv[])
{
#ifdef __PS3__
    s32 log_fd = -1;
    s32 log_res = sysLv2FsOpen("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, &log_fd, 0666, NULL, 0);
    if (log_res == 0) {
        ps3_init_logger(log_fd);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    _log("\n\n****************************************\n");
    _log("Ren'Py PS3: [BUILD V36-64BIT-STUBS] STARTING\n");
    _log("****************************************\n\n");
    _log("Ren'Py PS3: log_fd = %d\n", (int)log_fd);
#endif

    Py_NoSiteFlag = 1;
    Py_IgnoreEnvironmentFlag = 0;
    Py_NoUserSiteDirectory = 1;
    Py_OptimizeFlag = 0;
    Py_VerboseFlag = 2;
    Py_HashRandomizationFlag = 0;
    Py_InteractiveFlag = 0;

#ifdef __PS3__
    strncpy(title_id, "RENPY0001", sizeof(title_id));
    title_id[sizeof(title_id) - 1] = '\0';
    snprintf(app_dir_path, sizeof(app_dir_path), "/dev_hdd0/game/%s/USRDIR", title_id);
    snprintf(app_program_path, sizeof(app_program_path), "%s/EBOOT.BIN", app_dir_path);

    _log("Ren'Py PS3: App dir path: %s\n", app_dir_path);
    Py_SetProgramName(app_program_path);
#endif

    static struct _inittab builtins[] = {
        {"pygame_sdl2.color", initpygame_sdl2_color},
        {"pygame_sdl2.controller", initpygame_sdl2_controller},
        {"pygame_sdl2.display", initpygame_sdl2_display},
        {"pygame_sdl2.draw", initpygame_sdl2_draw},
        {"pygame_sdl2.error", initpygame_sdl2_error},
        {"pygame_sdl2.event", initpygame_sdl2_event},
        {"pygame_sdl2.gfxdraw", initpygame_sdl2_gfxdraw},
        {"pygame_sdl2.image", initpygame_sdl2_image},
        {"pygame_sdl2.joystick", initpygame_sdl2_joystick},
        {"pygame_sdl2.key", initpygame_sdl2_key},
        {"pygame_sdl2.locals", initpygame_sdl2_locals},
        {"pygame_sdl2.mouse", initpygame_sdl2_mouse},
        {"pygame_sdl2.power", initpygame_sdl2_power},
        {"pygame_sdl2.pygame_time", initpygame_sdl2_pygame_time},
        {"pygame_sdl2.rect", initpygame_sdl2_rect},
        {"pygame_sdl2.render", initpygame_sdl2_render},
        {"pygame_sdl2.rwobject", initpygame_sdl2_rwobject},
        {"pygame_sdl2.scrap", initpygame_sdl2_scrap},
        {"pygame_sdl2.surface", initpygame_sdl2_surface},
        {"pygame_sdl2.transform", initpygame_sdl2_transform},
        {"_renpy", init_renpy},
        {"_renpybidi", init_renpybidi},
        {"renpy.audio.renpysound", initrenpy_audio_renpysound},
        {"renpy.display.accelerator", initrenpy_display_accelerator},
        {"renpy.display.matrix", initrenpy_display_matrix},
        {"renpy.display.render", initrenpy_display_render},
        {"renpy.gl.gldraw", initrenpy_gl_gldraw},
        {"renpy.gl.glenviron_shader", initrenpy_gl_glenviron_shader},
        {"renpy.gl.glrtt_copy", initrenpy_gl_glrtt_copy},
        {"renpy.gl.glrtt_fbo", initrenpy_gl_glrtt_fbo},
        {"renpy.gl.gltexture", initrenpy_gl_gltexture},
        {"renpy.gl2.gl2draw", initrenpy_gl2_gl2draw},
        {"renpy.gl2.gl2mesh", initrenpy_gl2_gl2mesh},
        {"renpy.gl2.gl2mesh2", initrenpy_gl2_gl2mesh2},
        {"renpy.gl2.gl2mesh3", initrenpy_gl2_gl2mesh3},
        {"renpy.gl2.gl2model", initrenpy_gl2_gl2model},
        {"renpy.gl2.gl2polygon", initrenpy_gl2_gl2polygon},
        {"renpy.gl2.gl2shader", initrenpy_gl2_gl2shader},
        {"renpy.gl2.gl2texture", initrenpy_gl2_gl2texture},
        {"renpy.parsersupport", initrenpy_parsersupport},
        {"renpy.pydict", initrenpy_pydict},
        {"renpy.style", initrenpy_style},
        {"renpy.styledata.style_activate_functions", initrenpy_styledata_style_activate_functions},
        {"renpy.styledata.style_functions", initrenpy_styledata_style_functions},
        {"renpy.styledata.style_hover_functions", initrenpy_styledata_style_hover_functions},
        {"renpy.styledata.style_idle_functions", initrenpy_styledata_style_idle_functions},
        {"renpy.styledata.style_insensitive_functions", initrenpy_styledata_style_insensitive_functions},
        {"renpy.styledata.style_selected_activate_functions", initrenpy_styledata_style_selected_activate_functions},
        {"renpy.styledata.style_selected_functions", initrenpy_styledata_style_selected_functions},
        {"renpy.styledata.style_selected_hover_functions", initrenpy_styledata_style_selected_hover_functions},
        {"renpy.styledata.style_selected_idle_functions", initrenpy_styledata_style_selected_idle_functions},
        {"renpy.styledata.style_selected_insensitive_functions", initrenpy_styledata_style_selected_insensitive_functions},
        {"renpy.styledata.styleclass", initrenpy_styledata_styleclass},
        {"renpy.styledata.stylesets", initrenpy_styledata_stylesets},
        {"renpy.text.ftfont", initrenpy_text_ftfont},
        {"renpy.text.textsupport", initrenpy_text_textsupport},
        {"renpy.text.texwrap", initrenpy_text_texwrap},
        {"renpy.uguu.gl", initrenpy_uguu_gl},
        {"renpy.uguu.uguu", initrenpy_uguu_uguu},
        {NULL, NULL}
    };

    char* dir_paths[] = {
        app_dir_path,
        "/app_home",
        NULL,
    };

    int found_sysconfigdata = 0;
    int found_renpy = 0;
    char python_zip_full_path[512];

    for (int i = 0; dir_paths[i] != NULL; i++)
    {
        _log("Ren'Py PS3: Checking path: %s\n", dir_paths[i]);
        snprintf(python_zip_full_path, sizeof(python_zip_full_path), "%s/lib/python27.zip", dir_paths[i]);
        struct stat st;
        if (stat(python_zip_full_path, &st) == 0) {
            _log("Ren'Py PS3: Found python27.zip at %s\n", python_zip_full_path);
            found_sysconfigdata = 1;
            strncpy(python_home_buffer, dir_paths[i], sizeof(python_home_buffer));
            python_home_buffer[sizeof(python_home_buffer)-1] = '\0';
            
            snprintf(python_script_buffer, sizeof(python_script_buffer), "%s/renpy.py", dir_paths[i]);
            if (stat(python_script_buffer, &st) == 0) {
                _log("Ren'Py PS3: Found renpy.py at %s\n", python_script_buffer);
                found_renpy = 1;
                break;
            }
        }
    }

    if (!found_sysconfigdata || !found_renpy) {
        show_error_and_exit("Could not find Ren'Py data files.");
    }

    _log("Ren'Py PS3: Setting Home to %s\n", python_home_buffer);
    Py_SetPythonHome(python_home_buffer);

    char path_env[1024];
    snprintf(path_env, sizeof(path_env), "%s/lib/python27.zip:%s", python_home_buffer, python_home_buffer);
    _log("Ren'Py PS3: Setting PYTHONPATH to %s\n", path_env);
    SDL_setenv("PYTHONPATH", path_env, 1);
    SDL_setenv("PYTHONHOME", python_home_buffer, 1);

    _log("Ren'Py PS3: Python Version: %s\n", Py_GetVersion());

    _log("Ren'Py PS3: Extending Inittab...\n");
    PyImport_ExtendInittab(builtins);

    _log("Ren'Py PS3: Initializing Python (Py_InitializeEx(0))...\n");
    Py_InitializeEx(0);
    _log("Ren'Py PS3: Python Initialized!\n");

    char* pyargs[] = { python_script_buffer, app_dir_path, NULL };
    PySys_SetArgvEx(2, pyargs, 1);

    snprintf(python_snprintf_buffer, sizeof(python_snprintf_buffer), "import sys\nsys.path = ['%s', '%s']", python_zip_full_path, python_home_buffer);
    _log("Ren'Py PS3: Running path setup script...\n");
    PyRun_SimpleString(python_snprintf_buffer);

    _log("Ren'Py PS3: Importing basic modules...\n");
    PyRun_SimpleString("import os, pygame_sdl2, encodings\nprint 'Basic modules imported'\n");

    _log("Ren'Py PS3: Opening renpy.py for execution...\n");
    FILE* fp = fopen(python_script_buffer, "rb");
    if (fp) {
        _log("Ren'Py PS3: Executing renpy.py...\n");
        int res = PyRun_SimpleFileEx(fp, python_script_buffer, 1);
        _log("Ren'Py PS3: Execution result: %d\n", res);
    } else {
        show_error_and_exit("Failed to open renpy.py");
    }

    Py_Exit(0);
    return 0;
}
