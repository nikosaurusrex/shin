#include "shin.h"

#define MAX_COMMAND_LENGTH 256
#define MAX_TOKEN_LENGTH 128
#define MAX_TOKENS 4

static char command_buffer[MAX_COMMAND_LENGTH];
static u32 command_cursor = 0;

void command_execute(char *command, char args[MAX_TOKENS][MAX_TOKEN_LENGTH], u32 args_count) {
    Pane *active_pane = &pane_pool[active_pane_index];
    Buffer *target_buffer = active_pane->buffer;

    /* TODO: rework with good system */
    
    if (strcmp(command, "w") == 0) {
        if (args_count > 0) {
            free(target_buffer->file_path);
            target_buffer->file_path = strdup(args[0]);
        }
        write_buffer_to_file(target_buffer);
    } else if (strcmp(command, "find") == 0) {
        if (args_count > 0) {
            free(target_buffer->file_path);
            target_buffer->file_path = strdup(args[0]);
        }
        read_file_to_buffer(target_buffer);
    } else if (strcmp(command, "q") == 0) {
        global_running = false;
    } else if (isdigit(command[0])) {
        u32 end = 0;
        while (isdigit(command[end])) {
            end++;
        }

        char number_str[MAX_NUMBER_LENGTH];
        memcpy(number_str, command, end);
        number_str[end] = 0;
        u32 number = atoi(number_str);

        if (number > 0) {
            number -= 1;
        }

        buffer_goto_beginning(target_buffer);
        for (u32 i = 0; i < number; ++i) {
            buffer_goto_next_line(target_buffer);
        }
     }
}

void command_parse_and_run() {
    if (command_buffer[0] != ':') {
        return;
    }

    /* s test.txt */
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    u32 token_start = 1;
    u32 token_count = 0;
    u32 pos = 1;

    /* TODO: handle multiple widespaces better */

    while (pos < command_cursor) {
        char c = command_buffer[pos];

        if (c == ' ') {
            u32 token_length = pos - token_start;
            memcpy(tokens[token_count], command_buffer + token_start, token_length);
            tokens[token_count][token_length] = 0;

            token_count++;

            token_start = pos + 1;
        }

        pos++;
    }

    u32 token_length = pos - token_start;
    if (token_length > 0) {
        memcpy(tokens[token_count], command_buffer + token_start, token_length);
        tokens[token_count][token_length] = 0;

        token_count++;
    }

    if (token_count > 0) {
        char *cmd = tokens[0];

        command_execute(cmd, tokens + 1, token_count - 1);
    }
}

void begin_command() {
    current_buffer->mode = MODE_COMMAND;
    command_buffer[0] = ':';
    command_cursor = 1;
}

void command_confirm() {
	command_parse_and_run();
    command_cursor = 0;

    current_buffer->mode = MODE_NORMAL;
}

void command_exit() {
    command_cursor = 0;

    current_buffer->mode = MODE_NORMAL;
}

void command_insert_char() {
    command_buffer[command_cursor] = last_input_event.ch;
    command_cursor++;
    if (command_cursor >= MAX_COMMAND_LENGTH) {
        command_cursor = MAX_COMMAND_LENGTH - 1;
    }
}

void command_delete() {
    command_cursor--;
    if (command_cursor <= 1) {
        command_cursor = 1;
    }
}

u32 command_get_cursor() {
    return command_cursor;
}

char command_buffer_get(u32 i) {
    return command_buffer[i];
}