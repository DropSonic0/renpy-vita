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
#include <pwd.h>
#include <grp.h>

/* Fallback for sigset_t if missing */
#ifndef _SIGSET_T_DECLARED
typedef uint64_t sigset_t;
#endif

#define USED __attribute__((used))
#define VISIBLE __attribute__((visibility("default")))

/* Logger using direct PS3 syscalls with FSync */
static s32 _log_fd = -1;
static int _in_stub = 0;

void ps3_init_logger(s32 fd) {
    _log_fd = fd;
}

void _log(const char *fmt, ...) {
    if (_log_fd < 0) return;
    if (_in_stub) {
        u64 w = 0;
        sysLv2FsWrite(_log_fd, "[RECURSIVE_LOG]\n", 16, &w);
        return;
    }
    _in_stub = 1;
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        u64 written = 0;
        s32 res = sysLv2FsWrite(_log_fd, buf, (u64)len, &written);
        if (res == 0) sysLv2FsFsync(_log_fd);
    }
    _in_stub = 0;
}

/* Map POSIX open flags to PS3 native flags. */
static s32 translate_open_flags(int flags) {
    s32 out = 0;
    int acc = flags & 3;
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
#undef popen
#undef pclose
#undef getpwuid
#undef getpwnam
#undef getgrgid
#undef getgrnam

/* Identity Stubs */
USED VISIBLE uid_t getuid(void) { return 0; }
USED VISIBLE gid_t getgid(void) { return 0; }
USED VISIBLE uid_t geteuid(void) { return 0; }
USED VISIBLE gid_t getegid(void) { return 0; }
USED VISIBLE pid_t getppid(void) { return 1; }
USED VISIBLE pid_t getpid(void) { return 100; }

/* Wrapped Identity */
USED VISIBLE uid_t __wrap_getuid(void) { return getuid(); }
USED VISIBLE pid_t __wrap_getpid(void) { return getpid(); }

/* Signals and Process Stubs */
USED VISIBLE int kill(pid_t pid, int sig) { _log("STUB: kill(%d, %d)\n", (int)pid, sig); return 0; }
USED VISIBLE int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) { _log("STUB: sigaction(%d)\n", sig); return 0; }
USED VISIBLE int sigemptyset(sigset_t *set) { if (set) memset(set, 0, sizeof(sigset_t)); return 0; }
USED VISIBLE int sigfillset(sigset_t *set) { if (set) memset(set, 0xFF, sizeof(sigset_t)); return 0; }
USED VISIBLE int sigaddset(sigset_t *set, int signum) { return 0; }
USED VISIBLE int sigdelset(sigset_t *set, int signum) { return 0; }
USED VISIBLE int sigismember(const sigset_t *set, int signum) { return 0; }

USED VISIBLE int pipe(int fildes[2]) { _log("STUB: pipe\n"); errno = ENOSYS; return -1; }
USED VISIBLE int fork() { _log("STUB: fork\n"); errno = ENOSYS; return -1; }
USED VISIBLE int execv(const char *path, char *const argv[]) { _log("STUB: execv %s\n", path); errno = ENOSYS; return -1; }
USED VISIBLE int symlink(const char *path1, const char *path2) { _log("STUB: symlink\n"); errno = EROFS; return -1; }
USED VISIBLE int fdatasync(int fildes) { _log("STUB: fdatasync(%d)\n", fildes); return 0; }
USED VISIBLE char *ttyname(int fd) { return NULL; }
USED VISIBLE int readlink(const char *path, char *buf, size_t bufsiz) { return -1; }
USED VISIBLE int gethostname(char *name, size_t len) { snprintf(name, len, "ps3"); return 0; }
USED VISIBLE long sysconf(int name) { _log("STUB: sysconf(%d)\n", name); return -1; }
USED VISIBLE int isatty(int fd) {
    int res = (fd >= 0 && fd <= 2);
    _log("STUB: isatty(%d) -> %d\n", fd, res);
    return res;
}

/* Fatal Stubs */
USED VISIBLE void exit(int status) { _log("FATAL: exit(%d) called\n", status); while(1); }
USED VISIBLE void _exit(int status) { _log("FATAL: _exit(%d) called\n", status); while(1); }
USED VISIBLE void _Exit(int status) { _log("FATAL: _Exit(%d) called\n", status); while(1); }
USED VISIBLE void abort(void) { _log("FATAL: abort() called\n"); while(1); }
USED VISIBLE void __assert_fail(const char *a, const char *f, unsigned int l, const char *fn) { _log("ASSERT FAIL: %s at %s:%u in %s\n", a, f, l, fn); while(1); }

/* Wrapped Fatal */
USED VISIBLE void __wrap_exit(int s) { exit(s); }
USED VISIBLE void __wrap_abort(void) { abort(); }

/* Time Stubs */
USED VISIBLE int gettimeofday(struct timeval *tv, void *tz) {
    if (tv) { tv->tv_sec = 1600000000; tv->tv_usec = 0; }
    return 0;
}
USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) { return gettimeofday(tv, tz); }

