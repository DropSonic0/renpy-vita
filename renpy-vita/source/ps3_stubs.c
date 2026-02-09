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
#include <sys/times.h>

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
    if (flags & 0x0200) out |= SYS_O_CREAT;
    if (flags & 0x0008) out |= SYS_O_APPEND;
    if (flags & 0x0400) out |= SYS_O_TRUNC;
    if (flags & 0x0800) out |= SYS_O_EXCL;
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
#undef exit
#undef abort
#undef gettimeofday
#undef time

/* Identity and Process Stubs */
uid_t getuid(void) { _log("STUB: getuid\n"); return 0; }
uid_t _getuid(void) { return getuid(); }
gid_t getgid(void) { _log("STUB: getgid\n"); return 0; }
gid_t _getgid(void) { return getgid(); }
uid_t geteuid(void) { _log("STUB: geteuid\n"); return 0; }
uid_t _geteuid(void) { return geteuid(); }
gid_t getegid(void) { _log("STUB: getegid\n"); return 0; }
gid_t _getegid(void) { return getegid(); }
pid_t getppid(void) { _log("STUB: getppid\n"); return 1; }
pid_t _getppid(void) { return getppid(); }
pid_t getpid(void) { _log("STUB: getpid\n"); return 100; }
pid_t _getpid(void) { return getpid(); }

int kill(pid_t pid, int sig) { _log("STUB: kill(%d, %d)\n", (int)pid, sig); return 0; }
int _kill(pid_t pid, int sig) { return kill(pid, sig); }
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) { _log("STUB: sigaction(%d)\n", sig); return 0; }
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
int gethostname(char *name, size_t len) { _log("STUB: gethostname\n"); snprintf(name, len, "ps3"); return 0; }
long sysconf(int name) { _log("STUB: sysconf(%d)\n", name); return -1; }
int isatty(int fd) {
    int res = (fd >= 0 && fd <= 2);
    _log("STUB: isatty(%d) -> %d\n", fd, res);
    return res;
}

/* Exit Stubs */
void exit(int status) { _log("FATAL: exit(%d) called\n", status); while(1); }
void _exit(int status) { _log("FATAL: _exit(%d) called\n", status); while(1); }
void _Exit(int status) { _log("FATAL: _Exit(%d) called\n", status); while(1); }
void abort(void) { _log("FATAL: abort() called\n"); while(1); }

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    _log("ASSERT FAIL: %s at %s:%u in %s\n", assertion, file, line, function);
    while(1);
}

/* Time Stubs */
int gettimeofday(struct timeval *tv, struct timezone *tz) {
    _log("STUB: gettimeofday\n");
    if (tv) { tv->tv_sec = 1600000000; tv->tv_usec = 0; }
    return 0;
}
time_t time(time_t *t) {
    _log("STUB: time\n");
    time_t now = 1600000000;
    if (t) *t = now;
    return now;
}

int clock_gettime(int clk_id, struct timespec *tp) {
    _log("STUB: clock_gettime(%d)\n", clk_id);
    if (tp) { tp->tv_sec = 1600000000; tp->tv_nsec = 0; }
    return 0;
}
clock_t times(struct tms *buf) {
    _log("STUB: times\n");
    if (buf) memset(buf, 0, sizeof(struct tms));
    return 0;
}

/* Memory Stubs */
void *sbrk(intptr_t increment) {
    _log("STUB: sbrk(%ld)\n", (long)increment);
    errno = ENOMEM;
    return (void *)-1;
}

int brk(void *addr) {
    _log("STUB: brk(%p)\n", addr);
    errno = ENOMEM;
    return -1;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    _log("STUB: mmap(%zu, fd=%d)\n", length, fd);
    errno = ENOMEM;
    return (void *)-1;
}

int munmap(void *addr, size_t length) {
    _log("STUB: munmap(%p, %zu)\n", addr, length);
    return 0;
}

int mprotect(void *addr, size_t len, int prot) {
    _log("STUB: mprotect(%p, %zu)\n", addr, len);
    return 0;
}

/* Filesystem Mappings and Stubs */
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

int fstat(int fd, struct stat *buf);
int fstat64(int fd, struct stat *buf) { return fstat(fd, buf); }
int _fstat(int fd, struct stat *buf) { return fstat(fd, buf); }
int _fstat64(int fd, struct stat *buf) { return fstat(fd, buf); }

int fstat(int fd, struct stat *buf) {
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    int res_ret = -1;
    if (fd >= 0 && fd <= 2) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0020000 | 0666;
            buf->st_blksize = 4096;
        }
        res_ret = 0;
    } else {
        sysFSStat lv2_st;
        s32 res = sysLv2FsFStat((s32)fd, &lv2_st);
        if (res == 0) {
            map_stat(buf, &lv2_st);
            res_ret = 0;
        } else {
            u64 orig = 0, end = 0;
            sysLv2FsLSeek64((s32)fd, 0, 1, &orig);
            sysLv2FsLSeek64((s32)fd, 0, 2, &end);
            sysLv2FsLSeek64((s32)fd, orig, 0, &orig);
            if (buf && end > 0) {
                memset(buf, 0, sizeof(struct stat));
                buf->st_mode = 0100000 | 0666;
                buf->st_size = (off_t)end;
                buf->st_blksize = 4096;
                res_ret = 0;
            } else {
                errno = (int)(res & 0xFF);
            }
        }
    }
    _in_stub = saved_in_stub;
    _log("STUB: fstat(%d) -> res %d, size %lld\n", fd, res_ret, (long long)(buf?buf->st_size:0));
    return res_ret;
}

