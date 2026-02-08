#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <locale.h>
#include <stdarg.h>

/* Try to include pthread and mman for better wrapping */
#include <pthread.h>
#include <sys/mman.h>

/* Global guard for recursion in wrappers */
static int in_wrap = 0;

/* Real functions from the toolchain */
extern void* __real_malloc(size_t size);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);
extern void  __real_free(void* ptr);
extern int   __real_open(const char *pathname, int flags, ...);
extern ssize_t __real_read(int fd, void *buf, size_t count);
extern ssize_t __real_write(int fd, const void *buf, size_t count);
extern int   __real_stat(const char *path, struct stat *buf);
extern int   __real_fstat(int fd, struct stat *buf);
extern off_t __real_lseek(int fd, off_t offset, int whence);
extern int   __real_close(int fd);
extern int   __real_access(const char *pathname, int mode);
extern int   __real_isatty(int fd);
extern char* __real_getenv(const char *name);
extern int   __real_signal(int signum, void (*handler)(int));
extern int   __real_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
extern char* __real_setlocale(int category, const char *locale);
extern void* __real_sbrk(intptr_t increment);
extern int   __real_gettimeofday(struct timeval *tv, void *tz);
extern int   __real_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
extern int   __real_pthread_mutex_lock(pthread_mutex_t *mutex);
extern int   __real_pthread_mutex_unlock(pthread_mutex_t *mutex);
extern int   __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

/* Shared logging mechanism */
void log_me(const char* msg) {
    if (!msg) return;
    /* Direct write to both stdout and a file on disk */
    __real_write(1, msg, strlen(msg));
    static int log_fd = -1;
    if (log_fd == -1) {
        log_fd = __real_open("/dev_hdd0/game/RENPY0001/USRDIR/log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    }
    if (log_fd != -1) {
        __real_write(log_fd, msg, strlen(msg));
    }
}

/* Helper to print and log a message safely */
static void wrap_log(const char* fmt, ...) {
    if (in_wrap) return;
    in_wrap = 1;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    log_me(buffer);
    in_wrap = 0;
}

/* Wrappers */

void* __wrap_malloc(size_t size) {
    void* res = __real_malloc(size);
    wrap_log("WRAP: malloc(%zu) = %p\n", size, res);
    return res;
}

void* __wrap_realloc(void* ptr, size_t size) {
    void* res = __real_realloc(ptr, size);
    wrap_log("WRAP: realloc(%p, %zu) = %p\n", ptr, size, res);
    return res;
}

void* __wrap_calloc(size_t nmemb, size_t size) {
    void* res = __real_calloc(nmemb, size);
    wrap_log("WRAP: calloc(%zu, %zu) = %p\n", nmemb, size, res);
    return res;
}

void __wrap_free(void* ptr) {
    if (ptr) wrap_log("WRAP: free(%p)\n", ptr);
    __real_free(ptr);
}

#define DUMMY_URANDOM_FD 999

int __wrap_open(const char *pathname, int flags, ...) {
    wrap_log("WRAP: open(%s, 0x%x)\n", pathname, flags);

    if (pathname && (strcmp(pathname, "/dev/urandom") == 0 || strcmp(pathname, "/dev/random") == 0)) {
        wrap_log("WRAP: open(/dev/urandom) - providing dummy fd %d\n", DUMMY_URANDOM_FD);
        return DUMMY_URANDOM_FD;
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);
    }
    return __real_open(pathname, flags, mode);
}

ssize_t __wrap_read(int fd, void *buf, size_t count) {
    if (fd == DUMMY_URANDOM_FD) {
        wrap_log("WRAP: read(DUMMY_URANDOM, count=%zu)\n", count);
        memset(buf, 0x42, count); // Dummy predictable data
        return count;
    }

    wrap_log("WRAP: read(%d, count=%zu) entering\n", fd, count);
    ssize_t res = __real_read(fd, buf, count);
    wrap_log("WRAP: read(%d) returned %zd\n", fd, res);
    return res;
}

ssize_t __wrap_write(int fd, const void *buf, size_t count) {
    if (in_wrap) return __real_write(fd, buf, count);

    /* If writing to stdout/stderr from Python, prefix it */
    if (fd == 1 || fd == 2) {
        char prefix[32];
        snprintf(prefix, 32, "PYTHON-OUT[%d]: ", fd);
        __real_write(1, prefix, strlen(prefix));
        __real_write(1, buf, count);
        wrap_log("WRAP: write(%d, count=%zu)\n", fd, count);
        return count;
    }

    return __real_write(fd, buf, count);
}

int __wrap_stat(const char *path, struct stat *buf) {
    wrap_log("WRAP: stat(%s)\n", path);
    return __real_stat(path, buf);
}

int __wrap_fstat(int fd, struct stat *buf) {
    wrap_log("WRAP: fstat(%d)\n", fd);
    int res = __real_fstat(fd, buf);
    /* For standard fds, ensure they look like chars if possible */
    if (fd >= 0 && fd <= 2) {
        if (res != 0) {
            if (buf) { memset(buf, 0, sizeof(struct stat)); buf->st_mode = S_IFCHR; }
            return 0;
        }
        if (buf) buf->st_mode |= S_IFCHR;
    }
    return res;
}

off_t __wrap_lseek(int fd, off_t offset, int whence) {
    wrap_log("WRAP: lseek(%d, %ld, %d)\n", fd, (long)offset, whence);
    return __real_lseek(fd, offset, whence);
}

int __wrap_close(int fd) {
    if (fd == DUMMY_URANDOM_FD) {
        wrap_log("WRAP: close(DUMMY_URANDOM)\n");
        return 0;
    }
    wrap_log("WRAP: close(%d)\n", fd);
    return __real_close(fd);
}

int __wrap_access(const char *pathname, int mode) {
    wrap_log("WRAP: access(%s, %d)\n", pathname, mode);
    return __real_access(pathname, mode);
}

int __wrap_isatty(int fd) {
    wrap_log("WRAP: isatty(%d)\n", fd);
    return 0; /* Say no to TTY to avoid complex terminal code */
}

char* __wrap_getenv(const char *name) {
    char* res = __real_getenv(name);
    wrap_log("WRAP: getenv(%s) = %s\n", name, res ? res : "NULL");
    return res;
}

int __wrap_signal(int signum, void (*handler)(int)) {
    wrap_log("WRAP: signal(%d, %p)\n", signum, handler);
    return 0; /* Fake success */
}

int __wrap_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    wrap_log("WRAP: sigaction(%d)\n", signum);
    return 0; /* Fake success */
}

