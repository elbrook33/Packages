/*
    OverlayFS
    =========

    * Passes on filesystem commands to the OS.
    * Taken from fusexmp, with credits as follows:
    
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    gcc -Wall `pkg-config fuse --cflags --libs` fusexmp.c -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

const char* (*overlay_path_modifier) (const char*);
const char* (*overlay_path_creator)  (const char*);
void        (*overlay_path_freer)    (const char*);

static int overlay_getattr(const char *path, struct stat *stbuf)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = lstat(real_path, stbuf);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_access(const char *path, int mask)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = access(real_path, mask);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_readlink(const char *path, char *buf, size_t size)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = readlink(real_path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    overlay_path_freer(real_path);
    return 0;
}


static int overlay_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    DIR *dp;
    struct dirent *de;

    dp = opendir(real_path);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    overlay_path_freer(real_path);
    return 0;
}

static int overlay_mknod(const char *path, mode_t mode, dev_t rdev)
{
    const char* real_path = overlay_path_creator(path);
    if (real_path == NULL) return -EPERM;

    int res = mknod(real_path, mode, rdev);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_mkdir(const char *path, mode_t mode)
{
    const char* real_path = overlay_path_creator(path);
    if (real_path == NULL) return -EPERM;

    int res = mkdir(real_path, mode);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_unlink(const char *path)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = unlink(real_path);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_rmdir(const char *path)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = rmdir(real_path);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_symlink(const char *from, const char *to)
{
    const char  *real_from = overlay_path_modifier(from),
                *real_to = overlay_path_creator(to);
    if (real_from == NULL) {
      if (real_to != NULL) overlay_path_freer(real_to);
      return -ENOENT;
    }
    if (real_to == NULL) {
      overlay_path_freer(real_from);
      return -EPERM;
    }

    int res = symlink(real_from, real_to);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_from);
    overlay_path_freer(real_to);
    return 0;
}

static int overlay_rename(const char *from, const char *to)
{
    const char  *real_from = overlay_path_modifier(from),
                *real_to = overlay_path_creator(to);
    if (real_from == NULL) {
      if (real_to != NULL) overlay_path_freer(real_to);
      return -ENOENT;
    }
    if (real_to == NULL) {
      overlay_path_freer(real_from);
      return -EPERM;
    }

    int res = rename(real_from, real_to);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_from);
    overlay_path_freer(real_to);
    return 0;
}

static int overlay_link(const char *from, const char *to)
{
    const char  *real_from = overlay_path_modifier(from),
                *real_to = overlay_path_creator(to);
    if (real_from == NULL) {
      if (real_to != NULL) overlay_path_freer(real_to);
      return -ENOENT;
    }
    if (real_to == NULL) {
      overlay_path_freer(real_from);
      return -EPERM;
    }

    int res = link(from, to);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_from);
    overlay_path_freer(real_to);
    return 0;
}

static int overlay_chmod(const char *path, mode_t mode)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = chmod(real_path, mode);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_chown(const char *path, uid_t uid, gid_t gid)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = lchown(real_path, uid, gid);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_truncate(const char *path, off_t size)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = truncate(real_path, size);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_utimens(const char *path, const struct timespec ts[2])
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    struct timeval tv[2];
    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    int res = utimes(real_path, tv);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_open(const char *path, struct fuse_file_info *fi)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = open(real_path, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    overlay_path_freer(real_path);
    return 0;
}

static int overlay_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    (void) fi;
    int fd = open(real_path, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    overlay_path_freer(real_path);
    return res;
}

static int overlay_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    (void) fi;
    int fd = open(real_path, O_WRONLY);
    if (fd == -1)
        return -errno;

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    overlay_path_freer(real_path);
    return res;
}

static int overlay_statfs(const char *path, struct statvfs *stbuf)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = statvfs(real_path, stbuf);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_release(const char *path, struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) fi;
    return 0;
}

static int overlay_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int overlay_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = lsetxattr(real_path, name, value, size, flags);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}

static int overlay_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = lgetxattr(real_path, name, value, size);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return res;
}

static int overlay_listxattr(const char *path, char *list, size_t size)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = llistxattr(real_path, list, size);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return res;
}

static int overlay_removexattr(const char *path, const char *name)
{
    const char* real_path = overlay_path_modifier(path);
    if (real_path == NULL) return -ENOENT;

    int res = lremovexattr(real_path, name);
    if (res == -1)
        return -errno;

    overlay_path_freer(real_path);
    return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations overlay_operations = {
    .getattr	= overlay_getattr,
    .access	  = overlay_access,
    .readlink	= overlay_readlink,
    .readdir	= overlay_readdir,
    .mknod	  = overlay_mknod,
    .mkdir	  = overlay_mkdir,
    .symlink	= overlay_symlink,
    .unlink	  = overlay_unlink,
    .rmdir	  = overlay_rmdir,
    .rename	  = overlay_rename,
    .link	    = overlay_link,
    .chmod	  = overlay_chmod,
    .chown	  = overlay_chown,
    .truncate	= overlay_truncate,
    .utimens	= overlay_utimens,
    .open	    = overlay_open,
    .read	    = overlay_read,
    .write	  = overlay_write,
    .statfs	  = overlay_statfs,
    .release	= overlay_release,
    .fsync	  = overlay_fsync,
#ifdef HAVE_SETXATTR
    .setxattr	= overlay_setxattr,
    .getxattr	= overlay_getxattr,
    .listxattr	  = overlay_listxattr,
    .removexattr  = overlay_removexattr,
#endif
};
