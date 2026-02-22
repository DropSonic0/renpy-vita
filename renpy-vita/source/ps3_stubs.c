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

#define USED __attribute__((used))
#define VISIBLE __attribute__((visibility("default")))

static int _in_stub = 0;
static int _log_fd = -1;
static unsigned int malloc_count = 0;
static unsigned int read_count = 0;
static unsigned long long total_read_bytes = 0;

/* Declaraciones para las funciones reales de la libc */
extern int __real_open(const char *path, int flags, ...);
extern int __real_close(int fd);
extern ssize_t __real_read(int fd, void *buf, size_t count);
extern ssize_t __real_write(int fd, const void *buf, size_t count);
extern off_t __real_lseek(int fd, off_t offset, int whence);
extern int __real_fstat(int fd, struct stat *buf);
extern int __real_stat(const char *path, struct stat *buf);
extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void __real_exit(int status);
extern void __real_abort(void);
extern void (*__real_signal(int signum, void (*handler)(int)))(int);
extern int __real_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

/* Función de log principal */
VISIBLE void ps3_log(const char *fmt, ...) {
    if (_in_stub) return;
    _in_stub = 1;

    if (_log_fd < 0) {
        _log_fd = __real_open("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (_log_fd >= 0) {
            __real_write(_log_fd, "\n--- PS3 LOG RESTARTED ---\n", 27);
        }
    }

    if (_log_fd >= 0) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        if (len > 0) {
            __real_write(_log_fd, buffer, (size_t)len);
        }
    }

    _in_stub = 0;
}

VISIBLE void ps3_log_safe(const char *msg) {
    if (_in_stub) return;
    _in_stub = 1;
    if (_log_fd < 0) {
        _log_fd = __real_open("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    }
    if (_log_fd >= 0) {
        __real_write(_log_fd, msg, strlen(msg));
    }
    _in_stub = 0;
}

VISIBLE void ps3_crash_handler(int sig) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\n!!! CRASH DETECTED: Signal %d !!!\n", sig);
    ps3_log_safe(msg);
    __real_exit(sig);
}

/* --- I/O --- */

USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = __real_open(path, flags, mode);
    ps3_log("OPEN: \"%s\" (flags: 0x%x) -> %d\n", path, flags, fd);
    return fd;
}
USED VISIBLE int __wrap_open64(const char *path, int flags, ...) {
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

USED VISIBLE ssize_t __wrap_read(int fd, void *buf, size_t count) {
    ssize_t res = __real_read(fd, buf, count);
    if (res > 0) {
        total_read_bytes += res;
    }
    if (++read_count % 200 == 0) {
        ps3_log("READ Heartbeat: fd=%d, count=%u, total_read=%llu\n", fd, (unsigned int)count, total_read_bytes);
    }
    return res;
}
USED VISIBLE ssize_t __wrap__read(int fd, void *buf, size_t count) {
    return __wrap_read(fd, buf, count);
}

USED VISIBLE ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) {
        if (_log_fd >= 0 && fd != _log_fd) {
            _in_stub = 1;
            __real_write(_log_fd, fd == 1 ? "STDOUT: " : "STDERR: ", 8);
            __real_write(_log_fd, buf, count);
            _in_stub = 0;
        }
    }
    return __real_write(fd, buf, count);
}
USED VISIBLE ssize_t __wrap__write(int fd, const void *buf, size_t count) {
    return __wrap_write(fd, buf, count);
}

USED VISIBLE off_t __wrap_lseek(int fd, off_t offset, int whence) {
    if (fd >= 0 && fd <= 2) return 0;
    return __real_lseek(fd, offset, whence);
}
USED VISIBLE off_t __wrap_lseek64(int fd, off_t offset, int whence) {
    return __wrap_lseek(fd, offset, whence);
}
USED VISIBLE off_t __wrap__lseek(int fd, off_t offset, int whence) {
    return __wrap_lseek(fd, offset, whence);
}

USED VISIBLE int __wrap_close(int fd) {
    if (fd == _log_fd) _log_fd = -1;
    return __real_close(fd);
}
USED VISIBLE int __wrap__close(int fd) {
    return __wrap_close(fd);
}

USED VISIBLE int __wrap_fstat(int fd, struct stat *buf) {
    if (fd >= 0 && fd <= 2) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = S_IFCHR;
        return 0;
    }
    return __real_fstat(fd, buf);
}
USED VISIBLE int __wrap_fstat64(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }
USED VISIBLE int __wrap__fstat(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }

