#include <string.h>

#include <kernel/errno.h>
#include <kernel/kmalloc.h>
#include <kernel/thread.h>

#include <kernel/vfs/vfs.h>
#include <kernel/vfs/vdevice.h>

#include <arch/spinlock.h>

static struct vdevice devices[VFS_MAX_DEVICE];
static spinlock_t device_lock = SPINLOCK_INIT;

int device_create(int pid, const char __user* name, int ops,
                  struct vdevice **device)
{
    int found = 0;

    if (!(ops & VFS_OPS_OPEN) || !(ops & VFS_OPS_CLOSE))
        return -EINVAL;

    *device = NULL;

    spinlock_lock(&device_lock);

    for (int i = 0; i < VFS_MAX_DEVICE; ++i)
    {
        /* FIXME: User access */
        if (devices[i].active && strcmp(devices[i].name, name) == 0)
            found = 1;
        if (!(*device) && !devices[i].active)
        {
            if (!(devices[i].name = kmalloc(strlen(name) + 1)))
            {
                spinlock_unlock(&device_lock);

                return -ENOMEM;
            }

            if (!(devices[i].channel = channel_create()))
            {
                kfree(devices[i].name);
                spinlock_unlock(&device_lock);

                return -ENOMEM;
            }

            /* FIXME: User access !!! */
            strcpy(devices[i].name, name);

            devices[i].id = i;
            devices[i].active = 1;
            devices[i].pid = pid;
            devices[i].ops = ops;

            *device = &devices[i];
        }
    }

    /* Device already exists */
    if (found && *device)
    {
        (*device)->active = 0;
        kfree((*device)->name);
        channel_destroy((*device)->channel);
        *device = NULL;

        spinlock_unlock(&device_lock);

        return -EEXIST;
    }

    spinlock_unlock(&device_lock);

    if (!(*device))
        return -ENOMEM;

    return (*device)->id;
}

struct vdevice *device_get(int dev)
{
    if (dev < 0 || dev >= VFS_MAX_DEVICE)
        return NULL;

    if (!devices[dev].active)
        return NULL;

    return &devices[dev];
}

int device_recv_request(int dev, char *buf, size_t size)
{
    int pid = thread_current()->parent->pid;

    if (dev < 0 || dev >= VFS_MAX_DEVICE)
        return -EINVAL;

    if (!devices[dev].active)
        return -ENODEV;

    if (devices[dev].pid != pid)
        return -EINVAL;

    return channel_recv_request(devices[dev].channel, buf, size);
}

int device_send_response(int dev, uint32_t req_id, char *buf, size_t size)
{
    int res;
    int pid = thread_current()->parent->pid;
    struct message *message;

    if (dev < 0 || dev >= VFS_MAX_DEVICE)
        return -EINVAL;

    if (!devices[dev].active)
        return -ENODEV;

    if (devices[dev].pid != pid)
        return -EINVAL;

    if (!(message = message_alloc(size)))
        return -ENOMEM;

    memcpy(message + 1, buf, size);

    message->size = size;

    res = channel_send_response(devices[dev].channel, req_id, message);

    if (res < 0)
        message_free(message);

    return res;
}

int device_open(int dev, ino_t inode, uint16_t uid, uint16_t gid, int flags,
                mode_t mode)
{
    int ret = 0;
    struct vdevice *device;
    struct message *message;
    struct message *response;
    struct req_open *request;
    struct resp_open *answer;

    if (!(device = device_get(dev)))
        return -ENODEV;

    if (!(message = message_alloc(sizeof (struct req_open))))
        return -ENOMEM;

    request = MESSAGE_EXTRACT(struct req_open, message);

    request->inode = inode;
    request->uid = uid;
    request->gid = gid;
    request->flags = flags;
    request->mode = mode;

    message->mid = (message->mid & ~0xFF) | VFS_OPS_OPEN;

    if ((ret = channel_send_recv(device->channel, message, &response)) < 0)
    {
        message_free(message);

        return ret;
    }

    message_free(message);

    answer = MESSAGE_EXTRACT(struct resp_open, response);

    if (answer->ret < 0)
    {
        message_free(response);

        return answer->ret;
    }

    ret = answer->inode;

    message_free(response);

    return ret;
}

int device_read(int dev, struct req_rdwr *req, char *buf)
{
    int res;
    struct vdevice *device;
    struct process *pdevice;
    struct message *message;
    struct req_rdwr *request;
    struct message *response;
    struct resp_rdwr *answer;
    struct process *process = thread_current()->parent;

    if (!(device = device_get(dev)))
        return -ENODEV;

    if (!(device->ops & VFS_OPS_READ))
        return -ENOSYS;

    if (!(message = message_alloc(sizeof (struct rdwr_msg))))
        return -ENOMEM;

    request = MESSAGE_EXTRACT(struct req_rdwr, message);

    request->inode = req->inode;
    request->size = req->size;
    request->off = req->off;

    pdevice = process_get(device->pid);

    request->data = (void *)as_map(pdevice->as, 0, 0, req->size,
                                   AS_MAP_USER | AS_MAP_WRITE);

    if (!request->data)
    {
        message_free(message);

        return -ENOMEM;
    }

    message->mid = (message->mid & ~0xFF) | VFS_OPS_READ;

    if ((res = channel_send_recv(device->channel, message, &response)) < 0)
        goto end;

    answer = MESSAGE_EXTRACT(struct resp_rdwr, response);

    if (answer->ret < 0)
    {
        res = answer->ret;

        goto end;
    }

    res = as_copy(pdevice->as, process->as, request->data, buf, request->size);

    if (res == 0)
    {
        res = answer->size;

        req->off += answer->size;
    }

end:
    as_unmap(pdevice->as, (vaddr_t)request->data, AS_UNMAP_RELEASE);
    message_free(message);
    message_free(response);

    return res;
}

int device_close(int dev, ino_t inode)
{
    int res;
    struct vdevice *device;
    struct message *message;
    struct message *response;
    struct req_close *request;
    struct resp_close *answer;

    if (!(device = device_get(dev)))
        return -ENODEV;

    if (!(message = message_alloc(sizeof (struct req_close))))
        return -ENOMEM;

    request = MESSAGE_EXTRACT(struct req_close, message);

    request->inode = inode;

    message->mid = (message->mid & ~0xFF) | VFS_OPS_CLOSE;

    if ((res = channel_send_recv(device->channel, message, &response)) < 0)
    {
        message_free(message);

        return res;
    }

    answer = MESSAGE_EXTRACT(struct resp_close, message);

    res = answer->ret;

    message_free(message);
    message_free(response);

    return res;
}

int device_destroy(int pid, int dev)
{
    if (dev < 0 || dev >= VFS_MAX_DEVICE)
        return -EINVAL;

    if (!devices[dev].active)
        return -ENODEV;

    if (devices[dev].pid != pid)
        return -EINVAL;

    spinlock_lock(&device_lock);

    devices[dev].active = 0;

    kfree(devices[dev].name);

    spinlock_unlock(&device_lock);

    return 0;
}
