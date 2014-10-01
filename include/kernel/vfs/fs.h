#ifndef VFS_FS_H
# define VFS_FS_H

# include <kernel/types.h>

# include <kernel/vfs/mount.h>
# include <kernel/vfs/message.h>

struct fs_ops {
    void *(*init)(void);
    int (*lookup)(struct mount_entry *, const char *, uint16_t, uint16_t,
                  struct resp_lookup *);
    int (*mkdir)(struct mount_entry *, const char *, ino_t, uint16_t, uint16_t,
                 mode_t);
    int (*mount)(struct mount_entry *, ino_t, int);
    int (*open)(struct mount_entry *, ino_t, uint16_t, uint16_t, int, int);
    void (*cleanup)(void *);
};

extern struct fs_ops fiu_ops;

#endif /* !VFS_FS_H */
