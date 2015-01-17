#ifndef MESSAGE_STACK_H
#define MESSAGE_STACK_H

typedef struct message_stack {
    int max_lines;
    int current_lines;
    char **lines;
} message_stack_t;

void message_stack_add(message_stack_t *stack, char *line);
void message_stack_cleanup(message_stack_t *stack);
message_stack_t *message_stack_new(int max_lines);

#endif // MESSAGE_STACK_H

