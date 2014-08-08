#ifndef TIMER_H
# define TIMER_H

/* 1 ms */
# define TIMER_GRANULARITY 1

struct timer_glue
{
    void (*init)(void);
};

extern struct timer_glue __timer;

void timer_initialize(void);
void timer_handler(int irq, int data);

#endif /* !TIMER_H */
