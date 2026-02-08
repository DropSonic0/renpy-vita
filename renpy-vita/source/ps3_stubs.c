#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <lv2/sysfs.h>


/* getpid, kill, sigaction, sigemptyset, getuid, getgid, geteuid, getegid, getppid are provided by the toolchain */
int pipe(int fildes[2]) { printf("STUB: pipe\n"); errno = ENOSYS; return -1; }
int fork() { printf("STUB: fork\n"); errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { printf("STUB: execv %s\n", path); errno = ENOSYS; return -1; }
int symlink(const char *path1, const char *path2) { printf("STUB: symlink %s -> %s\n", path1, path2); errno = EROFS; return -1; }
int fdatasync(int fildes) { return 0; }
char *ttyname(int fd) { printf("STUB: ttyname %d\n", fd); return NULL; }
int readlink(const char *path, char *buf, size_t bufsiz) { printf("STUB: readlink %s\n", path); errno = ENOSYS; return -1; }
int gethostname(char *name, size_t len) { printf("STUB: gethostname\n"); snprintf(name, len, "ps3"); return 0; }
long sysconf(int name) { printf("STUB: sysconf %d\n", name); return -1; }
int isatty(int fd) { return 0; }

int fstat(int fildes, struct stat *buf) { 
    sysFSStat lv2_st;
    s32 res = sysFsFstat(fildes, &lv2_st);
    if (res == 0) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = lv2_st.st_mode;
            buf->st_size = lv2_st.st_size;
            buf->st_atime = lv2_st.st_atime;
            buf->st_mtime = lv2_st.st_mtime;
            buf->st_ctime = lv2_st.st_ctime;
            // printf("fstat %d: size %lld\n", fildes, (long long)buf->st_size);
        }
        return 0;
    }
    // printf("STUB: fstat %d failed: %08x\n", fildes, (unsigned int)res);
    return -1;
}

char *getcwd(char *buf, size_t size) {
    if (buf && size > 30) {
        strncpy(buf, "/dev_hdd0/game/RENPY0001/USRDIR", size);
        return buf;
    }
    return NULL;
}

/* Stubs for popen/pclose if not available in PSL1GHT */
FILE *popen(const char *command, const char *type) { printf("STUB: popen %s\n", command); return NULL; }
int pclose(FILE *stream) { printf("STUB: pclose\n"); return -1; }

/* Stubs for pwd/grp functions if missing */
void *getpwuid(unsigned int uid) { printf("STUB: getpwuid %u\n", uid); return NULL; }
void *getpwnam(const char *name) { printf("STUB: getpwnam %s\n", name); return NULL; }
void *getgrgid(unsigned int gid) { printf("STUB: getgrgid %u\n", gid); return NULL; }
void *getgrnam(const char *name) { printf("STUB: getgrnam %s\n", name); return NULL; }

/* Python stubs */
void PyEval_InitThreads() { printf("STUB: PyEval_InitThreads\n"); }

/* SDL_image stubs for SDL2 compatibility if only SDL1 version is present. */
void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) {
    fprintf(stderr, "ERROR: IMG_LoadTexture_RW called but not implemented (SDL_image 1.2 incompatibility)\n");
    return NULL;
}