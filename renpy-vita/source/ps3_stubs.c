#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int getuid() { return 0; }
int getgid() { return 0; }
int geteuid() { return 0; }
int getegid() { return 0; }
int getppid() { return 1; }
int pipe(int fildes[2]) { errno = ENOSYS; return -1; }
int fork() { errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { errno = ENOSYS; return -1; }
int symlink(const char *path1, const char *path2) { errno = EROFS; return -1; }
int fdatasync(int fildes) { return 0; }
char *ttyname(int fd) { return NULL; }

char *getcwd(char *buf, size_t size) {
    if (buf && size > 1) {
        buf[0] = '/';
        buf[1] = '\0';
        return buf;
    }
    return NULL;
}

/* Stubs for popen/pclose if not available in PSL1GHT */
FILE *popen(const char *command, const char *type) { return NULL; }
int pclose(FILE *stream) { return -1; }

/* Stubs for pwd/grp functions if missing */
void *getpwuid(unsigned int uid) { return NULL; }
void *getpwnam(const char *name) { return NULL; }
void *getgrgid(unsigned int gid) { return NULL; }
void *getgrnam(const char *name) { return NULL; }

/* Python stubs */
void PyEval_InitThreads() { }

/* SDL_image stubs for SDL2 compatibility if only SDL1 version is present.
   SDL1.2 SDL_image is binary-incompatible with SDL2 SDL_RWops. */
void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) {
    fprintf(stderr, "ERROR: IMG_LoadTexture_RW called but not implemented (SDL_image 1.2 incompatibility)\n");
    return NULL;
}

int IMG_Init(int flags) {
    return flags;
}

void IMG_Quit() { }
