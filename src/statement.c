#include "statement.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static PrepareResult prepare_insert(InputBuffer *input, Statement *statement) {
    statement->type = STATEMENT_INSERT;

    strtok(input->buffer, " ");
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");
    char *extra = strtok(NULL, " ");
    if (id_string == NULL || username == NULL || email == NULL || extra != NULL) {
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

/* Parses a decimal id token into `out`. */
static PrepareResult parse_id(const char *token, uint32_t *out) {
    char *end = NULL;
    errno = 0;
    long id = strtol(token, &end, 10);
    if (*end != '\0' || errno == ERANGE || id > UINT32_MAX) {
        return PREPARE_SYNTAX_ERROR;
    }
    if (id < 0) {
        return PREPARE_NEGATIVE_ID;
    }
    *out = (uint32_t)id;
    return PREPARE_SUCCESS;
}

static PrepareResult prepare_select(InputBuffer *input, Statement *statement) {
    statement->type = STATEMENT_SELECT;
    statement->select_by_id = 0;
    statement->select_range = 0;
    if (input->buffer[6] == '\0') {
        return PREPARE_SUCCESS;
    }

    strtok(input->buffer, " ");
    char *id_string = strtok(NULL, " ");
    char *high_string = strtok(NULL, " ");
    char *extra = strtok(NULL, " ");
    if (id_string == NULL || extra != NULL) {
        return PREPARE_SYNTAX_ERROR;
    }

    PrepareResult result = parse_id(id_string, &statement->select_id);
    if (result != PREPARE_SUCCESS) {
        return result;
    }
    if (high_string == NULL) {
        statement->select_by_id = 1;
        return PREPARE_SUCCESS;
    }

    result = parse_id(high_string, &statement->select_high);
    if (result != PREPARE_SUCCESS) {
        return result;
    }
    if (statement->select_id > statement->select_high) {
        return PREPARE_SYNTAX_ERROR;
    }
    statement->select_range = 1;
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer *input, Statement *statement) {
    /* Match the whole keyword so e.g. "insertx 1 a b" is not accepted. */
    if (strncmp(input->buffer, "insert", 6) == 0 &&
        (input->buffer[6] == ' ' || input->buffer[6] == '\0')) {
        return prepare_insert(input, statement);
    }
    if (strncmp(input->buffer, "select", 6) == 0 &&
        (input->buffer[6] == ' ' || input->buffer[6] == '\0')) {
        return prepare_select(input, statement);
    }
    if (strncmp(input->buffer, "count", 5) == 0 &&
        (input->buffer[5] == ' ' || input->buffer[5] == '\0')) {
        statement->type = STATEMENT_COUNT;
        strtok(input->buffer, " ");
        return strtok(NULL, " ") == NULL ? PREPARE_SUCCESS : PREPARE_SYNTAX_ERROR;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}