USED VISIBLE time_t time(time_t *t) {
    time_t now = 1600000000;
    if (t) *t = now;
    return now;
}
USED VISIBLE int clock_gettime(int id, struct timespec *tp) {
    if (tp) { tp->tv_sec = 1600000000; tp->tv_nsec = 0; }
    return 0;
}
USED VISIBLE clock_t times(struct tms *buf) {
    if (buf) memset(buf, 0, sizeof(struct tms));
    return 0;
}

/* Memory Stubs - Provide a small static heap */
static char sbrk_heap[512 * 1024];
static size_t sbrk_ptr = 0;
USED VISIBLE void *sbrk(intptr_t increment) {
    void *prev = (void *)-1;
    if (sbrk_ptr + (size_t)increment <= sizeof(sbrk_heap)) {
        prev = &sbrk_heap[sbrk_ptr];
        sbrk_ptr += (size_t)increment;
    }
    _log("STUB: sbrk(%ld) -> %p (used %zu/%zu)\n", (long)increment, prev, sbrk_ptr, sizeof(sbrk_heap));
    if (prev == (void*)-1) errno = ENOMEM;
    return prev;
}
USED VISIBLE void *__wrap_sbrk(intptr_t inc) { return sbrk(inc); }

USED VISIBLE int brk(void *addr) { return -1; }
USED VISIBLE void *mmap(void *a, size_t l, int p, int f, int fd, off_t o) { _log("STUB: mmap(%zu, fd=%d)\n", l, fd); errno = ENOMEM; return (void *)-1; }
USED VISIBLE void *__wrap_mmap(void *a, size_t l, int p, int f, int fd, off_t o) { return mmap(a,l,p,f,fd,o); }
USED VISIBLE int munmap(void *a, size_t l) { return 0; }
USED VISIBLE int mprotect(void *a, size_t l, int p) { return 0; }

/* Filesystem Stubs */
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
}

USED VISIBLE int fstat(int fd, struct stat *buf) {
    int saved = _in_stub; _in_stub = 1;
    int ret = -1;
    if (fd >= 0 && fd <= 2) {
        if (buf) { memset(buf, 0, sizeof(struct stat)); buf->st_mode = 0020000 | 0666; buf->st_blksize = 4096; }
        ret = 0;
    } else {
        sysFSStat lv2;
        s32 res = sysLv2FsFStat((s32)fd, &lv2);
        if (res == 0) { map_stat(buf, &lv2); ret = 0; }
        else { ret = -1; errno = (int)(res & 0xFF); }
    }
    _in_stub = saved;
    _log("STUB: fstat(%d) -> ret %d, size %lld\n", fd, ret, (long long)(buf?buf->st_size:0));
    return ret;
}
USED VISIBLE int __wrap_fstat(int fd, struct stat *buf) { return fstat(fd, buf); }

USED VISIBLE int stat(const char *path, struct stat *buf) {
    int saved = _in_stub; _in_stub = 1;
    sysFSStat lv2;
    s32 res = sysLv2FsStat(path, &lv2);
    _in_stub = saved;
    if (res == 0) { map_stat(buf, &lv2); _log("STUB: stat(%s) -> size %lld\n", path, (long long)buf->st_size); return 0; }
    _log("STUB: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}
USED VISIBLE int __wrap_stat(const char *path, struct stat *buf) { return stat(path, buf); }

USED VISIBLE int lstat(const char *path, struct stat *buf) { return stat(path, buf); }
USED VISIBLE int __wrap_lstat(const char *path, struct stat *buf) { return stat(path, buf); }

USED VISIBLE int open(const char *path, int flags, ...) {
    int saved = _in_stub; _in_stub = 1;
    s32 fd = -1; u32 mode = 0;
    if (flags & (0x0200 | 0x0040)) { va_list a; va_start(a, flags); mode = (u32)va_arg(a, int); va_end(a); }
    s32 n_flags = translate_open_flags(flags);
    s32 res = sysLv2FsOpen(path, n_flags, &fd, mode, NULL, 0);
    _in_stub = saved;
    _log("STUB: open(%s, 0x%x) -> fd %d (res %x)\n", path, flags, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF); return -1;
}
USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    va_list a; va_start(a, flags); int m = va_arg(a, int); va_end(a);
    return open(path, flags, m);
}

USED VISIBLE ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0;
    int saved = _in_stub; _in_stub = 1;
    u64 rb = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &rb);
    _in_stub = saved;
    if (res != 0) { _log("STUB: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res); errno = (int)(res & 0xFF); return -1; }
    _log("STUB: read(%d, %zu) -> %llu bytes\n", fd, count, (unsigned long long)rb);
    return (ssize_t)rb;
}
USED VISIBLE ssize_t __wrap_read(int fd, void *buf, size_t count) { return read(fd, buf, count); }

USED VISIBLE ssize_t write(int fd, const void *buf, size_t count) {
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        u64 w = 0;
        sysLv2FsWrite(_log_fd, "PY_OUT: ", 8, &w);
        sysLv2FsWrite(_log_fd, buf, (u64)count, &w);
        sysLv2FsFsync(_log_fd);
        return (ssize_t)count;
    }
    int saved = _in_stub; _in_stub = 1;
    u64 w = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &w);
    _in_stub = saved;
    if (res != 0) { _log("STUB: write(%d) -> FAIL %x\n", fd, (unsigned int)res); errno = (int)(res & 0xFF); return -1; }
    return (ssize_t)w;
}
USED VISIBLE ssize_t __wrap_write(int fd, const void *buf, size_t count) { return write(fd, buf, count); }

