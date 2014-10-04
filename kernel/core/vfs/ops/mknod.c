#include <string.h>

#include <kernel/errno.h>

#include <kernel/vfs/vops.h>
#include <kernel/vfs/fs.h>

int vfs_mknod(const char *path, int uid, int gid, mode_t mode, uint16_t dev)
{
    int ret;
    int path_size = strlen(path);
    struct resp_lookup res;
    struct mount_entry *mount_pt;

    if ((ret = vfs_lookup(path, uid, gid, &res, &mount_pt)) < 0)
        return ret;

    if (!mount_pt->ops->mknod)
        return -ENOSYS;

    if (ret == path_size)
        return -EEXIST;

    return mount_pt->ops->mknod(mount_pt, path + ret, res.inode, uid, gid,
                                mode, dev);
}
