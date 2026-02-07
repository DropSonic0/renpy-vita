#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int getuid() { return 0; }
int getgid() { return 0; }
int geteuid() { return 0; }
int getegid() { return 0; }
int getppid() { return 1; }
int pipe(int fildes[2]) { errno = ENOSYS; return -1; }
int fork() { errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { errno = ENOSYS; return -1; }
int symlink(const char *path1, const char *path2) { errno = EROFS; return -1; }
int fdatasync(int fildes) { return 0; }
char *ttyname(int fd) { return NULL; }

/* Stubs for popen/pclose if not available in PSL1GHT */
FILE *popen(const char *command, const char *type) { return NULL; }
int pclose(FILE *stream) { return -1; }

/* Stubs for pwd/grp functions if missing */
void *getpwuid(unsigned int uid) { return NULL; }
void *getpwnam(const char *name) { return NULL; }
void *getgrgid(unsigned int gid) { return NULL; }
void *getgrnam(const char *name) { return NULL; }
