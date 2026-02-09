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
#include <locale.h>

/* Fallback for sigset_t if missing */
#ifndef _SIGSET_T_DECLARED
typedef uint64_t sigset_t;
#endif

#define USED __attribute__((used))
#define VISIBLE __attribute__((visibility("default")))

static s32 _log_fd = -1;
static int _in_stub = 0;
static u32 _log_count = 0;

void ps3_init_logger(s32 fd) {
    _log_fd = fd;
}

/* Internal Safe String Builders */
static void _log_putc(char c) {
    if (_log_fd < 0) return;
    u64 written = 0;
    sysLv2FsWrite(_log_fd, &c, 1, &written);
}

static void _log_puts(const char *s) {
    if (_log_fd < 0 || !s) return;
    u64 written = 0;
    sysLv2FsWrite(_log_fd, s, strlen(s), &written);
}

static void _log_puthex(u64 val) {
    const char *chars = "0123456789abcdef";
    _log_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        _log_putc(chars[(val >> i) & 0xF]);
    }
}

static void _log_putnum(u64 val) {
    if (val == 0) { _log_putc('0'); return; }
    char buf[24];
    int i = sizeof(buf) - 1;
    buf[i--] = '\0';
    while (val > 0 && i >= 0) {
        buf[i--] = (char)('0' + (val % 10));
        val /= 10;
    }
    _log_puts(&buf[i+1]);
}

void _log_safe(const char *msg) {
    if (_log_fd < 0) return;
    _log_puts(msg);
    _log_count++;
    if ((_log_count % 50) == 0) sysLv2FsFsync(_log_fd);
}

void _log(const char *fmt, ...) {
    if (_log_fd < 0 || _in_stub) return;
    _in_stub = 1;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        u64 written = 0;
        sysLv2FsWrite(_log_fd, buf, (u64)len, &written);
        _log_count++;
        if ((_log_count % 20) == 0) sysLv2FsFsync(_log_fd);
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

/* Real symbols from libc */
extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void* __real_sbrk(intptr_t increment);
extern void* __real_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int __real_gettimeofday(struct timeval *tv, void *tz);
typedef void (*sighandler_t)(int);
extern sighandler_t __real_signal(int signum, sighandler_t handler);
extern char* __real_getenv(const char* name);
extern int __real_isatty(int fd);
extern char* __real_setlocale(int category, const char* locale);

/* Wrapped Memory Management */
USED VISIBLE void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if (!_in_stub) {
        _log_puts("WRAP: malloc("); _log_putnum(size); _log_puts(") -> "); _log_puthex((u64)ptr); _log_putc('\n');
    }
    return ptr;
}
USED VISIBLE void __wrap_free(void* ptr) {
    if (!_in_stub && ptr) {
        _log_puts("WRAP: free("); _log_puthex((u64)ptr); _log_puts(")\n");
    }
    __real_free(ptr);
}
USED VISIBLE void* __wrap_realloc(void* ptr, size_t size) {
    void* nptr = __real_realloc(ptr, size);
    if (!_in_stub) {
        _log_puts("WRAP: realloc("); _log_puthex((u64)ptr); _log_puts(", "); _log_putnum(size); _log_puts(") -> "); _log_puthex((u64)nptr); _log_putc('\n');
    }
    return nptr;
}
USED VISIBLE void* __wrap_calloc(size_t nmemb, size_t size) {
    void* ptr = __real_calloc(nmemb, size);
    if (!_in_stub) {
        _log_puts("WRAP: calloc("); _log_putnum(nmemb); _log_puts(", "); _log_putnum(size); _log_puts(") -> "); _log_puthex((u64)ptr); _log_putc('\n');
    }
    return ptr;
}
USED VISIBLE void *__wrap_sbrk(intptr_t inc) {
    void *ret = __real_sbrk(inc);
    if (!_in_stub) { _log_puts("WRAP: sbrk("); _log_putnum(inc); _log_puts(") -> "); _log_puthex((u64)ret); _log_putc('\n'); }
    return ret;
}
USED VISIBLE void *__wrap_mmap(void *a, size_t l, int p, int f, int fd, off_t o) {
    void *ret = __real_mmap(a, l, p, f, fd, o);
    if (!_in_stub) { _log_puts("WRAP: mmap("); _log_putnum(l); _log_puts(") -> "); _log_puthex((u64)ret); _log_putc('\n'); }
    return ret;
}

/* Wrapped System Control */
USED VISIBLE sighandler_t __wrap_signal(int signum, sighandler_t handler) {
    _log("WRAP: signal(%d, %p)\n", signum, handler);
    return __real_signal(signum, handler);
}
USED VISIBLE void __wrap_exit(int status) {
    _log("WRAP: exit(%d)\n", status);
    sysLv2FsFsync(_log_fd);
    while(1);
}
USED VISIBLE void __wrap__exit(int status) {
    _log("WRAP: _exit(%d)\n", status);
    sysLv2FsFsync(_log_fd);
    while(1);
}
USED VISIBLE void __wrap__Exit(int status) {
    _log("WRAP: _Exit(%d)\n", status);
    sysLv2FsFsync(_log_fd);
    while(1);
}
USED VISIBLE void __wrap_abort(void) {
    _log("WRAP: abort()\n");
    sysLv2FsFsync(_log_fd);
    while(1);
}
USED VISIBLE void __wrap___assert_fail(const char *a, const char *f, unsigned int l, const char *fn) {
    _log("WRAP: ASSERT FAIL: %s at %s:%u in %s\n", a, f, l, fn);
    sysLv2FsFsync(_log_fd);
    while(1);
}

