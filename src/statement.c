#include "statement.h"

#include <stdlib.h>
#include <string.h>

static PrepareResult prepare_insert(InputBuffer *input, Statement *statement) {
    statement->type = STATEMENT_INSERT;

    strtok(input->buffer, " ");
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");
    if (id_string == NULL || username == NULL || email == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }

    char *end = NULL;
    long id = strtol(id_string, &end, 10);
    if (*end != '\0') {
        return PREPARE_SYNTAX_ERROR;
    }
    if (id < 0) {
        return PREPARE_NEGATIVE_ID;
    }
    if (id > UINT32_MAX) {
        return PREPARE_SYNTAX_ERROR;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE || strlen(email) > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = (uint32_t)id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer *input, Statement *statement) {
    if (strncmp(input->buffer, "insert", 6) == 0) {
        return prepare_insert(input, statement);
    }
    if (strcmp(input->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}