char* __wrap_setlocale(int category, const char *locale) {
    wrap_log("WRAP: setlocale(%d, %s)\n", category, locale ? locale : "NULL");
    return "C"; /* Always return C locale */
}

void* __wrap_sbrk(intptr_t increment) {
    void* res = __real_sbrk(increment);
    wrap_log("WRAP: sbrk(%ld) = %p\n", (long)increment, res);
    return res;
}

int __wrap_gettimeofday(struct timeval *tv, void *tz) {
    /* No log here, too frequent */
    return __real_gettimeofday(tv, tz);
}

/* Pthread wrappers */
int __wrap_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    wrap_log("WRAP: pthread_mutex_init(%p)\n", mutex);
    return __real_pthread_mutex_init(mutex, attr);
}

int __wrap_pthread_mutex_lock(pthread_mutex_t *mutex) {
    /* Only log every 1000th lock to avoid spam, or just don't log */
    static int lcount = 0;
    if (lcount++ % 1000 == 0) wrap_log("WRAP: pthread_mutex_lock(%p) (sampled)\n", mutex);
    return __real_pthread_mutex_lock(mutex);
}

int __wrap_pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return __real_pthread_mutex_unlock(mutex);
}

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
    wrap_log("WRAP: pthread_create called! routine=%p\n", start_routine);
    return __real_pthread_create(thread, attr, start_routine, arg);
}

/* Stubs for missing POSIX functions */
int symlink(const char *target, const char *linkpath) { return -1; }
int popen(const char *command, const char *type) { return -1; }
int pclose(FILE *stream) { return -1; }
int fdatasync(int fd) { return 0; }
char *ttyname(int fd) { return NULL; }
int getuid() { return 0; }
int geteuid() { return 0; }
int getgid() { return 0; }
int getegid() { return 0; }
int setuid(uid_t uid) { return 0; }
int setgid(gid_t gid) { return 0; }
int fork() { return -1; }
int pipe(int pipefd[2]) { return -1; }
int waitpid(pid_t pid, int *status, int options) { return -1; }
int execv(const char *path, char *const argv[]) { return -1; }
int kill(pid_t pid, int sig) { return -1; }
int sigemptyset(sigset_t *set) { return 0; }
int sigfillset(sigset_t *set) { return 0; }
int sigaddset(sigset_t *set, int signum) { return 0; }
int sigdelset(sigset_t *set, int signum) { return 0; }
int sigismember(const sigset_t *set, int signum) { return 0; }
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) { return 0; }
int sigpending(sigset_t *set) { return 0; }
int sigsuspend(const sigset_t *mask) { return 0; }

/* Constructor to announce existence */
__attribute__((constructor)) void ps3_stubs_init() {
    log_me("Ren'Py PS3: [V21-TRACE] ps3_stubs constructor called\n");
}
