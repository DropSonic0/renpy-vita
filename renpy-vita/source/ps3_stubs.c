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
    char buf[1024];
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

/* Map POSIX open flags to PS3 native flags */
static s32 translate_open_flags(int flags) {
    s32 out = 0;
    /* Access modes */
    if ((flags & O_ACCMODE) == O_RDONLY) out |= SYS_O_RDONLY;
    if ((flags & O_ACCMODE) == O_WRONLY) out |= SYS_O_WRONLY;
    if ((flags & O_ACCMODE) == O_RDWR)   out |= SYS_O_RDWR;

    /* Other flags */
    if (flags & O_CREAT)  out |= SYS_O_CREAT;
    if (flags & O_EXCL)   out |= SYS_O_EXCL;
    if (flags & O_TRUNC)  out |= SYS_O_TRUNC;
    if (flags & O_APPEND) out |= SYS_O_APPEND;

    return out;
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
#undef open
#undef close
#undef read
#undef write
#undef lseek

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

int pipe(int fildes[2]) { _log("STUB: pipe\n"); errno = ENOSYS; return -1; }
int fork() { _log("STUB: fork\n"); errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { _log("STUB: execv %s\n", path); errno = ENOSYS; return -1; }
int symlink(const char *path1, const char *path2) { _log("STUB: symlink %s -> %s\n", path1, path2); errno = EROFS; return -1; }
int fdatasync(int fildes) { return 0; }
char *ttyname(int fd) { _log("STUB: ttyname %d\n", fd); return NULL; }
int readlink(const char *path, char *buf, size_t bufsiz) { _log("STUB: readlink %s\n", path); errno = ENOSYS; return -1; }
int gethostname(char *name, size_t len) { _log("STUB: gethostname\n"); snprintf(name, len, "ps3"); return 0; }
long sysconf(int name) { _log("STUB: sysconf %d\n", name); return -1; }
int isatty(int fd) { return 0; }

// Helper to map sysFSStat to struct stat
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
    buf->st_blksize = (long)lv2_st->st_blksize;
    if (buf->st_blksize <= 0) buf->st_blksize = 4096;

    buf->st_nlink = 1;
    buf->st_dev = 1;
    buf->st_ino = 1;
    buf->st_blocks = (buf->st_size + 511) / 512;
}

int fstat(int fd, struct stat *buf) {
    if (_in_stub) return -1;

    // Check for standard streams FIRST to avoid syscalls that might hang/fail
    if (fd >= 0 && fd <= 2) {
        if (buf) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0020000 | 0666; // S_IFCHR
            buf->st_nlink = 1;
            buf->st_blksize = 4096;
        }
        _log("STUB: fstat(%d) -> SUCCESS (Std Stream)\n", fd);
        return 0;
    }

    _in_stub = 1;
    sysFSStat lv2_st;
    memset(&lv2_st, 0, sizeof(sysFSStat));
    s32 res = sysLv2FsFStat((s32)fd, &lv2_st);

    u64 orig_pos = 0, end_pos = 0;
    sysLv2FsLSeek64((s32)fd, 0, 1, &orig_pos); // SEEK_CUR
    sysLv2FsLSeek64((s32)fd, 0, 2, &end_pos);  // SEEK_END
    sysLv2FsLSeek64((s32)fd, orig_pos, 0, &orig_pos); // SEEK_SET (restore)

    if (res == 0) {
        map_stat(buf, &lv2_st);
        if (buf->st_size == 0 && end_pos > 0) buf->st_size = (off_t)end_pos;
        _in_stub = 0;
        _log("STUB: fstat(%d) -> size %lld, mode %x\n", fd, (long long)buf->st_size, (unsigned int)buf->st_mode);
        return 0;
    }

    // Fallback for non-standard streams if syscall fails
    if (buf) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = 0100000 | 0666; // S_IFREG
        buf->st_size = (off_t)end_pos;
        buf->st_blksize = 4096;
    }
    _in_stub = 0;
    _log("STUB: fstat(%d) -> FALLBACK %x, size %lld\n", fd, (unsigned int)res, (long long)end_pos);
    return 0;
}

int stat(const char *path, struct stat *buf) {
    if (_in_stub) return -1;
    _in_stub = 1;
    sysFSStat lv2_st;
    s32 res = sysLv2FsStat(path, &lv2_st);
    if (res == 0) {
        map_stat(buf, &lv2_st);
        _in_stub = 0;
        _log("STUB: stat(%s) -> size %lld, mode %x\n", path, (long long)buf->st_size, (unsigned int)buf->st_mode);
        return 0;
    }
    _in_stub = 0;
    _log("STUB: stat(%s) -> FAIL %x\n", path, (unsigned int)res);
    return -1;
}

