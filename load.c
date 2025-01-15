#include "fixed_stack.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum State
{
	TAG,
	ATTR_NAME,
	ATTR_VAL,
	AFTER_ATTR_VAL,
	IDLE
};

#define KB_BYTES 1024
#define MB_BYTES 1048576
#define GB_BYTES 1073741824

void parse_size(size_t size, char *buf, int buf_cap)
{
	if (size <= KB_BYTES)
		snprintf(buf, buf_cap, "%luB", size);
	else if (size <= MB_BYTES)
		snprintf(buf, buf_cap, "%.1fKB", size / (float)KB_BYTES);
	else if (size <= GB_BYTES)
		snprintf(buf, buf_cap, "%.1fMB", size / (float)MB_BYTES);
	else
		snprintf(buf, buf_cap, "%.1fGB", size / (float)GB_BYTES);
}

struct Node
{
	long id;
	long version;
	long changeset;
};

bool streq(char *s1, char *s2)
{
	return strcmp(s1, s2) == 0;
}

void node_attr_add(struct Node *node, char *attr_name, char *attr_val)
{
	if (streq(attr_name, "id"))
		node->id = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "version"))
		node->version = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "changeset"))
		node->changeset = strtol(attr_val, NULL, 10);
}

void sql_insert_node(const struct Node *node, sqlite3_stmt *stmt_node_insert)
{
	sqlite3_bind_int64(stmt_node_insert, 1, node->id);
	sqlite3_bind_int64(stmt_node_insert, 2, node->version);
	sqlite3_bind_int64(stmt_node_insert, 3, node->changeset);

	const int r = sqlite3_step(stmt_node_insert);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt_node_insert);
	sqlite3_clear_bindings(stmt_node_insert);
}

void sql_insert_node_tag(long node_id, long node_version, char *k, char *v, sqlite3_stmt *stmt_node_tag_insert)
{
	sqlite3_bind_int64(stmt_node_tag_insert, 1, node_id);
	sqlite3_bind_int64(stmt_node_tag_insert, 2, node_version);
	sqlite3_bind_text(stmt_node_tag_insert, 3, k, -1, NULL);
	sqlite3_bind_text(stmt_node_tag_insert, 4, v, -1, NULL);

	const int r = sqlite3_step(stmt_node_tag_insert);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt_node_tag_insert);
	sqlite3_clear_bindings(stmt_node_tag_insert);
}

int main()
{
	sqlite3 *db;
	sqlite3_open("changes.sqlite3", &db);
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	sqlite3_stmt *stmt_node_insert;
	sqlite3_prepare_v2(db, "INSERT INTO nodes VALUES (?,?,?);", -1, &stmt_node_insert, NULL);
	sqlite3_stmt *stmt_node_tag_insert;
	sqlite3_prepare_v2(db, "INSERT INTO node_tags VALUES (?,?,?,?)", -1, &stmt_node_tag_insert, NULL);

	FILE *file = fopen("4490.osc", "r");
	assert(file);

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char *buf = malloc(file_size);

	char size_strbuf[256];
	parse_size(file_size, size_strbuf, 256);
	printf("Loading %s of data...\n", size_strbuf);

	fread(buf, 1, file_size, file);

	enum State state = IDLE;
	struct FixedStack tags;
	fstack_init(&tags);

	int sub_cursor = 0;
	int skip = 0;

	char tag_name[32] = {0};
	bool start_tag = false;
	char attr_name[128] = {0};
	char attr_val[512] = {0};

	struct Node node;
	char node_tag_k[128];

	printf("Parsing...\n");
	for (size_t i = 0; i < file_size; i++) {
		if (skip > 0) {
			skip--;
			continue;
		}
		const char c = buf[i];

		switch (state) {
		case IDLE:
			if (c == '<') // <tag> || </tag>
				state = TAG;
			break;
		case TAG:
			switch (c) {
			case ' ': // '<tag '
				state = ATTR_NAME;
				goto found_tag_name;
			case '>': // <tag>
				state = IDLE;
				goto found_tag_name;
			case '?': // <?
				state = IDLE;
				goto exit_tag_name;
			case '/': // </tag>
				start_tag = false;
				break;
			default:
				tag_name[sub_cursor++] = c;
				break;
			}
			break;

		found_tag_name:
			if (start_tag) {
				fstack_push(&tags, tag_name, strlen(tag_name) + 1); // Start-tag '<tag>'
			} else {
				// End-tag '</tag>' (we have had some markup in-between)
				if (streq(tag_name, "node"))
					sql_insert_node(&node, stmt_node_insert);
				fstack_down(&tags);
			}
			// fstack_print(&tags);
		exit_tag_name:
			memset(tag_name, '\0', sizeof(tag_name));
			sub_cursor = 0;
			start_tag = true;
			break;
		case ATTR_NAME:
			if (c == '=') {
				// we have the name
				state = ATTR_VAL;
				skip = 1; // skip the opening quote
				sub_cursor = 0;
				break;
			}
			// Empty-element tags would be supported here but we won't.
			// i.e. all nodes must have >= 1 attribute.
			attr_name[sub_cursor++] = c;
			break;
		case ATTR_VAL:
			if (c == '"') {
				// val and name acquired now
				if (streq(fstack_top(&tags), "node"))
					node_attr_add(&node, attr_name, attr_val);
				else if (streq(fstack_top(&tags), "tag") && streq(fstack_n(&tags, 1), "node")) {
					if (streq(attr_name, "k"))
						strcpy(node_tag_k, attr_val);
					else // "v"
						sql_insert_node_tag(node.id, node.version, node_tag_k, attr_val, stmt_node_tag_insert);
				}
				memset(attr_val, '\0', sizeof(attr_val));
				memset(attr_name, '\0', sizeof(attr_name));
				state = AFTER_ATTR_VAL;
				sub_cursor = 0;
				break;
			}
			attr_val[sub_cursor++] = c;
			break;
		case AFTER_ATTR_VAL:
			if (c == ' ') {
				state = ATTR_NAME;
				break;
			}
			if (c == '"')
				break;
			if (c == '/') { // End-tag with no markup in-between
				if (streq(fstack_top(&tags), "node"))
					sql_insert_node(&node, stmt_node_insert);
				fstack_down(&tags);
			}
			state = IDLE;
			break;
		}
	}
	printf("DONE\n");
	free(buf);
	fclose(file);
	sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
	sqlite3_finalize(stmt_node_insert);
	sqlite3_finalize(stmt_node_tag_insert);
	sqlite3_close(db);
	return 0;
}
