#ifndef VFS_MESSAGE_H
# define VFS_MESSAGE_H

# include <kernel/types.h>
# include <kernel/klist.h>

struct free_msg {
    struct message *msg;

    struct klist list;
};

struct message {
    uint32_t mid;

    size_t size;

    size_t max_size;
};

struct msg_response {
    uint32_t req_id;

    int ret;
};

/* Open related message */
struct open_msg {
    char *path;
};

/* Read/Write related message */
struct rdwr_msg {
    uint32_t index;

    size_t size;

    size_t off;

    void *data;
};

/* Close related message */
struct close_msg {
    uint32_t index;
};

struct message *message_alloc(size_t size);
void message_free(struct message *msg);

#endif /* !VFS_MESSAGE_H */
