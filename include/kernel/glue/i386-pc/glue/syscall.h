#ifndef GLUE_I386_PC_SYSCALL_H
# define GLUE_I386_PC_SYSCALL_H

# include <kernel/syscall.h>

void i386_pc_syscall_initialize(void);

void i386_pc_syscall_convert(struct irq_regs *regs, struct syscall *call);

#endif /* !GLUE_I386_PC_SYSCALL_H */
