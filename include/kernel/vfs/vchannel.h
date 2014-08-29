#ifndef VFS_VCHANNEL_H
# define VFS_VCHANNEL_H

# include <kernel/klist.h>

# include <kernel/vfs/message.h>

# include <arch/spinlock.h>

struct msg_list
{
    struct message *msg;

    struct klist list;
};

struct vchannel {
    int cid;

    spinlock_t inbox_lock;
    spinlock_t outbox_lock;

    struct klist inbox;

    struct klist outbox;
};

/*
 * Create a new channel, return the channel is success, NULL otherwise
 */
struct vchannel *channel_create(void);

/*
 * Send a message to the inbox of a channel
 *
 * Return:
 *          0: success
 *          -ENOMEM: Cannot allocate necessary memory
 */
int channel_send_request(struct vchannel *chan, struct message *msg);

/*
 * Receive a message from the inbox of the channel
 *
 * Return:
 *          0: Success
 *          -ENODATA: Message was supposed to be here but was not
 *          -EINVAL: Invalid size to receive the buffer
 */
int channel_recv_request(struct vchannel *chan, char *buf, size_t size);

/*
 * Receive a response to a request
 *
 * Return:
 *          0: Success
 *          -ENODATA: Message was supposed to be here but was not
 */
int channel_recv_response(struct vchannel *chan, uint32_t req_id,
                          struct message **msg);

/*
 * Destroy a channel
 */
void channel_destroy(struct vchannel *chan);

#endif /* !VFS_VCHANNEL_H */
