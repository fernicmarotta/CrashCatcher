#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

struct timer {
    uint32_t start;
    uint32_t interval;
};

void timer_set(struct timer *t, uint32_t interval);
void timer_reset(struct timer *t);
int timer_expired(struct timer *t);

#endif /* TIMER_H */