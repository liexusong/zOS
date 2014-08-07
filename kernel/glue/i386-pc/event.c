#include <glue/event.h>

#include <kernel/event.h>

#include <arch/cpu.h>

struct event_glue __event =
{
    i386_event_initialize,
    i386_event_enable,
    i386_event_disable,
};

void i386_event_initialize(void)
{
    idt_initialize();
}

void i386_event_enable(void)
{
    cpu_irq_enable();
}

void i386_event_disable(void)
{
    cpu_irq_disable();
}
