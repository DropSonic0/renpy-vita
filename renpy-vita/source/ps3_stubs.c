#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/memory.h>
#include <sys/process.h>
#include <lv2/sysfs.h>
#include <lv2/process.h>

#define USED __attribute__((used))
#define VISIBLE __attribute__((visibility("default")))

/* Native PS3 structures and syscalls */
/* Using the naming from PSL1GHT headers */

static int _in_stub = 0;
static int _log_fd = -1;

/* Log helper that syncs to disk immediately */
void _log(const char *fmt, ...) {
    if (_in_stub) return;
    _in_stub = 1;

    if (_log_fd < 0) {
        /* Use native open to avoid recursion */
        sysFsOpen("/dev_hdd0/game/RENPY0001/USRDIR/log.txt",
                     O_WRONLY | O_CREAT | O_APPEND, &_log_fd, NULL, 0);
    }

    if (_log_fd >= 0) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        if (len > 0) {
            u64 written = 0;
            sysFsWrite(_log_fd, buffer, (u64)len, &written);
        }
    }

    _in_stub = 0;
}

/* Safe log for signal handlers or inside wraps */
void _log_safe(const char *msg) {
    if (_log_fd < 0) {
        sysFsOpen("/dev_hdd0/game/RENPY0001/USRDIR/log.txt",
                     O_WRONLY | O_CREAT | O_APPEND, &_log_fd, NULL, 0);
    }
    if (_log_fd >= 0) {
        u64 written = 0;
        sysFsWrite(_log_fd, msg, (u64)strlen(msg), &written);
    }
}

/* Crash handler */
VISIBLE void ps3_crash_handler(int sig) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\n!!! FATAL CRASH: Signal %d !!!\n", sig);
    _log_safe(msg);

    /* Try to get some info */
    _log_safe("Process terminating...\n");

    sysProcessExit(sig);
}

/* --- File I/O Wraps --- */

extern int __real_open(const char *path, int flags, ...);
USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int fd = -1;
    int res = sysFsOpen(path, flags, &fd, NULL, 0);

    _log("open(\"%s\", 0x%x) -> res:%d, fd:%d\n", path, flags, res, fd);

    if (res != 0) {
        errno = res; // Mapping might be needed
        return -1;
    }
    return fd;
}
USED VISIBLE int __wrap_open64(const char *path, int flags, ...) {
    /* Redirect to our open wrap */
    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    return __wrap_open(path, flags, mode);
}
USED VISIBLE int __wrap__open(const char *path, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    return __wrap_open(path, flags, mode);
}

extern ssize_t __real_read(int fd, void *buf, size_t count);
USED VISIBLE ssize_t __wrap_read(int fd, void *buf, size_t count) {
    u64 nread = 0;
    int res = sysFsRead(fd, buf, (u64)count, &nread);

    // Too noisy for frequent reads, but useful for debugging hangs
    // _log("read(%d, %p, %u) -> res:%d, nread:%llu\n", fd, buf, count, res, nread);

    if (res != 0) {
        errno = res;
        return -1;
    }
    return (ssize_t)nread;
}
USED VISIBLE ssize_t __wrap__read(int fd, void *buf, size_t count) {
    return __wrap_read(fd, buf, count);
}

extern ssize_t __real_write(int fd, const void *buf, size_t count);
USED VISIBLE ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    /* Capture stdout/stderr for our log */
    if (fd == 1 || fd == 2) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "PY_OUT(%d): ", fd);
        _log_safe(prefix);

        /* Write to log in chunks if too large */
        u64 written = 0;
        if (_log_fd >= 0) {
             sysFsWrite(_log_fd, buf, (u64)count, &written);
        }
        return (ssize_t)count;
    }

    u64 nwritten = 0;
    int res = sysFsWrite(fd, buf, (u64)count, &nwritten);

    if (res != 0) {
        errno = res;
        return -1;
    }
    return (ssize_t)nwritten;
}
USED VISIBLE ssize_t __wrap__write(int fd, const void *buf, size_t count) {
    return __wrap_write(fd, buf, count);
}

extern off_t __real_lseek(int fd, off_t offset, int whence);
USED VISIBLE off_t __wrap_lseek(int fd, off_t offset, int whence) {
    /* Safety for standard streams */
    if (fd >= 0 && fd <= 2) {
        return 0;
    }

    u64 pos = 0;
    int res = sysFsLseek(fd, (s64)offset, whence, &pos);

    if (res != 0) {
        errno = res;
        return -1;
    }
    return (off_t)pos;
}
USED VISIBLE off_t __wrap_lseek64(int fd, off_t offset, int whence) {
    return __wrap_lseek(fd, offset, whence);
}
USED VISIBLE off_t __wrap__lseek(int fd, off_t offset, int whence) {
    return __wrap_lseek(fd, offset, whence);
}

