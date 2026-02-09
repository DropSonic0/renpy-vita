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
#include <dirent.h>

#ifndef _SIGSET_T_DECLARED
typedef uint64_t sigset_t;
#endif

#define USED __attribute__((used))
#define VISIBLE __attribute__((visibility("default")))

static s32 _log_fd = -1;
static int _in_stub = 0;
static u32 _log_count = 0;
static u32 _malloc_count = 0;

void ps3_init_logger(s32 fd) {
    _log_fd = fd;
}

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
    if ((_log_count % 100) == 0) sysLv2FsFsync(_log_fd);
}

void _log(const char *fmt, ...) {
    if (_log_fd < 0 || _in_stub) return;
    int saved = _in_stub; _in_stub = 1;
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
    _in_stub = saved;
}

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

/* REAL LIBC SYMBOLS */
extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void* __real_sbrk(intptr_t inc);
extern void* __real_mmap(void *a, size_t l, int p, int f, int fd, off_t o);
extern int __real_gettimeofday(struct timeval *tv, void *tz);
typedef void (*sighandler_t)(int);
extern sighandler_t __real_signal(int signum, sighandler_t handler);
extern char* __real_getenv(const char* name);
extern char* __real_setlocale(int category, const char* locale);
extern int __real_isatty(int fd);

/* WRAPPED MEMORY */
USED VISIBLE void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    _malloc_count++;
    if (!_in_stub && ((_malloc_count % 200) == 0 || !ptr)) {
        int saved = _in_stub; _in_stub = 1;
        _log_puts("WRAP: malloc("); _log_putnum(size); _log_puts(") -> "); _log_puthex((u64)ptr);
        if (!ptr) _log_puts(" !!! FAILED !!!");
        _log_putc('\n');
        _in_stub = saved;
    }
    return ptr;
}
USED VISIBLE void __wrap_free(void* ptr) { __real_free(ptr); }
USED VISIBLE void* __wrap_realloc(void* ptr, size_t size) {
    void* nptr = __real_realloc(ptr, size);
    if (!_in_stub && !nptr && size > 0) { _log_safe("WRAP: realloc FAILED\n"); }
    return nptr;
}
USED VISIBLE void* __wrap_calloc(size_t n, size_t s) { return __real_calloc(n, s); }
USED VISIBLE void *__wrap_sbrk(intptr_t inc) {
    void *ret = __real_sbrk(inc);
    if (!_in_stub) { _log_puts("WRAP: sbrk("); _log_putnum(inc); _log_puts(") -> "); _log_puthex((u64)ret); _log_putc('\n'); }
    return ret;
}
USED VISIBLE void *__wrap_mmap(void *a, size_t l, int p, int f, int fd, off_t o) {
    if (!_in_stub) { _log_safe("WRAP: mmap called\n"); }
    return __real_mmap(a, l, p, f, fd, o);
}

/* WRAPPED SYSTEM CONTROL */
USED VISIBLE sighandler_t __wrap_signal(int s, sighandler_t h) { _log("WRAP: signal(%d)\n", s); return __real_signal(s, h); }
USED VISIBLE void __wrap_exit(int s) { _log("WRAP: exit(%d)\n", s); sysLv2FsFsync(_log_fd); while(1); }
USED VISIBLE void __wrap__exit(int s) { _log("WRAP: _exit(%d)\n", s); sysLv2FsFsync(_log_fd); while(1); }
USED VISIBLE void __wrap__Exit(int s) { _log("WRAP: _Exit(%d)\n", s); sysLv2FsFsync(_log_fd); while(1); }
USED VISIBLE void __wrap_abort(void) { _log("WRAP: abort()\n"); sysLv2FsFsync(_log_fd); while(1); }
USED VISIBLE void __wrap___assert_fail(const char *a, const char *f, unsigned int l, const char *fn) {
    _log("WRAP: ASSERT FAIL: %s at %s:%u\n", a, f, l); sysLv2FsFsync(_log_fd); while(1);
}
USED VISIBLE void __wrap___assert(const char *f, int l, const char *e) {
    _log("WRAP: ASSERT: %s at %s:%d\n", e, f, l); sysLv2FsFsync(_log_fd); while(1);
}