/* Time Stubs */
USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    int ret = __real_gettimeofday(tv, tz);
    if (ret != 0 && tv) { tv->tv_sec = 1600000000; tv->tv_usec = 0; ret = 0; }
    return ret;
}
USED VISIBLE time_t time(time_t *t) {
    time_t now = 1600000000;
    if (t) *t = now;
    return now;
}

/* Filesystem Wraps */
USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & (0x0200 | 0x0040)) {
        va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args);
    }
    s32 fd = -1;
    s32 n_flags = translate_open_flags(flags);
    s32 res = sysLv2FsOpen(path, n_flags, &fd, (u32)mode, NULL, 0);
    _log("WRAP: open(%s, 0x%x) -> fd %d (res %x)\n", path, flags, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF);
    return -1;
}

USED VISIBLE ssize_t __wrap_read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0;
    u64 rb = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &rb);
    if (res != 0) {
        _log("WRAP: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res);
        errno = (int)(res & 0xFF); return -1;
    }
    return (ssize_t)rb;
}

USED VISIBLE ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        u64 w = 0;
        sysLv2FsWrite(_log_fd, "PY_OUT: ", 8, &w);
        sysLv2FsWrite(_log_fd, buf, (u64)count, &w);
        return (ssize_t)count;
    }
    u64 written = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &written);
    if (res != 0) {
        errno = (int)(res & 0xFF); return -1;
    }
    return (ssize_t)written;
}

USED VISIBLE int __wrap_close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    s32 res = sysLv2FsClose((s32)fd);
    _log("WRAP: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)(res & 0xFF); return -1;
}

USED VISIBLE off_t __wrap_lseek(int fd, off_t offset, int whence) {
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);
    if (res == 0) return (off_t)pos;
    _log("WRAP: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)(res & 0xFF); return -1;
}

/* Stat Mappings */
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

USED VISIBLE int __wrap_fstat(int fd, struct stat *buf) {
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
    _log("WRAP: fstat(%d) -> res %d, size %lld\n", fd, ret, (long long)(buf?buf->st_size:0));
    return ret;
}

USED VISIBLE int __wrap_stat(const char *path, struct stat *buf) {
    sysFSStat lv2;
    s32 res = sysLv2FsStat(path, &lv2);
    if (res == 0) {
        map_stat(buf, &lv2);
        _log("WRAP: stat(%s) -> size %lld\n", path, (long long)buf->st_size);
        return 0;
    }
    _log("WRAP: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}

USED VISIBLE int __wrap_lstat(const char *path, struct stat *buf) {
    return __wrap_stat(path, buf);
}

/* Environment and Terminal Wraps */
USED VISIBLE char* __wrap_getenv(const char* name) {
    char* res = __real_getenv(name);
    if (!_in_stub) { _log("WRAP: getenv(%s) -> %s\n", name, res ? res : "NULL"); }
    return res;
}
USED VISIBLE int __wrap_isatty(int fd) {
    int res = __real_isatty(fd);
    if (!_in_stub) { _log("WRAP: isatty(%d) -> %d\n", fd, res); }
    return res;
}
USED VISIBLE char* __wrap_setlocale(int category, const char* locale) {
    char* res = __real_setlocale(category, locale);
    if (!_in_stub) { _log("WRAP: setlocale(%d, %s) -> %s\n", category, locale ? locale : "NULL", res ? res : "NULL"); }
    return res;
}

/* Other System Wraps */
USED VISIBLE int __wrap_fcntl(int fd, int cmd, ...) {
    _log("WRAP: fcntl(%d, %d)\n", fd, cmd);
    return 0;
}
USED VISIBLE int __wrap_ioctl(int fd, unsigned long request, ...) {
    _log("WRAP: ioctl(%d, %lu)\n", fd, request);
    return -1;
}

/* Identity and process basics */
USED VISIBLE uid_t getuid(void) { return 0; }
USED VISIBLE gid_t getgid(void) { return 0; }
USED VISIBLE uid_t geteuid(void) { return 0; }
USED VISIBLE gid_t getegid(void) { return 0; }
USED VISIBLE pid_t getppid(void) { return 1; }
USED VISIBLE pid_t getpid(void) { return 100; }

/* Missing Python symbols */
USED VISIBLE FILE *popen(const char *command, const char *type) { _log("STUB: popen %s\n", command); return NULL; }
USED VISIBLE int pclose(FILE *stream) { return -1; }
USED VISIBLE struct passwd *getpwuid(uid_t uid) { return NULL; }
USED VISIBLE struct passwd *getpwnam(const char *name) { return NULL; }
USED VISIBLE struct group *getgrgid(gid_t gid) { return NULL; }
USED VISIBLE struct group *getgrnam(const char *name) { return NULL; }
USED VISIBLE int pipe(int fildes[2]) { _log("STUB: pipe\n"); errno = ENOSYS; return -1; }
USED VISIBLE int symlink(const char *path1, const char *path2) { _log("STUB: symlink\n"); errno = EROFS; return -1; }
USED VISIBLE int fdatasync(int fildes) { return 0; }
USED VISIBLE char *ttyname(int fd) { return NULL; }
USED VISIBLE int execv(const char *path, char *const argv[]) { _log("STUB: execv %s\n", path); errno = ENOSYS; return -1; }
USED VISIBLE int readlink(const char *path, char *buf, size_t bufsiz) { return -1; }
USED VISIBLE int gethostname(char *name, size_t len) { snprintf(name, len, "ps3"); return 0; }
USED VISIBLE long sysconf(int name) { return -1; }

/* Python internal overrides */
void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads (potential GIL issues)\n"); }
void* IMG_LoadTexture_RW(void* r, void* s, int f) { return NULL; }
