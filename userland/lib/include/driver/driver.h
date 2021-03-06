#ifndef DRIVER_DRIVER_H
# define DRIVER_DRIVER_H

# include <zos/vfs.h>

# define DRV_NORESPONSE (-2147483647 - 1)

struct driver {
    char *dev_name;

    int channel_fd;

    dev_t dev_id;

    int running;

    vop_t ops;

    /* Private data that the driver can use */
    void *private;

    struct driver_ops *dev_ops;
};

struct driver_ops {
    int (*open)(struct driver *, struct req_open *, ino_t *);
    int (*read)(struct driver *, struct req_rdwr *, size_t *);
    int (*write)(struct driver *, struct req_rdwr *, size_t *);
    int (*close)(struct driver *, struct req_close *);
    int (*ioctl)(struct driver *, struct req_ioctl *, struct resp_ioctl *);
};

int driver_create(const char *dev_name, int perm, struct driver_ops *dev_ops,
                  struct driver *result);

int driver_loop(struct driver *driver);

#endif /* !DRIVER_DRIVER_H */
