#include <string.h>

#include <kernel/errno.h>
#include <kernel/thread.h>

#include <kernel/vfs/vops.h>
#include <kernel/vfs/fs.h>
#include <kernel/vfs/vdevice.h>

int vfs_read(int fd, void *buf, size_t count)
{
    int ret;
    struct process *p = thread_current()->parent;
    struct req_rdwr request;

    if (fd < 0 || fd > PROCESS_MAX_OPEN_FD || !p->files[fd].used)
        return -EINVAL;

    request.inode = p->files[fd].inode;
    request.size = count;
    request.off = p->files[fd].offset;

    if (p->files[fd].dev >= 0)
        ret = device_read(p->files[fd].dev, &request, buf);
    else
    {
        if (!p->files[fd].mount->ops->read)
            return -ENOSYS;

        ret = p->files[fd].mount->ops->read(p->files[fd].mount, &request, buf);
    }

    if (ret < 0)
        return ret;

    p->files[fd].offset = request.off;

    return ret;
}