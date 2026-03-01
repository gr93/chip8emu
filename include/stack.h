#ifndef STACK_H
#define STACK_H

#define STACK_SIZE 16

struct stack {
    unsigned short data[STACK_SIZE];
    int sp;  // stack pointer (-1 means empty)
};

void stack_init(struct stack *s);
void stack_push(struct stack *s, unsigned short value);
unsigned short stack_pop(struct stack *s);
int stack_empty(struct stack *s);

#endif