/* WRAPPED IDENTITY & PROCESS */
USED VISIBLE uid_t __wrap_getuid(void) { return 0; }
USED VISIBLE gid_t __wrap_getgid(void) { return 0; }
USED VISIBLE uid_t __wrap_geteuid(void) { return 0; }
USED VISIBLE gid_t __wrap_getegid(void) { return 0; }
USED VISIBLE pid_t __wrap_getpid(void) { return 100; }
USED VISIBLE pid_t __wrap_getppid(void) { return 1; }
USED VISIBLE int __wrap_pipe(int f[2]) { errno = ENOSYS; return -1; }
USED VISIBLE int __wrap_symlink(const char *p1, const char *p2) { errno = EROFS; return -1; }
USED VISIBLE int __wrap_fdatasync(int f) { return 0; }
USED VISIBLE char *__wrap_ttyname(int f) { return NULL; }
USED VISIBLE int __wrap_execv(const char *p, char *const a[]) { errno = ENOSYS; return -1; }
USED VISIBLE int __wrap_readlink(const char *p, char *b, size_t s) { return -1; }
USED VISIBLE int __wrap_gethostname(char *n, size_t l) { snprintf(n, l, "ps3"); return 0; }
USED VISIBLE long __wrap_sysconf(int n) { return -1; }

/* TIME */
USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    int ret = __real_gettimeofday(tv, tz);
    if (ret != 0 && tv) { tv->tv_sec = 1600000000; tv->tv_usec = 0; ret = 0; }
    return ret;
}

/* FILESYSTEM COMMON */
static void map_stat(struct stat *buf, sysFSStat *lv2) {
    if (!buf || !lv2) return;
    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = (mode_t)lv2->st_mode;
    buf->st_size = (off_t)lv2->st_size;
    buf->st_atime = (time_t)lv2->st_atime;
    buf->st_mtime = (time_t)lv2->st_mtime;
    buf->st_ctime = (time_t)lv2->st_ctime;
    buf->st_blksize = 4096; buf->st_nlink = 1;
}

/* WRAPPED OPEN */
int do_open(const char *path, int flags, int mode) {
    s32 fd = -1;
    s32 n_flags = translate_open_flags(flags);
    s32 res = sysLv2FsOpen(path, n_flags, &fd, (u32)mode, NULL, 0);
    _log("WRAP: open(%s, 0x%x) -> fd %d (res %x)\n", path, flags, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF); return -1;
}
USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & (0x0200 | 0x0040)) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return do_open(path, flags, mode);
}
USED VISIBLE int __wrap_open64(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & (0x0200 | 0x0040)) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return do_open(path, flags, mode);
}
USED VISIBLE int __wrap__open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & (0x0200 | 0x0040)) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return do_open(path, flags, mode);
}

/* WRAPPED READ */
ssize_t do_read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0;
    u64 rb = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &rb);
    if (res != 0) { _log("WRAP: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res); return -1; }
    return (ssize_t)rb;
}
USED VISIBLE ssize_t __wrap_read(int f, void *b, size_t c) { return do_read(f, b, c); }
USED VISIBLE ssize_t __wrap__read(int f, void *b, size_t c) { return do_read(f, b, c); }

/* WRAPPED WRITE */
ssize_t do_write(int fd, const void *buf, size_t count) {
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        u64 w = 0;
        sysLv2FsWrite(_log_fd, "PY_OUT: ", 8, &w);
        sysLv2FsWrite(_log_fd, buf, (u64)count, &w);
        sysLv2FsFsync(_log_fd);
        return (ssize_t)count;
    }
    u64 w = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &w);
    return (res == 0) ? (ssize_t)w : -1;
}
USED VISIBLE ssize_t __wrap_write(int f, const void *b, size_t c) { return do_write(f, b, c); }
USED VISIBLE ssize_t __wrap__write(int f, const void *b, size_t c) { return do_write(f, b, c); }

/* WRAPPED CLOSE */
int do_close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    s32 res = sysLv2FsClose((s32)fd);
    _log("WRAP: close(%d) -> %x\n", fd, (unsigned int)res);
    return (res == 0) ? 0 : -1;
}
USED VISIBLE int __wrap_close(int f) { return do_close(f); }
USED VISIBLE int __wrap__close(int f) { return do_close(f); }

/* WRAPPED LSEEK */
off_t do_lseek(int fd, off_t offset, int whence) {
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);
    if (res == 0) return (off_t)pos;
    return -1;
}
USED VISIBLE off_t __wrap_lseek(int f, off_t o, int w) { return do_lseek(f, o, w); }
USED VISIBLE off_t __wrap_lseek64(int f, off_t o, int w) { return do_lseek(f, o, w); }
USED VISIBLE off_t __wrap__lseek(int f, off_t o, int w) { return do_lseek(f, o, w); }

