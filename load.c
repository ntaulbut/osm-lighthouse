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

enum OSM_Element_Type
{
	NODE,
	WAY,
	RELATION
};

struct OSM_Element
{
	long id;
	long version;
	long changeset;
	char *action;
	enum OSM_Element_Type type;
};

bool streq(char *s1, char *s2)
{
	return strcmp(s1, s2) == 0;
}

void elem_attr_add(struct OSM_Element *elem, char *attr_name, char *attr_val)
{
	if (streq(attr_name, "id"))
		elem->id = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "version"))
		elem->version = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "changeset"))
		elem->changeset = strtol(attr_val, NULL, 10);
}

sqlite3_stmt *stmt_insert_node;
sqlite3_stmt *stmt_insert_node_tag;
sqlite3_stmt *stmt_insert_way;
sqlite3_stmt *stmt_insert_way_node;

void sql_insert_node_tag(long node_id, long node_version, char *k, char *v)
{
	sqlite3_bind_int64(stmt_insert_node_tag, 1, node_id);
	sqlite3_bind_int64(stmt_insert_node_tag, 2, node_version);
	sqlite3_bind_text(stmt_insert_node_tag, 3, k, -1, NULL);
	sqlite3_bind_text(stmt_insert_node_tag, 4, v, -1, NULL);

	const int r = sqlite3_step(stmt_insert_node_tag);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt_insert_node_tag);
	sqlite3_clear_bindings(stmt_insert_node_tag);
}

void sql_insert_way_node(long way_id, long way_version, long node_id)
{
	sqlite3_bind_int64(stmt_insert_way_node, 1, way_id);
	sqlite3_bind_int64(stmt_insert_way_node, 2, way_version);
	sqlite3_bind_int64(stmt_insert_way_node, 3, node_id);

	const int r = sqlite3_step(stmt_insert_way_node);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt_insert_way_node);
	sqlite3_clear_bindings(stmt_insert_way_node);
}

void sql_insert_elem(const struct OSM_Element *elem, char *action)
{
	sqlite3_stmt *stmt;
	if (elem->type == NODE)
		stmt = stmt_insert_node;
	else
		stmt = stmt_insert_way;
	sqlite3_bind_int64(stmt, 1, elem->id);
	sqlite3_bind_int64(stmt, 2, elem->version);
	sqlite3_bind_int64(stmt, 3, elem->changeset);
	sqlite3_bind_text(stmt, 4, action, -1, NULL);

	const int r = sqlite3_step(stmt);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
}

int main()
{
	sqlite3 *db;
	sqlite3_open("changes.sqlite3", &db);
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	sqlite3_prepare_v2(db, "INSERT INTO nodes VALUES (?,?,?,?);", -1, &stmt_insert_node, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO node_tags VALUES (?,?,?,?)", -1, &stmt_insert_node_tag, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO ways VALUES (?,?,?,?);", -1, &stmt_insert_way, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO way_nodes VALUES (?,?,?);", -1, &stmt_insert_way_node, NULL);

	FILE *file = fopen("975.osc", "r");
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

	struct OSM_Element elem;
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
				if (streq(tag_name, "node") || streq(tag_name, "way"))
					sql_insert_elem(&elem, fstack_n(&tags, 1));
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
				char *top = fstack_top(&tags);
				char *one_below = fstack_n(&tags, 1);
				if (streq(top, "node")) {
					// node
					elem.type = NODE; // TODO:dumb
					elem_attr_add(&elem, attr_name, attr_val);
				} else if (streq(top, "taaag") && streq(one_below, "node")) {
					// node -> tag
					if (streq(attr_name, "k"))
						strcpy(node_tag_k, attr_val);
					else // "v"
						sql_insert_node_tag(elem.id, elem.version, node_tag_k, attr_val);
				} else if (streq(top, "way")) {
					// way
					elem.type = WAY; // TODO:dumb
					elem_attr_add(&elem, attr_name, attr_val);
				} else if (streq(top, "nd") && streq(one_below, "way")) {
					// way -> nd
					sql_insert_way_node(elem.id, elem.version, strtol(attr_val, NULL, 10));
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
				char *top = fstack_top(&tags);
				if (streq(top, "node") || streq(top, "way"))
					sql_insert_elem(&elem, fstack_n(&tags, 1));
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
	sqlite3_finalize(stmt_insert_node);
	sqlite3_finalize(stmt_insert_node_tag);
	sqlite3_close(db);
	return 0;
}
