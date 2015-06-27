#include <string.h>

#include <kernel/errno.h>

#include <kernel/proc/thread.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/vfs/vops.h>
#include <kernel/fs/vfs/message.h>

int vfs_mknod(struct thread *t, const char *path, mode_t mode, uint16_t dev)
{
    int ret;
    int path_size = strlen(path);
    struct resp_lookup res;
    struct mount_entry *mount_pt;
    uid_t uid;
    gid_t gid;

    /* Kernel request */
    if (!t)
    {
        uid = 0;
        gid = 0;
    }
    else
    {
        uid = t->uid;
        gid = t->gid;
    }

    if ((ret = vfs_lookup(t, path, &res, &mount_pt)) < 0)
        return ret;

    if (!mount_pt->ops->mknod)
        return -ENOSYS;

    if (ret == path_size)
        return -EEXIST;

    return mount_pt->ops->mknod(mount_pt, path + ret, res.inode, uid, gid,
                                mode, dev);
}