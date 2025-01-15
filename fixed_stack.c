#include "fixed_stack.h"
#include <stdio.h>
#include <string.h>

void fstack_init(struct FixedStack *stack)
{
	stack->records = 0;
	stack->top = stack->arena;
}

void fstack_push(struct FixedStack *stack, void *const data, const size_t data_size)
{
	memcpy(stack->top, data, data_size);
	stack->records++;
	stack->top += data_size;
	stack->record_sizes[stack->records - 1] = data_size;
}

/* Get the top of the `stack` *without* popping it. */
void *fstack_top(const struct FixedStack *stack)
{
	const size_t top_size = stack->record_sizes[stack->records - 1];
	return stack->top - top_size;
}

/* Get the item `n_below` items below the top of the `stack`, without popping or moving the top down. */
void *fstack_n(const struct FixedStack *stack, const size_t n_below)
{
	size_t record_n = stack->records;
	const size_t target_n = record_n - n_below;
	void *p = stack->top;
	while (record_n-- >= target_n) {
		p -= stack->record_sizes[record_n];
	}
	return p;
}

/* Assuming it's a stack of strings */
void fstack_print(const struct FixedStack *stack)
{
	printf("Stack(");
	const char *p = stack->arena;
	for (size_t n = 0; n < stack->records; n++) {
		if (n > 0)
			printf(" -> %s", p);
		else
			printf("%s", p);
		p += stack->record_sizes[n];
	}
	printf(")\n");
}

/* Move the top of the `stack` down. */
void fstack_down(struct FixedStack *stack)
{
	const size_t top_size = stack->record_sizes[stack->records - 1];
	stack->top -= top_size;
	stack->records--;
}
