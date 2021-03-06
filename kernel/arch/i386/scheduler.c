#include <kernel/panic.h>
#include <kernel/cpu.h>

#include <arch/scheduler.h>
#include <arch/pm.h>

void i386_switch(struct irq_regs *regs, struct thread *new,
                 struct thread *old, spinlock_t *sched_lock)
{
    struct process *parent = new->parent;
    struct cpu *cpu = cpu_get(cpu_id_get());

    thread_save_state(old, regs);

    /* We manually unlock the mutex to avoid interrupt to occur here */
    spinlock_unlock_no_restore(sched_lock);

    if (new->regs.cs == KERNEL_CS)
        __asm__ __volatile__("mov %0, %%esp\n"
                             :
                             : "r" (new->regs.esp));

    /* Set kernel stack as esp0 */
    cpu->arch.tss.esp0 = new->kstack;

    cr3_set(parent->as->arch.cr3);

    ds_set(new->regs.ds);
    es_set(new->regs.es);
    fs_set(new->regs.fs);
    gs_set(new->regs.gs);

    __asm__ __volatile__("cmpb %5, %1\n"
                         "jz 1f\n"
                         "push %4\n"
                         "push %3\n"
                         "1:\n"
                         "push %0\n"
                         "push %1\n"
                         "push %2\n"
                         "push %6\n"
                         "push %7\n"
                         "push %8\n"
                         "push %9\n"
                         "pushl $0\n"
                         "push %10\n"
                         "push %11\n"
                         "push %12\n"
                         "popal\n"
                         "iret\n"
                         :
                         : "g" (new->regs.eflags),
                           "g" (new->regs.cs),
                           "g" (new->regs.eip),
                           "g" (new->regs.esp),
                           "g" (new->regs.ss),
                           "i" (KERNEL_CS),
                           "g" (new->regs.eax),
                           "g" (new->regs.ecx),
                           "g" (new->regs.edx),
                           "g" (new->regs.ebx),
                           "g" (new->regs.ebp),
                           "g" (new->regs.esi),
                           "g" (new->regs.edi)
                        : "memory");
}
