#include <kernel/kmalloc.h>
#include <kernel/zos.h>
#include <kernel/console.h>
#include <kernel/klist.h>

struct kmalloc_blk
{
    void *ptr;
    uint32_t size;
    uint8_t free;

    struct klist list;
};

struct klist kmalloc_head;

struct kmalloc_blk *first_blk;

void kmalloc_initialize(struct boot_info *boot)
{
    klist_head_init(&kmalloc_head);

    first_blk = (void *)boot->heap_start;

    first_blk->ptr = first_blk + 1;
    first_blk->size = boot->heap_size - KSTACK_SIZE -
                      sizeof (struct kmalloc_blk);
    first_blk->free = 1;

    klist_add(&kmalloc_head, &first_blk->list);

    console_message(T_OK, "kmalloc initialisation");
    console_message(T_INF, "kmalloc initial heap: 0x%x-0x%x (%u Ko)",
                    first_blk->ptr, first_blk->ptr + first_blk->size,
                    first_blk->size / 1024);
}

static void *kmalloc_split(struct kmalloc_blk *blk, size_t size)
{
    struct kmalloc_blk *new_blk;

    blk->free = 0;

    /* Split needed */
    if (blk->size + sizeof (struct kmalloc_blk) > size)
    {
        new_blk = ((void *)(blk + 1)) + size;
        new_blk->size = blk->size - size - sizeof (struct kmalloc_blk);
        new_blk->ptr = new_blk + 1;
        new_blk->free = 1;

        blk->size = size;

        klist_add(&blk->list, &new_blk->list);
    }

    return blk->ptr;
}

void *kmalloc(size_t size)
{
    struct kmalloc_blk *blk;

    klist_for_each_elem(&kmalloc_head, blk, list)
    {
        if (blk->size >= size && blk->free)
            return kmalloc_split(blk, size);
    }

    console_message(T_ERR, "kmalloc no more memory");

    return NULL;
}

static void kmerge(struct kmalloc_blk *blk)
{
    if (blk->list.next != &kmalloc_head)
    {
        struct kmalloc_blk *next = klist_elem(blk->list.next,
                                              struct kmalloc_blk, list);

        if (next->free && next == ((void *)(blk + 1)) + blk->size)
        {
            blk->size += next->size + sizeof (struct kmalloc_blk);

            klist_del(&next->list);
        }
    }

    if (blk->list.prev != &kmalloc_head)
    {
        struct kmalloc_blk *prev = klist_elem(blk->list.prev,
                                              struct kmalloc_blk, list);

        if (prev->free && blk == ((void *)(prev + 1)) + prev->size)
        {
            prev->size += blk->size + sizeof (struct kmalloc_blk);

            klist_del(&blk->list);
        }
    }
}

void kfree(void *ptr)
{
    struct kmalloc_blk *blk;

    if (!ptr)
        return;

    blk = ptr;
    --blk;

    if (blk->ptr != ptr)
        console_message(T_ERR, "kfree 0x%x junk ptr", ptr);

    blk->free = 1;

    kmerge(blk);
}

void kmalloc_dump(void)
{
    struct kmalloc_blk *blk;

    console_message(T_INF, "kmalloc dump");

    klist_for_each_elem(&kmalloc_head, blk, list)
    {
        console_message(T_INF, "%s: 0x%x-0x%x (%u o)",
                        blk->free ? "FREE" : "USED",
                        blk->ptr, blk->ptr + blk->size, blk->size);
    }
}
