#include <string.h>

#include <kernel/errno.h>
#include <kernel/process.h>
#include <kernel/thread.h>

#include <kernel/vfs/vfs.h>
#include <kernel/vfs/vdevice.h>
#include <kernel/vfs/fs.h>

static int fiu_lookup(struct mount_entry *root, const char *path, uint16_t uid,
                      uint16_t gid, struct resp_lookup *ret)
{
    int res;
    struct vdevice *device;
    struct message *message;
    struct message *response;
    struct req_lookup *request;
    struct resp_lookup *answer;
    struct process *pdevice;

    if (!(device = device_get(root->dev)))
        return -ENODEV;

    if (!(message = message_alloc(sizeof (struct req_lookup))))
        return -ENOMEM;

    request = MESSAGE_EXTRACT(struct req_lookup, message);

    pdevice = process_get(device->pid);

    request->path_size = strlen(path);
    request->path = (void *)as_map(pdevice->as, 0, 0, request->path_size,
                                   AS_MAP_USER | AS_MAP_WRITE);

    if (!request->path)
    {
        message_free(message);

        return -ENOMEM;
    }

    res = as_copy(thread_current()->parent->as, pdevice->as, path,
                  request->path, request->path_size);

    if (res < 0)
    {
        as_unmap(pdevice->as, (vaddr_t)request->path, AS_UNMAP_RELEASE);

        message_free(message);

        return res;
    }

    request->uid = uid;
    request->gid = gid;

    message->mid = (message->mid & ~0xFF) | VFS_OPS_LOOKUP;

    if ((res = channel_send_recv(device->channel, message, &response)) < 0)
    {
        as_unmap(pdevice->as, (vaddr_t)request->path, AS_UNMAP_RELEASE);

        message_free(message);

        return res;
    }

    answer = MESSAGE_EXTRACT(struct resp_lookup, response);

    ret->ret = answer->ret;
    ret->inode = answer->inode;
    ret->processed = answer->processed;
    ret->dev = answer->dev;

    message_free(message);
    message_free(response);
    as_unmap(pdevice->as, (vaddr_t)request->path, AS_UNMAP_RELEASE);

    if (ret->ret < 0)
        return ret->ret;

    return 0;
}

static int fiu_open(struct mount_entry *root, ino_t inode, uint16_t uid,
                    uint16_t gid, int flags, mode_t mode)
{
    return device_open(root->dev, inode, uid, gid, flags, mode);
}

static int fiu_read(struct mount_entry *root, struct req_rdwr *req, void *buf)
{
    return device_read_write(root->dev, req, buf, VFS_OPS_READ);
}

static int fiu_close(struct mount_entry *root, ino_t inode)
{
    return device_close(root->dev, inode);
}

struct fs_ops fiu_ops = {
    .lookup = fiu_lookup,
    .open = fiu_open,
    .read = fiu_read,
    .close = fiu_close,
};
