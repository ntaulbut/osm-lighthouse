#ifndef FIXED_STACK_H
#define FIXED_STACK_H

#include <stdlib.h>
#define MAX_DEPTH 20

struct FixedStack
{
	char arena[256];
	size_t records;
	char *top;
	size_t record_sizes[MAX_DEPTH];
};

void fstack_init(struct FixedStack *stack);

void fstack_push(struct FixedStack *stack, void *data, size_t data_size);

void *fstack_top(const struct FixedStack *stack);

void *fstack_n(const struct FixedStack *stack, size_t n_below);

void fstack_down(struct FixedStack *stack);

void fstack_print(const struct FixedStack *stack);

#endif
