#include <kernel/console.h>

#include <glue/syscall.h>

#include <arch/idt.h>

struct syscall_glue syscall_glue_dispatcher =
{
    i386_pc_syscall_initialize,
    i386_pc_syscall_convert,
};

int i386_pc_syscall_initialize(void)
{
    int err;

    err = interrupt_register(IRQ_SYSCALL, INTERRUPT_CALLBACK, syscall_handler);
    if (err < 0) {
        console_message(T_ERR, "Unable to register syscall interrupt");
        return err;
    }

    return 0;
}

int i386_pc_syscall_convert(struct irq_regs *regs, struct syscall *call)
{
    call->num = regs->eax;

    call->ret = (void *)&regs->eax;

    call->arg1 = regs->ebx;
    call->arg2 = regs->ecx;
    call->arg3 = regs->edx;
    call->arg4 = regs->esi;
    call->arg5 = regs->edi;

    return 1;
}