USED VISIBLE off_t lseek(int fd, off_t offset, int whence) {
    int saved = _in_stub; _in_stub = 1;
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);
    _in_stub = saved;
    if (res == 0) { _log("STUB: lseek(%d, %lld, %d) -> %llu\n", fd, (long long)offset, whence, (unsigned long long)pos); return (off_t)pos; }
    _log("STUB: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)(res & 0xFF); return -1;
}
USED VISIBLE off_t __wrap_lseek(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }

USED VISIBLE int close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    int saved = _in_stub; _in_stub = 1;
    s32 res = sysLv2FsClose((s32)fd);
    _in_stub = saved;
    _log("STUB: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)(res & 0xFF); return -1;
}
USED VISIBLE int __wrap_close(int fd) { return close(fd); }

USED VISIBLE int fcntl(int fd, int cmd, ...) { _log("STUB: fcntl(%d, %d)\n", fd, cmd); return 0; }
USED VISIBLE int ioctl(int fd, unsigned long request, ...) { _log("STUB: ioctl(%d, %lu)\n", fd, request); return -1; }
USED VISIBLE int select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) { _log("STUB: select(%d)\n", n); return 0; }

USED VISIBLE char *getenv(const char *name) {
    extern char *SDL_getenv(const char *);
    char *res = SDL_getenv(name);
    if (!_in_stub) { int s = _in_stub; _in_stub = 1; _log("STUB: getenv(%s) -> %s\n", name, res?res:"NULL"); _in_stub = s; }
    return res;
}

/* NEWLIB REENTRANT VARIANTS */
struct _reent;
USED VISIBLE int _open_r(struct _reent *r, const char *p, int f, int m) { return open(p, f, m); }
USED VISIBLE _ssize_t _read_r(struct _reent *r, int fd, void *b, size_t c) { return (_ssize_t)read(fd, b, c); }
USED VISIBLE _ssize_t _write_r(struct _reent *r, int fd, const void *b, size_t c) { return (_ssize_t)write(fd, b, c); }
USED VISIBLE _off_t _lseek_r(struct _reent *r, int fd, _off_t o, int w) { return (_off_t)lseek(fd, (off_t)o, w); }
USED VISIBLE int _close_r(struct _reent *r, int fd) { return close(fd); }
USED VISIBLE int _fstat_r(struct _reent *r, int fd, struct stat *s) { return fstat(fd, s); }
USED VISIBLE int _stat_r(struct _reent *r, const char *p, struct stat *s) { return stat(p, s); }

/* Underscored aliases */
USED VISIBLE int _stat(const char *p, struct stat *s) { return stat(p, s); }
USED VISIBLE int _fstat(int fd, struct stat *s) { return fstat(fd, s); }
USED VISIBLE int _open(const char *p, int f, ...) { va_list a; va_start(a, f); int m = va_arg(a, int); va_end(a); return open(p, f, m); }
USED VISIBLE ssize_t _read(int fd, void *b, size_t c) { return read(fd, b, c); }
USED VISIBLE ssize_t _write(int fd, const void *b, size_t c) { return write(fd, b, c); }
USED VISIBLE off_t _lseek(int fd, off_t o, int w) { return lseek(fd, o, w); }
USED VISIBLE int _close(int fd) { return close(fd); }

/* Missing standard functions for Python */
USED VISIBLE FILE *popen(const char *command, const char *type) { _log("STUB: popen %s\n", command); return NULL; }
USED VISIBLE int pclose(FILE *stream) { _log("STUB: pclose\n"); return -1; }
USED VISIBLE struct passwd *getpwuid(uid_t uid) { _log("STUB: getpwuid %u\n", (unsigned int)uid); return NULL; }
USED VISIBLE struct passwd *getpwnam(const char *name) { _log("STUB: getpwnam %s\n", name); return NULL; }
USED VISIBLE struct group *getgrgid(gid_t gid) { _log("STUB: getgrgid %u\n", (unsigned int)gid); return NULL; }
USED VISIBLE struct group *getgrnam(const char *name) { _log("STUB: getgrnam %s\n", name); return NULL; }

/* Python/SDL_image stubs */
USED VISIBLE void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads\n"); }
USED VISIBLE void* IMG_LoadTexture_RW(void* r, void* s, int f) { return NULL; }
