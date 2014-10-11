#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <zos/print.h>

#include "fs.h"
#include "inode_cache.h"
#include "block.h"

# define EXT2FS_OPEN_TIMEOUT 1000
# define EXT2FS_OPEN_RETRY 50

static int ext2fs_load_group_table(struct ext2fs *ext2)
{
    int ret;
    size_t group_nb;
    size_t inode_nb;
    size_t gt_size;
    size_t offset;

    group_nb = (ext2->sb.total_blocks / ext2->sb.block_per_group) +
               ((ext2->sb.total_blocks % ext2->sb.block_per_group) ? 1 : 0);

    inode_nb = (ext2->sb.total_inodes / ext2->sb.inode_per_group) +
               ((ext2->sb.total_inodes % ext2->sb.inode_per_group) ? 1 : 0);

    gt_size = (group_nb > inode_nb ? group_nb : inode_nb);

    ext2->grp_table = malloc(sizeof (struct ext2_group_descriptor) * gt_size);

    if (!ext2->grp_table)
        return 0;

    offset = ext2->block_size == 1024 ? 2048 : ext2->sb.block_size;

    lseek(ext2->fd, offset, SEEK_SET);

    ret = read(ext2->fd, ext2->grp_table,
               sizeof (struct ext2_group_descriptor) * gt_size);

    if (ret < 0)
        return 0;

    return (size_t)ret == sizeof (struct ext2_group_descriptor) * gt_size;
}

int ext2fs_initialize(struct ext2fs *ext2, const char *disk)
{
    int ret;
    int read_size = 0;
    int timeout = 0;

    /* Wait for the disk driver to be alive */
    while (timeout < EXT2FS_OPEN_TIMEOUT)
    {
        ext2->fd = open(disk, 0, 0);

        if (ext2->fd >= 0)
            break;

        timeout += EXT2FS_OPEN_RETRY;
        usleep(EXT2FS_OPEN_RETRY);
    }

    if (ext2->fd < 0)
        return 0;

    /* Superblock is always 1024 bytes after the beginning */
    lseek(ext2->fd, 1024, SEEK_CUR);

    while (read_size != sizeof (struct ext2_superblock))
    {
        ret = read(ext2->fd, ((char *)&ext2->sb) + read_size,
                   sizeof (struct ext2_superblock) - read_size);

        if (ret < 0)
            break;

        read_size += ret;
    }

    if (ret < 0)
        return 0;

    if (ext2->sb.magic != EXT2_SB_MAGIC)
        return 0;

    ext2->block_size = 1024 << ext2->sb.block_size;

    if (!ext2fs_load_group_table(ext2))
        return 0;

    if (!ext2_icache_initialize(ext2))
        return 0;

    return (fiu_cache_initialize(&ext2->fiu, 64, ext2->block_size,
                                 ext2_block_fetch, ext2_block_flush) == 0);
}

uint32_t inode_find_in_dir(struct ext2fs *ext2, struct ext2_inode *inode,
                           const char *name)
{
    uint32_t offset = 0;
    uint32_t offset_block = 0;
    uint32_t block_num = 0;
    uint32_t res = 0;
    void *block = NULL;
    struct ext2_dirent *dirent;
    char *dirent_name;

    if (!inode_block_data(ext2, inode, 0, &block_num))
        return 0;

    if (!(block = fiu_cache_request(&ext2->fiu, block_num)))
        return 0;

    dirent = block;

    while (offset < inode->lower_size)
    {
        dirent_name = (char *)dirent + sizeof (struct ext2_dirent);

        if (dirent->size + offset_block > ext2->block_size)
        {
            uprint("Dirent overlap between 2 blocks");

            return 0;
        }

        if (!strcmp(dirent_name, name))
        {
            res = dirent->inode;

            break;
        }

        offset_block += dirent->size;
        offset += dirent->size;

        if (offset_block >= ext2->block_size)
        {
            offset_block = offset % ext2->block_size;

            fiu_cache_release(&ext2->fiu, block_num);

            if (!inode_block_data(ext2, inode, offset, &block_num))
                return 0;

            if (!(block = fiu_cache_request(&ext2->fiu, block_num)))
                return 0;
        }

        dirent = (void *)((char *)block + offset_block);
    }

    fiu_cache_release(&ext2->fiu, block_num);

    return res;
}

int ext2fs_lookup(struct fiu_internal *fiu, struct req_lookup *req,
                  struct resp_lookup *response)
{
    struct ext2fs *ext2 = fiu->private;
    struct ext2_inode *inode = ext2_icache_request(ext2, 2);
    uint32_t inode_nb = 2;
    uint32_t tmp;
    char *part;
    char *path_left;

    response->ret = -1;
    response->processed = 0;

    while (*req->path == '/')
    {
        ++req->path;
        ++response->processed;
    }

    part = strtok_r(req->path, "/", &path_left);

    while (1)
    {
        if (!(inode->type_perm & EXT2_TYPE_DIRECTORY))
            break;

        if (!(tmp = inode_find_in_dir(ext2, inode, part)))
        {
            ext2_icache_release(ext2, inode_nb);

            break;
        }

        ext2_icache_release(ext2, inode_nb);

        inode_nb = tmp;

        if (!(inode = ext2_icache_request(ext2, inode_nb)))
            break;

        response->processed += path_left - part;

        if (!(part = strtok_r(NULL, "/", &path_left)))
        {
            ext2_icache_release(ext2, inode_nb);

            break;
        }
    }

    response->inode = inode_nb;
    response->dev = -1;

    if (response->ret == -1)
    {
        if (response->processed == req->path_size)
            return LOOKUP_RES_OK;
        else
            return LOOKUP_RES_KO;
    }

    return response->ret;
}