USED VISIBLE int __wrap_stat(const char *path, struct stat *buf) {
    int res = __real_stat(path, buf);
    ps3_log("STAT: \"%s\" -> %d\n", path, res);
    return res;
}
USED VISIBLE int __wrap_stat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__stat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

USED VISIBLE int __wrap_lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap_lstat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

/* --- Directorios --- */
USED VISIBLE void* __wrap_opendir(const char *name) { ps3_log("OPENDIR: \"%s\"\n", name); return NULL; }
USED VISIBLE void* __wrap_readdir(void *dirp) { return NULL; }
USED VISIBLE int __wrap_closedir(void *dirp) { return 0; }

/* --- Memoria --- */
USED VISIBLE void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if (++malloc_count % 50 == 0) {
        ps3_log("MALLOC Heartbeat: %d calls, last size: %u -> %p\n", malloc_count, (unsigned int)size, ptr);
    }
    if (ptr == NULL && size > 0) {
        ps3_log("MALLOC FAILED: %u bytes\n", (unsigned int)size);
    }
    return ptr;
}
USED VISIBLE void __wrap_free(void* ptr) { __real_free(ptr); }
USED VISIBLE void* __wrap_realloc(void* ptr, size_t size) { return __real_realloc(ptr, size); }
USED VISIBLE void* __wrap_calloc(size_t nmemb, size_t size) { return __real_calloc(nmemb, size); }

/* --- Sistema --- */
USED VISIBLE void __wrap_exit(int status) { ps3_log("EXIT: %d\n", status); __real_exit(status); }
USED VISIBLE void __wrap__exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap__Exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap_abort(void) { ps3_log("ABORT\n"); __real_abort(); }

USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    if (tv) { tv->tv_sec = time(NULL); tv->tv_usec = 0; }
    return 0;
}

USED VISIBLE int __wrap_isatty(int fd) { return 0; }
USED VISIBLE char* __wrap_getenv(const char *name) { ps3_log("GETENV: \"%s\"\n", name); return NULL; }

USED VISIBLE char* __wrap_getcwd(char *buf, size_t size) {
    static char static_path[] = "/dev_hdd0/game/RENPY0001/USRDIR";
    if (buf == NULL) return static_path;
    strncpy(buf, static_path, size);
    return buf;
}

USED VISIBLE int __wrap_chdir(const char *path) { ps3_log("CHDIR: \"%s\"\n", path); return 0; }

USED VISIBLE char* __wrap_setlocale(int category, const char *locale) {
    ps3_log("SETLOCALE: cat=%d loc=%s\n", category, locale ? locale : "NULL");
    return "C";
}

USED VISIBLE void (*__wrap_signal(int signum, void (*handler)(int)))(int) {
    ps3_log("SIGNAL: %d\n", signum);
    return __real_signal(signum, handler);
}

USED VISIBLE int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    ps3_log("SIGACTION: %d\n", signum);
    return __real_sigaction(signum, act, oldact);
}

USED VISIBLE int __wrap_kill(pid_t pid, int sig) { return 0; }
USED VISIBLE int __wrap_raise(int sig) { return 0; }
USED VISIBLE int __wrap_fflush(FILE *stream) { return 0; }
USED VISIBLE int __wrap_fcntl(int fd, int cmd, ...) { return 0; }
USED VISIBLE int __wrap_ioctl(int fd, unsigned long request, ...) { return -1; }

USED VISIBLE void __wrap___assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    ps3_log("ASSERT: %s at %s:%u\n", assertion, file, line);
    __real_exit(1);
}
USED VISIBLE void __wrap___assert(const char *file, int line, const char *assertion) { __wrap___assert_fail(assertion, file, line, ""); }


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

/* Stubs */
VISIBLE FILE *popen(const char *command, const char *type) { return NULL; }
VISIBLE int pclose(FILE *stream) { return -1; }
VISIBLE struct passwd *getpwuid(uid_t uid) { return NULL; }
VISIBLE struct passwd *getpwnam(const char *name) { return NULL; }
VISIBLE struct group *getgrgid(gid_t gid) { return NULL; }
VISIBLE struct group *getgrnam(const char *name) { return NULL; }
VISIBLE void PyEval_InitThreads() { }
VISIBLE void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) { return NULL; }
