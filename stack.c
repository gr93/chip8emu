#include "stack.h"
#include <stdio.h>

void stack_init(struct stack *s) {
    s->sp = -1;
}

void stack_push(struct stack *s, unsigned short value) {
    if (s->sp < STACK_SIZE - 1) {
        s->sp++;
        s->data[s->sp] = value;
    } else {
        printf("Stack overflow!\n");
    }
}

unsigned short stack_pop(struct stack *s) {
    if (s->sp >= 0) {
        unsigned short value = s->data[s->sp];
        s->sp--;
        return value;
    } else {
        printf("Stack underflow!\n");
        return 0;
    }
}

int stack_empty(struct stack *s) {
    return s->sp == -1;
}
