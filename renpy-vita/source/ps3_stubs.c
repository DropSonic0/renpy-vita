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
#include <stdarg.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

static int _in_stub = 0;
static void _log(const char *fmt, ...) {
    if (_in_stub) return;
    _in_stub = 1;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    _in_stub = 0;
}

/* Undefine potential macros from signal.h that conflict with stubs */
#undef sigemptyset
#undef sigfillset
#undef sigaddset
#undef sigdelset
#undef sigismember
#undef sigaction
#undef kill
#undef getpid
#undef stat
#undef lstat

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

// Helper to map sysFSStat to struct stat
static void map_stat(struct stat *buf, sysFSStat *lv2_st) {
    if (!buf || !lv2_st) return;
    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = lv2_st->st_mode;
    buf->st_uid = lv2_st->st_uid;
    buf->st_gid = lv2_st->st_gid;
    buf->st_atime = lv2_st->st_atime;
    buf->st_mtime = lv2_st->st_mtime;
    buf->st_ctime = lv2_st->st_ctime;
    buf->st_size = lv2_st->st_size;
    buf->st_blksize = 4096;
    buf->st_nlink = 1;
}

int fstat(int fd, struct stat *buf) {
    sysFSStat lv2_st;
    memset(&lv2_st, 0, sizeof(sysFSStat));
    s32 res = sysLv2FsFStat(fd, &lv2_st);

    u64 cur=0, end=0;
    sysLv2FsLSeek64(fd, 0, 1, &cur);
    sysLv2FsLSeek64(fd, 0, 2, &end);
    sysLv2FsLSeek64(fd, cur, 0, &cur);

    if (res == 0) {
        map_stat(buf, &lv2_st);
        if (buf->st_size == 0 && end > 0) buf->st_size = end;
        _log("STUB: fstat(%d) -> size %lld, mode %x\n", fd, (long long)buf->st_size, (unsigned int)buf->st_mode);
        return 0;
    }

    if (fd >= 0 && fd <= 2) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0020000 | 0666; // S_IFCHR
            buf->st_nlink = 1;
            buf->st_blksize = 4096;
        }
        _log("STUB: fstat(%d) -> SUCCESS (Std Stream Fake)\n", fd);
        return 0;
    }

    // Fallback
    if (buf) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = 0100000 | 0666;
        buf->st_size = end;
    }
    _log("STUB: fstat(%d) -> FALLBACK %x, size %lld\n", fd, (unsigned int)res, (long long)end);
    return 0;
}

int stat(const char *path, struct stat *buf) {
    sysFSStat lv2_st;
    s32 res = sysLv2FsStat(path, &lv2_st);
    if (res == 0) {
        map_stat(buf, &lv2_st);
        _log("STUB: stat(%s) -> size %lld, mode %x\n", path, (long long)buf->st_size, (unsigned int)buf->st_mode);
        return 0;
    }
    _log("STUB: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}

int lstat(const char *path, struct stat *buf) { return stat(path, buf); }

#undef open
int open(const char *path, int flags, ...) {
    unsigned int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, unsigned int);
        va_end(args);
    }
    s32 fd = -1;
    s32 res = sysLv2FsOpen(path, flags, &fd, mode, NULL, 0);
    _log("STUB: open(%s, %x) -> fd %d (res %x)\n", path, flags, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)res;
    return -1;
}

#undef read
ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) {
        // stdin: return EOF to avoid blocking
        return 0;
    }
    u64 read_bytes = 0;
    s32 res = sysLv2FsRead(fd, buf, count, &read_bytes);
    if (res != 0) {
        _log("STUB: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res);
        errno = (int)res;
        return -1;
    }
    // Only log small reads or zip reads to avoid log spam
    if (count < 1024) {
        // _log("STUB: read(%d, %zu) -> %llu\n", fd, count, read_bytes);
    }
    return (ssize_t)read_bytes;
}

#undef write
ssize_t write(int fd, const void *buf, size_t count) {
    u64 written = 0;
    s32 res = sysLv2FsWrite(fd, buf, count, &written);
    if (res != 0) {
        // Can't log here if it's the log file failing
        errno = (int)res;
        return -1;
    }
    return (ssize_t)written;
}

#undef lseek
off_t lseek(int fd, off_t offset, int whence) {
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64(fd, (u64)offset, whence, &pos);
    if (res == 0) return (off_t)pos;
    _log("STUB: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)res;
    return -1;
}

#undef select
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    _log("STUB: select(%d)\n", nfds);
    return 0;
}

#undef getenv
char *getenv(const char *name) {
    // SDL_getenv should work, but let's log it
    extern char *SDL_getenv(const char *);
    char *res = SDL_getenv(name);
    _log("STUB: getenv(%s) -> %s\n", name, res ? res : "NULL");
    return res;
}

#undef close
int close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    s32 res = sysLv2FsClose((s32)fd);
    _log("STUB: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)res;
    return -1;
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