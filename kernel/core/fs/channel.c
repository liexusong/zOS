/*
 * zOS
 * Copyright (C) 2015 Baptiste Covolato
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with zOS.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file    kernel/core/fs/channel.c
 * \brief   Implementation of function related to channel IPC mechanism
 *
 * \author  Baptiste Covolato
 */

#include <string.h>

#include <kernel/errno.h>

#include <kernel/mem/kmalloc.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/channel.h>

#include <kernel/fs/vfs/message.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static struct klist channels;
static spinlock_t clock;

static inline void channel_lock(void)
{
    spinlock_lock(&clock);
}

static inline void channel_unlock(void)
{
    spinlock_unlock(&clock);
}

int channel_initialize(void)
{
    klist_head_init(&channels);

    spinlock_init(&clock);

    return 0;
}

static struct channel_slave *channel_get_slave(struct channel *channel,
                                               uint16_t id)
{
    int found = 0;
    struct channel_slave *slave;

    klist_for_each_elem(&channel->slaves, slave, list) {
        if (slave->id == id) {
            found = 1;
            break;
        }
    }

    if (!found)
        return NULL;

    return slave;
}

static struct channel *channel_from_name_nolock(const char *name)
{
    struct channel *channel;

    klist_for_each_elem(&channels, channel, list) {
        if (!strncmp(channel->name, name, CHANNEL_NAME_MAXL)) {
            channel_unlock();
            return channel;
        }
    }

    return NULL;
}

struct channel *channel_from_name(const char *name)
{
    struct channel *channel;

    channel_lock();

    channel = channel_from_name_nolock(name);

    channel_unlock();

    return channel;
}

static int channel_read_message(struct wait_queue *queue, struct klist *input,
                                spinlock_t *lock, size_t size, void *buf)
{
    size_t to_read;
    struct channel_message *message;

    do {
        /* Wait for a message to be received. If there is already a message
         * on the first try, the function won't block
         */
        wait_queue_wait(queue, thread_current(), !klist_empty(input));

        spinlock_lock(lock);

        message = klist_first_elem(input, struct channel_message, list);

        if (message)
            klist_del(&message->list);

        spinlock_unlock(lock);
    } while (!message);

    to_read = MIN(message->size - message->off, size);

    memcpy(buf, (char *)(message + 1) + message->off, to_read);

    if (size < message->size) {
        message->off = message->size;

        spinlock_lock(lock);

        klist_add(input, &message->list);

        spinlock_unlock(lock);
    } else {
        kfree(message);
    }

    return to_read;
}

static int channel_write_message(struct wait_queue *queue, struct klist *input,
                                 spinlock_t *lock, uint16_t cid, size_t size,
                                 void *buf)
{
    struct channel_message *message;

    message = kmalloc(sizeof (struct channel_message) + size);
    if (!message)
        return -ENOMEM;

    message->cid = cid;
    message->size = size;
    message->off = 0;

    memcpy(message + 1, buf, size);

    spinlock_lock(lock);

    klist_add_back(input, &message->list);

    spinlock_unlock(lock);

    wait_queue_notify(queue);

    return size;
}

static int __channel_slave_read(struct file *file, struct process *p,
                                struct req_rdwr *req, void *buf)
{
    (void) p;

    struct channel_slave *slave;

    if (file->f_ops != &channel_slave_f_ops)
        return -EBADF;

    slave = file->private;

    return channel_slave_read(slave, buf, req->size);
}

static int __channel_slave_write(struct file *file, struct process *p,
                                 struct req_rdwr *req, void *buf)
{
    (void) p;

    struct channel_slave *slave;

    if (file->f_ops != &channel_slave_f_ops)
        return -EBADF;

    slave = file->private;

    return channel_slave_write(slave, buf, req->size);
}

static int __channel_slave_close(struct file *file, ino_t inode)
{
    (void) inode;

    struct channel_slave *slave;

    if (file->f_ops != &channel_slave_f_ops)
        return -EBADF;

    slave = file->private;

    return channel_slave_close(slave);
}

static int __channel_master_read(struct file *file, struct process *p,
                                 struct req_rdwr *req, void *buf)
{
    (void) p;

    struct channel *channel;

    if (file->f_ops != &channel_master_f_ops)
        return -EBADF;

    channel = file->private;

    return channel_master_read(channel, buf, req->size);
}

static int __channel_master_write(struct file *file, struct process *p,
                                  struct req_rdwr *req, void *buf)
{
    (void) p;

    struct channel *channel;

    if (file->f_ops != &channel_master_f_ops)
        return -EBADF;

    channel = file->private;

    return channel_master_write(channel, buf, req->size);
}

static int __channel_master_close(struct file *file, ino_t inode)
{
    (void) inode;

    struct channel *channel;

    if (file->f_ops != &channel_master_f_ops)
        return -EBADF;

    channel = file->private;

    return channel_master_close(channel);
}