extern int __real_close(int fd);
USED VISIBLE int __wrap_close(int fd) {
    if (fd == _log_fd) {
        _log_fd = -1;
    }
    int res = sysFsClose(fd);
    _log("close(%d) -> %d\n", fd, res);
    if (res != 0) {
        errno = res;
        return -1;
    }
    return 0;
}
USED VISIBLE int __wrap__close(int fd) {
    return __wrap_close(fd);
}

/* Helper to map PS3 Stat to Newlib Stat */
static void map_stat(sysFSStat *ps3_st, struct stat *st) {
    memset(st, 0, sizeof(struct stat));
    st->st_mode = ps3_st->st_mode;
    st->st_size = ps3_st->st_size;
    st->st_atime = ps3_st->st_atime;
    st->st_mtime = ps3_st->st_mtime;
    st->st_ctime = ps3_st->st_ctime;
    st->st_uid = ps3_st->st_uid;
    st->st_gid = ps3_st->st_gid;

    /* Python often expects S_IFREG for regular files */
    /* PS3 mode might need conversion depending on toolchain */
}

USED VISIBLE int __wrap_fstat(int fd, struct stat *buf) {
    /* Safety for standard streams */
    if (fd >= 0 && fd <= 2) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = S_IFCHR;
        return 0;
    }

    sysFSStat ps3_st;
    int res = sysFsFstat(fd, &ps3_st);
    if (res == 0) {
        map_stat(&ps3_st, buf);
        _log("fstat(%d) -> size:%lld\n", fd, (long long)buf->st_size);
        return 0;
    }

    /* Fallback using lseek if FStat fails */
    u64 current = 0, end = 0;
    sysFsLseek(fd, 0, SEEK_CUR, &current);
    sysFsLseek(fd, 0, SEEK_END, &end);
    sysFsLseek(fd, (s64)current, SEEK_SET, &current);

    memset(buf, 0, sizeof(struct stat));
    buf->st_mode = S_IFREG | 0666;
    buf->st_size = end;
    _log("fstat(%d) FALLBACK -> size:%lld\n", fd, (long long)buf->st_size);
    return 0;
}
USED VISIBLE int __wrap_fstat64(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }
USED VISIBLE int __wrap__fstat(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }

USED VISIBLE int __wrap_stat(const char *path, struct stat *buf) {
    sysFSStat ps3_st;
    int res = sysFsStat(path, &ps3_st);
    if (res == 0) {
        map_stat(&ps3_st, buf);
        return 0;
    }
    _log("stat(\"%s\") FAILED: %d\n", path, res);
    errno = ENOENT;
    return -1;
}
USED VISIBLE int __wrap_stat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__stat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

USED VISIBLE int __wrap_lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap_lstat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

/* --- Directory Wraps --- */
USED VISIBLE void* __wrap_opendir(const char *name) { _log("STUB: opendir %s\n", name); return NULL; }
USED VISIBLE void* __wrap_readdir(void *dirp) { return NULL; }
USED VISIBLE int __wrap_closedir(void *dirp) { return 0; }

/* --- Memory Wraps --- */
static int malloc_count = 0;
extern void* __real_malloc(size_t size);
USED VISIBLE void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    malloc_count++;
    if (malloc_count % 1000 == 0) {
        _log("malloc(%u) -> %p (count: %d)\n", (unsigned int)size, ptr, malloc_count);
    }
    return ptr;
}

extern void __real_free(void* ptr);
USED VISIBLE void __wrap_free(void* ptr) {
    __real_free(ptr);
}

extern void* __real_realloc(void* ptr, size_t size);
USED VISIBLE void* __wrap_realloc(void* ptr, size_t size) {
    return __real_realloc(ptr, size);
}

extern void* __real_calloc(size_t nmemb, size_t size);
USED VISIBLE void* __wrap_calloc(size_t nmemb, size_t size) {
    return __real_calloc(nmemb, size);
}

USED VISIBLE void* __wrap_sbrk(intptr_t increment) {
    _log("STUB: sbrk(%d)\n", (int)increment);
    errno = ENOMEM;
    return (void*)-1;
}

USED VISIBLE void* __wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    _log("STUB: mmap fd:%d len:%u\n", fd, (unsigned int)length);
    errno = ENOMEM;
    return (void*)-1;
}

