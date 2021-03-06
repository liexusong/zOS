#include <string.h>

#include <kernel/errno.h>

#include <kernel/proc/thread.h>

#include <kernel/mem/kmalloc.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/vfs/vops.h>
#include <kernel/fs/vfs/device.h>
#include <kernel/fs/vfs/message.h>
#include <kernel/fs/vfs/mount.h>

int vfs_open_device(struct thread *t, const char *device_name, int flags,
                    mode_t mode)
{
    int ret;
    int fd;
    struct file *file;
    struct device *device;
    struct process *process;
    uid_t uid;
    gid_t gid;
    dev_t dev_id;

    /* Kernel request */
    if (!t) {
        uid = 0;
        gid = 0;

        process = process_get(0);
    } else {
        uid = t->uid;
        gid = t->gid;

        process = t->parent;
    }

    dev_id = device_get_from_name(device_name);
    if (dev_id < 0)
        return dev_id;

    device = device_get(dev_id);
    if (!device)
        return -ENODEV;

    if (!device->f_ops->open)
        return -ENOSYS;

    fd = process_new_fd(process);
    if (fd < 0)
        return fd;

    file = &process->files[fd];

    file->inode = inode_new(mode);
    if (!file->inode) {
        process_free_fd(process, fd);
        return -ENOMEM;
    }

    file->offset = 0;
    file->mount = NULL;
    file->inode->dev = dev_id;
    file->f_ops = device->f_ops;

    ret = file->f_ops->open(file, -1, process->pid, uid, gid, flags, mode);
    if (ret < 0) {
        inode_del(file->inode);
        process_free_fd(process, fd);
        return ret;
    }

    file->inode->inode = ret;

    return fd;
}
