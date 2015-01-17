#include "message_stack.h"

#include <stdlib.h>
#include <string.h>

void message_stack_add(message_stack_t *stack, char *line)
{
    // przenosimy dalej
    for (int i = stack->current_lines; i > 0; i--) {
        if (i == stack->max_lines) {
            free(stack->lines[i]);
            continue;
        }

        stack->lines[i] = stack->lines[i - 1];
    }

    stack->lines[0] = line;

    if (stack->current_lines < stack->max_lines) {
        stack->current_lines++;
    }
}

void message_stack_cleanup(message_stack_t *stack)
{
    for (int i = 0; i < stack->current_lines; i++) {
        free(stack->lines[i]);
    }

    free(stack->lines);
    free(stack);
}

message_stack_t *message_stack_new(int max_lines)
{
    message_stack_t *stack = calloc(sizeof(message_stack_t), 1);

    stack->current_lines = 0;
    stack->max_lines = max_lines;
    stack->lines = calloc(sizeof(char*), max_lines);

    return stack;
}