int lstat(const char *path, struct stat *buf) { return stat(path, buf); }

int open(const char *path, int flags, ...) {
    if (_in_stub) return -1;
    _in_stub = 1;
    s32 fd = -1;
    u32 mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (u32)va_arg(args, int);
        va_end(args);
    }
    /* Map POSIX flags to PS3 native flags */
    s32 native_flags = translate_open_flags(flags);

    s32 res = sysLv2FsOpen(path, native_flags, &fd, mode, NULL, 0);
    _in_stub = 0;
    _log("STUB: open(%s, %x -> native %x, %o) -> fd %d (res %x)\n", path, flags, (unsigned int)native_flags, mode, (int)fd, (unsigned int)res);
    if (res == 0) return (int)fd;
    errno = (int)(res & 0xFF);
    return -1;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0; // EOF for stdin
    if (_in_stub) return -1;
    _in_stub = 1;
    u64 read_bytes = 0;
    s32 res = sysLv2FsRead((s32)fd, buf, (u64)count, &read_bytes);
    _in_stub = 0;
    if (res != 0) {
        _log("STUB: read(%d, %zu) -> FAIL %x\n", fd, count, (unsigned int)res);
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)read_bytes;
}

ssize_t write(int fd, const void *buf, size_t count) {
    /* Manually redirect FD 1 and 2 to log if they aren't already */
    if ((fd == 1 || fd == 2) && _log_fd >= 0) {
        u64 written = 0;
        sysLv2FsWrite(_log_fd, buf, (u64)count, &written);
        return (ssize_t)written;
    }

    if (_in_stub) return -1;
    _in_stub = 1;
    u64 written = 0;
    s32 res = sysLv2FsWrite((s32)fd, buf, (u64)count, &written);
    _in_stub = 0;
    if (res != 0) {
        errno = (int)(res & 0xFF);
        return -1;
    }
    return (ssize_t)written;
}

off_t lseek(int fd, off_t offset, int whence) {
    if (_in_stub) return -1;
    _in_stub = 1;
    u64 pos = 0;
    s32 res = sysLv2FsLSeek64((s32)fd, (u64)offset, whence, &pos);
    _in_stub = 0;
    if (res == 0) return (off_t)pos;
    _log("STUB: lseek(%d, %lld, %d) -> FAIL %x\n", fd, (long long)offset, whence, (unsigned int)res);
    errno = (int)(res & 0xFF);
    return -1;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    _log("STUB: select(%d)\n", nfds);
    return 0;
}

char *getenv(const char *name) {
    extern char *SDL_getenv(const char *);
    char *res = SDL_getenv(name);
    _log("STUB: getenv(%s) -> %s\n", name, res ? res : "NULL");
    return res;
}

int close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    if (_in_stub) return -1;
    _in_stub = 1;
    s32 res = sysLv2FsClose((s32)fd);
    _in_stub = 0;
    _log("STUB: close(%d) -> %x\n", fd, (unsigned int)res);
    if (res == 0) return 0;
    errno = (int)(res & 0xFF);
    return -1;
}

/* Stubs for popen/pclose if not available in PSL1GHT */
FILE *popen(const char *command, const char *type) { _log("STUB: popen %s\n", command); return NULL; }
int pclose(FILE *stream) { _log("STUB: pclose\n"); return -1; }

/* Stubs for pwd/grp functions if missing */
void *getpwuid(unsigned int uid) { _log("STUB: getpwuid %u\n", uid); return NULL; }
void *getpwnam(const char *name) { _log("STUB: getpwnam %s\n", name); return NULL; }
void *getgrgid(unsigned int gid) { _log("STUB: getgrgid %u\n", gid); return NULL; }
void *getgrnam(const char *name) { _log("STUB: getgrnam %s\n", name); return NULL; }

/* Python stubs */
void PyEval_InitThreads() { _log("STUB: PyEval_InitThreads\n"); }

/* SDL_image stubs for SDL2 compatibility if only SDL1 version is present. */
void* IMG_LoadTexture_RW(void* renderer, void* src, int freesrc) {
    _log("ERROR: IMG_LoadTexture_RW called but not implemented\n");
    return NULL;
}