/* --- System/Process Wraps --- */
USED VISIBLE void __wrap_exit(int status) {
    char msg[64];
    snprintf(msg, sizeof(msg), "\n--- EXIT CALLED (status: %d) ---\n", status);
    _log_safe(msg);
    sysProcessExit(status);
}
USED VISIBLE void __wrap__exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap__Exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap_abort(void) {
    _log_safe("\n!!! ABORT CALLED !!!\n");
    sysProcessExit(1);
}

USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    if (tv) {
        tv->tv_sec = time(NULL);
        tv->tv_usec = 0;
    }
    return 0;
}

USED VISIBLE int __wrap_isatty(int fd) {
    if (fd >= 0 && fd <= 2) return 1;
    return 0;
}

USED VISIBLE char* __wrap_getenv(const char *name) {
    if (strcmp(name, "PYTHONHOME") == 0) return "/dev_hdd0/game/RENPY0001/USRDIR";
    return NULL;
}

USED VISIBLE char* __wrap_getcwd(char *buf, size_t size) {
    snprintf(buf, size, "/dev_hdd0/game/RENPY0001/USRDIR");
    return buf;
}

USED VISIBLE int __wrap_chdir(const char *path) {
    return 0;
}

extern void (*__real_signal(int signum, void (*handler)(int)))(int);
USED VISIBLE void (*__wrap_signal(int signum, void (*handler)(int)))(int) {
    _log("signal(%d, %p)\n", signum, handler);
    return __real_signal(signum, handler);
}

extern int __real_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
USED VISIBLE int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    _log("sigaction(%d)\n", signum);
    return __real_sigaction(signum, act, oldact);
}

USED VISIBLE int __wrap_kill(pid_t pid, int sig) { return 0; }
USED VISIBLE int __wrap_raise(int sig) { return 0; }

USED VISIBLE int __wrap_fflush(FILE *stream) { return 0; }

USED VISIBLE int __wrap_fcntl(int fd, int cmd, ...) {
    return 0;
}

USED VISIBLE int __wrap_ioctl(int fd, unsigned long request, ...) {
    return -1;
}

USED VISIBLE void __wrap___assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    char msg[512];
    snprintf(msg, sizeof(msg), "ASSERT FAILED: %s at %s:%u (%s)\n", assertion, file, line, function);
    _log_safe(msg);
    sysProcessExit(1);
}

USED VISIBLE void __wrap___assert(const char *file, int line, const char *assertion) {
    __wrap___assert_fail(assertion, file, (unsigned int)line, "unknown");
}

USED VISIBLE int __wrap_setlocale(int category, const char *locale) { return 0; }

USED VISIBLE uid_t __wrap_getuid() { return 0; }
USED VISIBLE gid_t __wrap_getgid() { return 0; }
USED VISIBLE uid_t __wrap_geteuid() { return 0; }
USED VISIBLE gid_t __wrap_getegid() { return 0; }
USED VISIBLE pid_t __wrap_getpid() { return 100; }
USED VISIBLE pid_t __wrap_getppid() { return 1; }

USED VISIBLE int __wrap_pipe(int fildes[2]) { errno = ENOSYS; return -1; }
USED VISIBLE int __wrap_symlink(const char *path1, const char *path2) { errno = EROFS; return -1; }
USED VISIBLE int __wrap_fdatasync(int fildes) { return 0; }
USED VISIBLE char * __wrap_ttyname(int fd) { return NULL; }
USED VISIBLE int __wrap_execv(const char *path, char *const argv[]) { errno = ENOSYS; return -1; }
USED VISIBLE int __wrap_readlink(const char *path, char *buf, size_t bufsiz) { errno = ENOSYS; return -1; }
USED VISIBLE int __wrap_gethostname(char *name, size_t len) { snprintf(name, len, "ps3"); return 0; }
USED VISIBLE long __wrap_sysconf(int name) { return -1; }

/* Missing POSIX stubs required by Python 2.7 */
VISIBLE FILE *popen(const char *command, const char *type) { return NULL; }
VISIBLE int pclose(FILE *stream) { return -1; }
VISIBLE struct passwd *getpwuid(uid_t uid) { return NULL; }
VISIBLE struct passwd *getpwnam(const char *name) { return NULL; }
VISIBLE struct group *getgrgid(gid_t gid) { return NULL; }
VISIBLE struct group *getgrnam(const char *name) { return NULL; }

/* SDL_image stubs */
VISIBLE void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) {
    return NULL;
}
