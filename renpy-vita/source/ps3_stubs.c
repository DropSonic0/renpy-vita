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

/* Declarations for the "real" (original) functions from libc */
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

/* Internal logging helper using the real open/write to avoid recursion */
void _log(const char *fmt, ...) {
    if (_in_stub) return;
    _in_stub = 1;

    if (_log_fd < 0) {
        /* Open log in append mode, creating it if it doesn't exist */
        _log_fd = __real_open("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (_log_fd >= 0) {
            __real_write(_log_fd, "\n--- LOG SYSTEM INITIALIZED ---\n", 32);
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

void _log_safe(const char *msg) {
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

/* Crash handler called from main.c */
VISIBLE void ps3_crash_handler(int sig) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\n!!! FATAL CRASH: Signal %d !!!\n", sig);
    _log_safe(msg);
    _log_safe("Process terminating...\n");
    __real_exit(sig);
}

/* --- I/O Wraps --- */

USED VISIBLE int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int fd = __real_open(path, flags, mode);
    _log("open(\"%s\", 0x%x) -> %d\n", path, flags, fd);
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
    return __real_read(fd, buf, count);
}
USED VISIBLE ssize_t __wrap__read(int fd, void *buf, size_t count) {
    return __wrap_read(fd, buf, count);
}

USED VISIBLE ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    /* Special handling for stdout/stderr to mirror them to the log file */
    if (fd == 1 || fd == 2) {
        /* Ensure log is initialized */
        if (_log_fd < 0) {
             _log_fd = __real_open("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
        }

        if (_log_fd >= 0 && fd != _log_fd) {
            _in_stub = 1;
            __real_write(_log_fd, fd == 1 ? "STDOUT: " : "STDERR: ", 8);
            __real_write(_log_fd, buf, count);
            _in_stub = 0;
        }
        /* We still let it go through the real write in case it's actually visible somewhere */
        return __real_write(fd, buf, count);
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
    /* Pretend standard streams are character devices to avoid hangs in some libraries */
    if (fd >= 0 && fd <= 2) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = S_IFCHR;
        return 0;
    }
    int res = __real_fstat(fd, buf);
    if (res == 0) {
         // _log("fstat(%d) -> size:%lld\n", fd, (long long)buf->st_size);
    }
    return res;
}
USED VISIBLE int __wrap_fstat64(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }
USED VISIBLE int __wrap__fstat(int fd, struct stat *buf) { return __wrap_fstat(fd, buf); }

USED VISIBLE int __wrap_stat(const char *path, struct stat *buf) {
    int res = __real_stat(path, buf);
    if (res != 0) {
        _log("stat(\"%s\") FAILED\n", path);
    }
    return res;
}
USED VISIBLE int __wrap_stat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__stat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

USED VISIBLE int __wrap_lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap_lstat64(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }
USED VISIBLE int __wrap__lstat(const char *path, struct stat *buf) { return __wrap_stat(path, buf); }

/* --- Directory Wraps --- */
USED VISIBLE void* __wrap_opendir(const char *name) { _log("opendir(\"%s\")\n", name); return NULL; }
USED VISIBLE void* __wrap_readdir(void *dirp) { return NULL; }
USED VISIBLE int __wrap_closedir(void *dirp) { return 0; }

/* --- Memory Wraps --- */
static int malloc_count = 0;
USED VISIBLE void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if (++malloc_count % 1000 == 0) _log("malloc(%u) -> %p (count: %d)\n", (unsigned int)size, ptr, malloc_count);
    return ptr;
}
USED VISIBLE void __wrap_free(void* ptr) { __real_free(ptr); }
USED VISIBLE void* __wrap_realloc(void* ptr, size_t size) { return __real_realloc(ptr, size); }
USED VISIBLE void* __wrap_calloc(size_t nmemb, size_t size) { return __real_calloc(nmemb, size); }

USED VISIBLE void* __wrap_sbrk(intptr_t increment) { errno = ENOMEM; return (void*)-1; }
USED VISIBLE void* __wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) { errno = ENOMEM; return (void*)-1; }

/* --- System Wraps --- */
USED VISIBLE void __wrap_exit(int status) { _log("exit(%d)\n", status); __real_exit(status); }
USED VISIBLE void __wrap__exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap__Exit(int status) { __wrap_exit(status); }
USED VISIBLE void __wrap_abort(void) { _log("abort()\n"); __real_abort(); }

USED VISIBLE int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    if (tv) { tv->tv_sec = time(NULL); tv->tv_usec = 0; }
    return 0;
}

USED VISIBLE int __wrap_isatty(int fd) { return (fd >= 0 && fd <= 2); }
USED VISIBLE char* __wrap_getenv(const char *name) { return NULL; }
USED VISIBLE char* __wrap_getcwd(char *buf, size_t size) { snprintf(buf, size, "/dev_hdd0/game/RENPY0001/USRDIR"); return buf; }
USED VISIBLE int __wrap_chdir(const char *path) { return 0; }

USED VISIBLE void (*__wrap_signal(int signum, void (*handler)(int)))(int) {
    _log("signal(%d, %p)\n", signum, handler);
    return __real_signal(signum, handler);
}

USED VISIBLE int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    _log("sigaction(%d)\n", signum);
    return __real_sigaction(signum, act, oldact);
}

USED VISIBLE int __wrap_kill(pid_t pid, int sig) { return 0; }
USED VISIBLE int __wrap_raise(int sig) { return 0; }
USED VISIBLE int __wrap_fflush(FILE *stream) { return 0; }
USED VISIBLE int __wrap_fcntl(int fd, int cmd, ...) { return 0; }
USED VISIBLE int __wrap_ioctl(int fd, unsigned long request, ...) { return -1; }

USED VISIBLE void __wrap___assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    _log("ASSERT FAILED: %s at %s:%u\n", assertion, file, line);
    __real_exit(1);
}
USED VISIBLE void __wrap___assert(const char *file, int line, const char *assertion) { __wrap___assert_fail(assertion, file, line, ""); }

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

/* Missing POSIX stubs required by the Python build */
VISIBLE FILE *popen(const char *command, const char *type) { return NULL; }
VISIBLE int pclose(FILE *stream) { return -1; }
VISIBLE struct passwd *getpwuid(uid_t uid) { return NULL; }
VISIBLE struct passwd *getpwnam(const char *name) { return NULL; }
VISIBLE struct group *getgrgid(gid_t gid) { return NULL; }
VISIBLE struct group *getgrnam(const char *name) { return NULL; }

/* Python internal stub that's missing from some static builds */
VISIBLE void PyEval_InitThreads() { }

/* SDL_image compatibility stub */
VISIBLE void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) { return NULL; }
