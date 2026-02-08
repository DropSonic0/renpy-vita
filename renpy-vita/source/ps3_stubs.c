#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <lv2/sysfs.h>
#include <sys/file.h>

/* Undefine potential macros from signal.h that conflict with stubs */
#undef sigemptyset
#undef sigfillset
#undef sigaddset
#undef sigdelset
#undef sigismember
#undef sigaction
#undef kill
#undef getpid

/* get* functions are declared in unistd.h but missing from libraries */
uid_t getuid(void) { return 0; }
gid_t getgid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getegid(void) { return 0; }
pid_t getppid(void) { return 1; }
pid_t getpid(void) { return 100; }

int kill(pid_t pid, int sig) { return 0; }
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) { return 0; }
int sigemptyset(sigset_t *set) { if (set) memset(set, 0, sizeof(sigset_t)); return 0; }
int sigfillset(sigset_t *set) { if (set) memset(set, 0xFF, sizeof(sigset_t)); return 0; }
int sigaddset(sigset_t *set, int signum) { return 0; }
int sigdelset(sigset_t *set, int signum) { return 0; }
int sigismember(const sigset_t *set, int signum) { return 0; }

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
    if (fildes >= 0 && fildes <= 2) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0020000 | 0666; // S_IFCHR
            buf->st_nlink = 1;
            buf->st_blksize = 4096;
        }
        return 0;
    }

    sysFSStat lv2_st;
    memset(&lv2_st, 0, sizeof(sysFSStat));
    s32 res = sysLv2FsFStat(fildes, &lv2_st);

    // Also try to get size via Lseek as a backup/validation
    u64 cur_pos = 0, end_pos = 0;
    sysLv2FsLSeek64(fildes, 0, 1, &cur_pos); // SEEK_CUR
    sysLv2FsLSeek64(fildes, 0, 2, &end_pos); // SEEK_END
    sysLv2FsLSeek64(fildes, cur_pos, 0, &cur_pos); // SEEK_SET

    if (res == 0) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = lv2_st.st_mode;
            // Use lseek size if it seems more plausible than the stat result
            buf->st_size = (end_pos > 0) ? end_pos : lv2_st.st_size;
            buf->st_atime = lv2_st.st_atime;
            buf->st_mtime = lv2_st.st_mtime;
            buf->st_ctime = lv2_st.st_ctime;
            buf->st_blksize = 4096;
            buf->st_nlink = 1;

            printf("fstat(%d) -> size %lld (lseek %lld), mode %08x\n",
                fildes, (long long)lv2_st.st_size, (long long)end_pos, (unsigned int)buf->st_mode);

            // Hex dump for alignment debugging
            unsigned char *p = (unsigned char *)&lv2_st;
            printf("fstat(%d) struct: ", fildes);
            for(int i=0; i<sizeof(sysFSStat); i++) printf("%02x ", p[i]);
            printf("\n");
        }
        return 0;
    }

    // If it fails, return a dummy success to avoid crashing Python
    if (buf) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = 0100000 | 0666; // S_IFREG
        buf->st_size = (off_t)end_pos;
    }
    printf("fstat(%d) -> FALLBACK (actual error %08x, lseek size %lld)\n", fildes, (unsigned int)res, (long long)end_pos);
    return 0;
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