int stat(const char *path, struct stat *buf);
int stat64(const char *path, struct stat *buf) { return stat(path, buf); }
int _stat(const char *path, struct stat *buf) { return stat(path, buf); }
int _stat64(const char *path, struct stat *buf) { return stat(path, buf); }

int stat(const char *path, struct stat *buf) {
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    sysFSStat lv2_st;
    s32 res = sysLv2FsStat(path, &lv2_st);
    _in_stub = saved_in_stub;
    if (res == 0) {
        map_stat(buf, &lv2_st);
        _log("STUB: stat(%s) -> size %lld\n", path, (long long)buf->st_size);
        return 0;
    }
    _log("STUB: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}

int lstat(const char *path, struct stat *buf) { return stat(path, buf); }
int lstat64(const char *path, struct stat *buf) { return stat(path, buf); }
int _lstat(const char *path, struct stat *buf) { return stat(path, buf); }
int _lstat64(const char *path, struct stat *buf) { return stat(path, buf); }

int open(const char *path, int flags, ...);
int open64(const char *path, int flags, ...) {
    va_list args; va_start(args, flags); int mode = va_arg(args, int); va_end(args);
    return open(path, flags, mode);
}
int _open(const char *path, int flags, ...) {
    va_list args; va_start(args, flags); int mode = va_arg(args, int); va_end(args);
    return open(path, flags, mode);
}

int open(const char *path, int flags, ...) {
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    s32 fd = -1;
    u32 mode = 0;
    if (flags & (0x0200 | 0x0040)) {
        va_list args; va_start(args, flags); mode = (u32)va_arg(args, int); va_end(args);
    }
    s32 native_flags = translate_open_flags(flags);
    s32 res = sysLv2FsOpen(path, native_flags, &fd, mode, NULL, 0);
    _in_stub = saved_in_stub;
    _log("STUB: open(%s, 0x%x) -> fd %d (res %x)\n", path, flags, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF);
    return -1;
}

ssize_t _read(int fd, void *buf, size_t count) { return read(fd, buf, count); }
ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0;
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    u64 read_bytes = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &read_bytes);
    _in_stub = saved_in_stub;
    if (res != 0) {
        _log("STUB: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res);
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)read_bytes;
}

ssize_t _write(int fd, const void *buf, size_t count) { return write(fd, buf, count); }
ssize_t write(int fd, const void *buf, size_t count) {
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        /* Capture Python's own output to our log */
        u64 written = 0;
        sysLv2FsWrite(_log_fd, "PY_OUT: ", 8, &written);
        sysLv2FsWrite(_log_fd, buf, (u64)count, &written);
        return (ssize_t)count;
    }
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    u64 written = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &written);
    _in_stub = saved_in_stub;
    if (res != 0) {
        _log("STUB: write(%d) -> FAIL %x\n", fd, (unsigned int)res);
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)written;
}

off_t lseek(int fd, off_t offset, int whence);
off_t lseek64(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }
off_t _lseek(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }
off_t _lseek64(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }

off_t lseek(int fd, off_t offset, int whence) {
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);
    _in_stub = saved_in_stub;
    if (res == 0) return (off_t)pos;
    _log("STUB: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)(res & 0xFF);
    return -1;
}

int _close(int fd) { return close(fd); }
int close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    int saved_in_stub = _in_stub;
    _in_stub = 1;
    s32 res = sysLv2FsClose((s32)fd);
    _in_stub = saved_in_stub;
    _log("STUB: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)(res & 0xFF);
    return -1;
}

int fcntl(int fd, int cmd, ...) { _log("STUB: fcntl(%d, %d)\n", fd, cmd); return 0; }
int ioctl(int fd, unsigned long request, ...) { _log("STUB: ioctl(%d, %lu)\n", fd, request); return -1; }
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    _log("STUB: select(%d)\n", nfds);
    return 0;
}

char *getenv(const char *name) {
    extern char *SDL_getenv(const char *);
    char *res = SDL_getenv(name);
    if (!_in_stub) {
        int saved = _in_stub; _in_stub = 1;
        _log("STUB: getenv(%s) -> %s\n", name, res ? res : "NULL");
        _in_stub = saved;
    }
    return res;
}

FILE *popen(const char *command, const char *type) { _log("STUB: popen %s\n", command); return NULL; }
int pclose(FILE *stream) { return -1; }
void *getpwuid(unsigned int uid) { return NULL; }
void *getpwnam(const char *name) { return NULL; }
void *getgrgid(unsigned int gid) { return NULL; }
void *getgrnam(const char *name) { return NULL; }

void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads\n"); }
void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) { return NULL; }
