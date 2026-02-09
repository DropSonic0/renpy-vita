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

/* Fallback for sigset_t if missing */
#ifndef _SIGSET_T_DECLARED
typedef uint64_t sigset_t;
#endif

/* Logger using direct PS3 syscalls to avoid recursion */
static s32 _log_fd = -1;
static int _in_stub = 0;

void ps3_init_logger(s32 fd) {
    _log_fd = fd;
}

void _log(const char *fmt, ...) {
    if (_log_fd < 0 || _in_stub) return;
    _in_stub = 1;
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        u64 written = 0;
        sysLv2FsWrite(_log_fd, buf, (u64)len, &written);
    }
    _in_stub = 0;
}

/* Map POSIX open flags to PS3 native flags. */
static s32 translate_open_flags(int flags) {
    s32 out = 0;
    int acc = flags & 3; // O_ACCMODE
    if (acc == 0) out |= SYS_O_RDONLY;
    else if (acc == 1) out |= SYS_O_WRONLY;
    else if (acc == 2) out |= SYS_O_RDWR;

    /* Newlib flags vs PS3 native flags */
    if (flags & 0x0200) out |= SYS_O_CREAT;  // Newlib O_CREAT
    if (flags & 0x0008) out |= SYS_O_APPEND; // Newlib O_APPEND
    if (flags & 0x0400) out |= SYS_O_TRUNC;  // Newlib O_TRUNC
    if (flags & 0x0800) out |= SYS_O_EXCL;   // Newlib O_EXCL

    /* Some Newlibs use 0x0040 for O_CREAT */
    if (flags & 0x0040) out |= SYS_O_CREAT;

    return out;
}

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
#undef open
#undef close
#undef read
#undef write
#undef lseek
#undef fcntl
#undef ioctl

/* get* functions */
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

int pipe(int fildes[2]) { _log("STUB: pipe\n"); errno = ENOSYS; return -1; }
int fork() { _log("STUB: fork\n"); errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { _log("STUB: execv %s\n", path); errno = ENOSYS; return -1; }
int symlink(const char *path1, const char *path2) { _log("STUB: symlink %s -> %s\n", path1, path2); errno = EROFS; return -1; }
int fdatasync(int fildes) { return 0; }
char *ttyname(int fd) { _log("STUB: ttyname %d\n", fd); return NULL; }
int readlink(const char *path, char *buf, size_t bufsiz) { _log("STUB: readlink %s\n", path); errno = ENOSYS; return -1; }
int gethostname(char *name, size_t len) { snprintf(name, len, "ps3"); return 0; }
long sysconf(int name) { return -1; }
int isatty(int fd) { return 0; }

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    _log("ASSERT FAIL: %s at %s:%u in %s\n", assertion, file, line, function);
    while(1);
}

static void map_stat(struct stat *buf, sysFSStat *lv2_st) {
    if (!buf || !lv2_st) return;
    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = (mode_t)lv2_st->st_mode;
    buf->st_uid = (uid_t)lv2_st->st_uid;
    buf->st_gid = (gid_t)lv2_st->st_gid;
    buf->st_atime = (time_t)lv2_st->st_atime;
    buf->st_mtime = (time_t)lv2_st->st_mtime;
    buf->st_ctime = (time_t)lv2_st->st_ctime;
    buf->st_size = (off_t)lv2_st->st_size;
    buf->st_blksize = 4096;
    buf->st_nlink = 1;
    buf->st_blocks = (buf->st_size + 511) / 512;
}

int fstat(int fd, struct stat *buf) {
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    int res_ret = -1;

    if (fd >= 0 && fd <= 2) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0020000 | 0666; // S_IFCHR
            buf->st_nlink = 1;
            buf->st_blksize = 4096;
        }
        res_ret = 0;
        goto end;
    }

    sysFSStat lv2_st;
    memset(&lv2_st, 0, sizeof(sysFSStat));
    s32 res = sysLv2FsFStat((s32)fd, &lv2_st);

    u64 orig_pos = 0, end_pos = 0, dummy = 0;
    sysLv2FsLSeek64((s32)fd, 0, 1, &orig_pos);
    sysLv2FsLSeek64((s32)fd, 0, 2, &end_pos);
    sysLv2FsLSeek64((s32)fd, orig_pos, 0, &dummy);

    if (res == 0) {
        map_stat(buf, &lv2_st);
        if (buf->st_size == 0 && end_pos > 0) buf->st_size = (off_t)end_pos;
        res_ret = 0;
    } else {
        // Fallback for redirected streams or files that fail fstat but are valid
        if (buf && end_pos > 0) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0100000 | 0666; // S_IFREG
            buf->st_size = (off_t)end_pos;
            buf->st_blksize = 4096;
            res_ret = 0;
        } else {
            res_ret = -1;
            errno = (int)(res & 0xFF);
        }
    }

