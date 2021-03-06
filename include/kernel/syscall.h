#ifndef SYSCALL_H
# define SYSCALL_H

# include <kernel/interrupt.h>

# include <arch/cpu.h>

struct syscall
{
    int num;

    /* Pointer on return value */
    int *ret;

    int arg1;
    int arg2;
    int arg3;
    int arg4;
    int arg5;

    struct irq_regs *regs;
};

typedef int (*syscall_callback)(struct syscall *interface);

struct syscall_glue
{
    int (*init)(void);
    int (*convert)(struct irq_regs *, struct syscall *);
};

extern struct syscall_glue syscall_glue_dispatcher;

void syscall_initialize(void);

void syscall_handler(struct irq_regs *regs);

/* Syscalls */

/* Process */
int sys_usleep(struct syscall *interface);
int sys_exit(struct syscall *interface);
int sys_fork(struct syscall *interface);
int sys_getpid(struct syscall *interface);
int sys_waitpid(struct syscall *interface);
int sys_execv(struct syscall *interface);

/* Thread */
int sys_thread_create(struct syscall *interface);
int sys_thread_exit(struct syscall *interface);
int sys_gettid(struct syscall *interface);

/* Interrupt */
int sys_interrupt_register(struct syscall *interface);
int sys_interrupt_listen(struct syscall *interface);
int sys_interrupt_unregister(struct syscall *interface);

/* Memory */
int sys_mmap(struct syscall *interface);
int sys_munmap(struct syscall *interface);
int sys_mmap_physical(struct syscall *interface);

/* Vfs */
int sys_vfs_device_create(struct syscall *interface);

int sys_vfs_open(struct syscall *interface);
int sys_vfs_read(struct syscall *interface);
int sys_vfs_write(struct syscall *interface);
int sys_vfs_close(struct syscall *interface);
int sys_vfs_lseek(struct syscall *interface);

int sys_vfs_mount(struct syscall *interface);

int sys_vfs_stat(struct syscall *interface);
int sys_vfs_fstat(struct syscall *interface);

int sys_vfs_ioctl(struct syscall *interface);

int sys_vfs_dup(struct syscall *interface);
int sys_vfs_dup2(struct syscall *interface);

int sys_vfs_getdirent(struct syscall *interface);

int sys_vfs_device_exists(struct syscall *interface);
int sys_vfs_open_device(struct syscall *interface);

/* Fs - channel */
int sys_fs_channel_create(struct syscall *interface);
int sys_fs_channel_open(struct syscall *interface);

/* Fs */
int sys_fs_register(struct syscall *interface);
int sys_fs_unregister(struct syscall *interface);

#endif /* !SYSCALL_H */
