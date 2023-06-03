#define MAX_COMMAND_LENGTH 256
#define MAX_TOKEN_LENGTH 128
#define MAX_TOKENS 4

void command_execute(char *command, char args[MAX_TOKENS][MAX_TOKEN_LENGTH], u32 args_count) {
    /* TODO: rework with good system */

    if (strcmp(command, "w") == 0) {
        Buffer *target_buffer = command_pane->parent->buffer;

        if (args_count > 0) {
            free(target_buffer->file_path);
            target_buffer->file_path = strdup(args[0]);
        }
        write_buffer_to_file(target_buffer);
    } else if (strcmp(command, "find") == 0) {
        Buffer *target_buffer = command_pane->parent->buffer;

        if (args_count > 0) {
            free(target_buffer->file_path);
            target_buffer->file_path = strdup(args[0]);
        }
        read_file_to_buffer(target_buffer);
    } else if (strcmp(command, "q") == 0) {
        global_running = false;
    }
}

void command_parse_and_run() {
	char command_raw[MAX_COMMAND_LENGTH];
    u32 _ignored = 0;

    u32 command_length = buffer_get_line(current_buffer, command_raw, sizeof(command_raw) - 1, &_ignored);
	command_raw[command_length] = 0;

    if (command_raw[0] != ':') {
        return;
    }

    /* s test.txt */
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    u32 token_start = 1;
    u32 token_count = 0;
    u32 pos = 1;

    /* TODO: handle multiple widespaces better */

    while (pos < command_length) {
        char c = command_raw[pos];

        if (c == ' ') {
            u32 token_length = pos - token_start;
            memcpy(tokens[token_count], command_raw + token_start, token_length);
            tokens[token_count][token_length] = 0;

            token_count++;

            token_start = pos + 1;
        }

        pos++;
    }

    u32 token_length = pos - token_start;
    if (token_length > 0) {
        memcpy(tokens[token_count], command_raw + token_start, token_length);
        tokens[token_count][token_length] = 0;

        token_count++;
    }

    if (token_count > 0) {
        char *cmd = tokens[0];

        command_execute(cmd, tokens + 1, token_count - 1);
    }
}