struct file_operation channel_slave_f_ops = {
    .read = __channel_slave_read,
    .write = __channel_slave_write,
    .close = __channel_slave_close,
};

struct file_operation channel_master_f_ops = {
    .read = __channel_master_read,
    .write = __channel_master_write,
    .close = __channel_master_close,
};

int channel_create(const char *name, struct file *file,
                   struct channel **channel)
{
    struct channel *new_channel;
    struct channel *tmp;

    new_channel = kmalloc(sizeof (struct channel));
    if (!new_channel)
        return -ENOMEM;

    channel_lock();

    klist_for_each_elem(&channels, tmp, list) {
        if (!strncmp(tmp->name, name, CHANNEL_NAME_MAXL)) {
            channel_unlock();
            kfree(new_channel);
            return -EEXIST;
        }
    }

    klist_add(&channels, &new_channel->list);

    channel_unlock();

    strncpy(new_channel->name, name, CHANNEL_NAME_MAXL);
    new_channel->slave_id = 0;
    new_channel->proc = thread_current()->parent;
    wait_queue_init(&new_channel->wait);
    klist_head_init(&new_channel->slaves);
    klist_head_init(&new_channel->input);
    spinlock_init(&new_channel->lock);

    file->inode = NULL;
    file->private = new_channel;
    file->f_ops = &channel_master_f_ops;
    file->mount = NULL;

    if (channel)
        *channel = new_channel;

    return 0;
}

int channel_master_read(struct channel *channel, void *buf, size_t size)
{
    return channel_read_message(&channel->wait, &channel->input,
                                &channel->lock, size, buf);
}

int channel_master_write(struct channel *channel, void *buf, size_t size)
{
    struct channel_slave *slave;
    struct msg_header *hdr = buf;

    if (size < sizeof (struct msg_header))
        return -EINVAL;

    slave = channel_get_slave(channel, hdr->slave_id);
    if (!slave)
        return -EINVAL;

    return channel_write_message(&slave->wait, &slave->input, &slave->lock,
                                 slave->id, size - sizeof (struct msg_header),
                                 hdr + 1);
}

int channel_master_close(struct channel *channel)
{
    channel_lock();

    klist_del(&channel->list);

    channel_unlock();

    /* XXX: We need to notify slaves if any */

    klist_for_each(&channel->input, data, list) {
        struct channel_message *msg = klist_elem(data, struct channel_message,
                                                 list);

        klist_del(&msg->list);
        kfree(msg);
    }

    kfree(channel);

    return 0;
}

int channel_open(struct channel *channel, struct file *file,
                 struct channel_slave **slave)
{
    struct channel_slave *new_slave;

    new_slave = kmalloc(sizeof (struct channel_slave));
    if (!new_slave)
        return -ENOMEM;

    channel_lock();

    klist_add(&channel->slaves, &new_slave->list);

    new_slave->id = channel->slave_id++;

    channel_unlock();

    new_slave->parent = channel;
    new_slave->proc = thread_current()->parent;
    wait_queue_init(&new_slave->wait);
    klist_head_init(&new_slave->input);
    spinlock_init(&new_slave->lock);

    if (file) {
        file->private = new_slave;
        file->f_ops = &channel_slave_f_ops;
        file->mount = NULL;
    }

    if (slave)
        *slave = new_slave;

    return 0;
}

int channel_open_from_name(const char *name, struct file *file,
                           struct channel_slave **slave)
{
    int ret = -ENOENT;
    struct channel *tmp;
    struct channel_slave *new_slave;

    new_slave = kmalloc(sizeof (struct channel_slave));
    if (!new_slave)
        return -ENOMEM;

    channel_lock();

    tmp = channel_from_name_nolock(name);

    if (!tmp) {
        channel_unlock();
        kfree(new_slave);
        return ret;
    }

    channel_unlock();

    return channel_open(tmp, file, slave);
}

int channel_slave_read(struct channel_slave *slave, void *buf, size_t size)
{
    return channel_read_message(&slave->wait, &slave->input, &slave->lock,
                                size, buf);
}

int channel_slave_write(struct channel_slave *slave, void *buf, size_t size)
{
    struct channel *cparent;

    cparent = slave->parent;

    return channel_write_message(&cparent->wait, &cparent->input,
                                 &cparent->lock, slave->id, size, buf);
}

int channel_slave_close(struct channel_slave *slave)
{
    channel_lock();

    klist_del(&slave->list);

    channel_unlock();

    klist_for_each(&slave->input, data, list) {
        struct channel_message *msg = klist_elem(data, struct channel_message,
                                                 list);

        klist_del(&msg->list);
        kfree(msg);
    }

    kfree(slave);

    return 0;
}