end:
    _in_stub = saved_in_stub;
    if (log_it) {
        if (res_ret == 0)
            _log("STUB: fstat(%d) -> size %lld, mode %x\n", fd, (long long)(buf?buf->st_size:0), (unsigned int)(buf?buf->st_mode:0));
        else
            _log("STUB: fstat(%d) -> FAIL\n", fd);
    }
    return res_ret;
}

int stat(const char *path, struct stat *buf) {
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    sysFSStat lv2_st;
    s32 res = sysLv2FsStat(path, &lv2_st);

    _in_stub = saved_in_stub;
    if (res == 0) {
        map_stat(buf, &lv2_st);
        if (log_it) _log("STUB: stat(%s) -> size %lld, mode %x\n", path, (long long)buf->st_size, (unsigned int)buf->st_mode);
        return 0;
    }
    if (log_it) _log("STUB: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}

int lstat(const char *path, struct stat *buf) { return stat(path, buf); }

int open(const char *path, int flags, ...) {
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    s32 fd = -1;
    u32 mode = 0;
    if (flags & (0x0200 | 0x0040)) { // O_CREAT
        va_list args;
        va_start(args, flags);
        mode = (u32)va_arg(args, int);
        va_end(args);
    }
    s32 native_flags = translate_open_flags(flags);
    s32 res = sysLv2FsOpen(path, native_flags, &fd, mode, NULL, 0);

    _in_stub = saved_in_stub;
    if (log_it) _log("STUB: open(%s, 0x%x [native 0x%x]) -> fd %d (res %x)\n", path, flags, (unsigned int)native_flags, (int)fd, (unsigned int)res);

    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF);
    return -1;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0;
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    u64 read_bytes = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &read_bytes);

    _in_stub = saved_in_stub;
    if (res != 0) {
        if (log_it) _log("STUB: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res);
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)read_bytes;
}

ssize_t write(int fd, const void *buf, size_t count) {
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        u64 written = 0;
        sysLv2FsWrite(_log_fd, buf, (u64)count, &written);
        return (ssize_t)written;
    }

    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    u64 written = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &written);

    _in_stub = saved_in_stub;
    if (res != 0) {
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)written;
}

off_t lseek(int fd, off_t offset, int whence) {
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);

    _in_stub = saved_in_stub;
    if (res == 0) return (off_t)pos;
    if (log_it) _log("STUB: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)(res & 0xFF);
    return -1;
}

int close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    int log_it = !_in_stub;
    int saved_in_stub = _in_stub;
    _in_stub = 1;

    s32 res = sysLv2FsClose((s32)fd);

    _in_stub = saved_in_stub;
    if (log_it) _log("STUB: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)(res & 0xFF);
    return -1;
}

int fcntl(int fd, int cmd, ...) {
    return 0;
}

int ioctl(int fd, unsigned long request, ...) {
    return -1;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    return 0;
}

char *getenv(const char *name) {
    extern char *SDL_getenv(const char *);
    char *res = SDL_getenv(name);
    // Only log if not already in a stub (to avoid infinite loop if SDL_getenv calls getenv)
    if (!_in_stub) {
        int saved = _in_stub;
        _in_stub = 1;
        _log("STUB: getenv(%s) -> %s\n", name, res ? res : "NULL");
        _in_stub = saved;
    }
    return res;
}

FILE *popen(const char *command, const char *type) { return NULL; }
int pclose(FILE *stream) { return -1; }
void *getpwuid(unsigned int uid) { return NULL; }
void *getpwnam(const char *name) { return NULL; }
void *getgrgid(unsigned int gid) { return NULL; }
void *getgrnam(const char *name) { return NULL; }

void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads\n"); }

void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) {
    return NULL;
}