/* WRAPPED STAT */
int do_stat(const char *path, struct stat *buf) {
    sysFSStat lv2;
    s32 res = sysLv2FsStat(path, &lv2);
    if (res == 0) { map_stat(buf, &lv2); _log("WRAP: stat(%s) -> size %lld\n", path, (long long)buf->st_size); return 0; }
    _log("WRAP: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}
USED VISIBLE int __wrap_stat(const char *p, struct stat *b) { return do_stat(p, b); }
USED VISIBLE int __wrap_stat64(const char *p, struct stat *b) { return do_stat(p, b); }
USED VISIBLE int __wrap__stat(const char *p, struct stat *b) { return do_stat(p, b); }

/* WRAPPED FSTAT */
int do_fstat(int fd, struct stat *buf) {
    if (fd >= 0 && fd <= 2) {
        if (buf) { memset(buf, 0, sizeof(struct stat)); buf->st_mode = 0020000 | 0666; buf->st_blksize = 4096; }
        return 0;
    }
    sysFSStat lv2;
    s32 res = sysLv2FsFStat((s32)fd, &lv2);
    if (res == 0) { map_stat(buf, &lv2); _log("WRAP: fstat(%d) -> size %lld\n", fd, (long long)buf->st_size); return 0; }
    return -1;
}
USED VISIBLE int __wrap_fstat(int f, struct stat *b) { return do_fstat(f, b); }
USED VISIBLE int __wrap_fstat64(int f, struct stat *b) { return do_fstat(f, b); }
USED VISIBLE int __wrap__fstat(int f, struct stat *b) { return do_fstat(f, b); }

/* WRAPPED LSTAT */
USED VISIBLE int __wrap_lstat(const char *p, struct stat *b) { return do_stat(p, b); }
USED VISIBLE int __wrap_lstat64(const char *p, struct stat *b) { return do_stat(p, b); }
USED VISIBLE int __wrap__lstat(const char *p, struct stat *b) { return do_stat(p, b); }

/* DIRECTORY WRAPS */
USED VISIBLE DIR* __wrap_opendir(const char *name) {
    _log("WRAP: opendir(%s)\n", name);
    errno = ENOTDIR;
    return NULL;
}
USED VISIBLE struct dirent* __wrap_readdir(DIR *dirp) { return NULL; }
USED VISIBLE int __wrap_closedir(DIR *dirp) { return -1; }

/* MISC WRAPS */
USED VISIBLE char* __wrap_getcwd(char* buf, size_t size) {
    _log("WRAP: getcwd(%p, %zu)\n", buf, size);
    if (buf) {
        strncpy(buf, "/dev_hdd0/game/RENPY0001/USRDIR", size);
        return buf;
    }
    return NULL;
}
USED VISIBLE int __wrap_chdir(const char* path) {
    _log("WRAP: chdir(%s)\n", path);
    return 0;
}
USED VISIBLE char* __wrap_getenv(const char* n) {
    char* r = __real_getenv(n);
    if (!_in_stub) _log("WRAP: getenv(%s) -> %s\n", n, r ? r : "NULL");
    return r;
}
USED VISIBLE int __wrap_isatty(int fd) {
    int res = (fd >= 0 && fd <= 2);
    if (!_in_stub) _log("WRAP: isatty(%d) -> %d\n", fd, res);
    return res;
}
USED VISIBLE char* __wrap_setlocale(int c, const char* l) {
    char* r = __real_setlocale(c, l);
    if (!_in_stub) _log("WRAP: setlocale(%d, %s)\n", c, l ? l : "NULL");
    return r;
}
USED VISIBLE int __wrap_fcntl(int fd, int cmd, ...) { _log("WRAP: fcntl(%d, %d)\n", fd, cmd); return 0; }
USED VISIBLE int __wrap_ioctl(int fd, unsigned long r, ...) { _log("WRAP: ioctl(%d, %lu)\n", fd, r); return -1; }
USED VISIBLE int __wrap_raise(int sig) { _log("WRAP: raise(%d)\n", sig); sysLv2FsFsync(_log_fd); while(1); return 0; }
USED VISIBLE int __wrap_kill(pid_t pid, int sig) { _log("WRAP: kill(%d, %d)\n", (int)pid, sig); sysLv2FsFsync(_log_fd); while(1); return 0; }

/* BASICS */
USED VISIBLE uid_t getuid(void) { return 0; }
USED VISIBLE gid_t getgid(void) { return 0; }
USED VISIBLE pid_t getpid(void) { return 100; }
USED VISIBLE FILE *popen(const char *c, const char *t) { return NULL; }
USED VISIBLE int pclose(FILE *s) { return -1; }
USED VISIBLE struct passwd *getpwuid(uid_t uid) { return NULL; }
USED VISIBLE struct passwd *getpwnam(const char *name) { return NULL; }
USED VISIBLE struct group *getgrgid(gid_t gid) { return NULL; }
USED VISIBLE struct group *getgrnam(const char *name) { return NULL; }

/* Python internal overrides */
void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads\n"); }
void* IMG_LoadTexture_RW(void* r, void* s, int f) { return NULL; }
