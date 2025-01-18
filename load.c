#include "load.h"
#include "fixed_stack.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sqlite3_stmt *stmt_insert_node;
sqlite3_stmt *stmt_insert_way;
sqlite3_stmt *stmt_insert_relation;
sqlite3_stmt *stmt_insert_changeset;
sqlite3_stmt *stmt_insert_changeset_tag;

enum State
{
	TAG,
	ATTR_NAME,
	ATTR_VAL,
	AFTER_ATTR_VAL,
	IDLE
};

enum OSM_Element_Type
{
	NODE,
	WAY,
	RELATION,
	CHANGESET,
	CHANGESET_TAG,
	NOT
};

struct OSM_Element
{
	long id;
	long version;
	long changeset;
	char *action;
	enum OSM_Element_Type type;
};

struct OSM_Changeset
{
	long id;
	char created_at[21];
	char closed_at[21];
	bool open;
	char user[256];
	long uid;
	double min_lat;
	double max_lat;
	double min_lon;
	double max_lon;
	long comments;
};

int main(const int argc, char **argv)
{
	assert(argc == 3);

	sqlite3 *db;
	sqlite3_open(argv[2], &db);
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	sqlite3_prepare_v2(db, "INSERT INTO nodes VALUES (?,?,?,?);", -1, &stmt_insert_node, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO ways VALUES (?,?,?,?);", -1, &stmt_insert_way, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO relations VALUES (?,?,?,?);", -1, &stmt_insert_relation, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO changesets VALUES (?,?,?,?,?,?,?,?,?,?,?);", -1, &stmt_insert_changeset, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO changeset_tags VALUES (?,?,?);", -1, &stmt_insert_changeset_tag, NULL);

	FILE *file = fopen(argv[1], "r");
	assert(file);

	fseek(file, 0L, SEEK_END);
	const size_t file_size = ftell(file);
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
	char attr_val_k[512] = {0};

	struct OSM_Element elem;
	struct OSM_Changeset changeset;

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
			if (start_tag)
				fstack_push(&tags, tag_name, strlen(tag_name) + 1); // Start-tag '<tag>'

			// Have to check what we're leaving in case of <changeset><tag /></changeset>
			// At </changeset> elem will still be <tag.
			char *top = fstack_top(&tags);
			if (streq(top, "node"))
				elem.type = NODE;
			else if (streq(top, "way"))
				elem.type = WAY;
			else if (streq(top, "relation"))
				elem.type = RELATION;
			else if (streq(top, "changeset"))
				elem.type = CHANGESET;
			else if (streq(top, "tag") && streq(fstack_n(&tags, 1), "changeset"))
				elem.type = CHANGESET_TAG;
			else
				elem.type = NOT;

			if (!start_tag) {
				// End-tag '</tag>' (we have had some markup in-between)
				if (elem.type == NODE || elem.type == RELATION || elem.type == WAY)
					sql_insert_elem(&elem, fstack_n(&tags, 1));
				else if (elem.type == CHANGESET)
					sql_insert_changeset(&changeset);
				else if (elem.type == CHANGESET_TAG) // should never happen, these always self-close
					sql_insert_changeset_tag(changeset.id, attr_val_k, attr_val);

				fstack_down(&tags);
			}
			// fstack_print(&tags);
		exit_tag_name:
			tag_name[sub_cursor] = '\0';
			sub_cursor = 0;
			start_tag = true;
			break;
		case ATTR_NAME:
			if (c == '=') {
				// we have the name
				attr_name[sub_cursor] = '\0';

				sub_cursor = 0;
				skip = 1; // Skip opening quote
				state = ATTR_VAL;
				break;
			}
			// Empty-element tags would be supported here but we won't.
			// i.e. all nodes must have >= 1 attribute.
			attr_name[sub_cursor++] = c;
			break;
		case ATTR_VAL:
			if (c == '"') {
				// val and name acquired now
				attr_val[sub_cursor] = '\0';

				if (elem.type == NODE || elem.type == WAY || elem.type == RELATION) {
					elem_attr_add(&elem, attr_name, attr_val);
				} else if (elem.type == CHANGESET) {
					changeset_attr_add(&changeset, attr_name, attr_val);
				} else if (elem.type == CHANGESET_TAG && streq(attr_name, "k")) {
					strcpy(attr_val_k, attr_val); // assume `v` is 2nd so leave it in attr_val.
				}

				sub_cursor = 0;
				state = AFTER_ATTR_VAL;
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
			if (c == '/') {
				// End-tag with no markup in-between
				if (elem.type == NODE || elem.type == RELATION || elem.type == WAY)
					sql_insert_elem(&elem, fstack_n(&tags, 1));
				else if (elem.type == CHANGESET)
					sql_insert_changeset(&changeset);
				else if (elem.type == CHANGESET_TAG)
					sql_insert_changeset_tag(changeset.id, attr_val_k, attr_val);

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
	sqlite3_finalize(stmt_insert_way);
	sqlite3_finalize(stmt_insert_relation);
	sqlite3_finalize(stmt_insert_changeset);
	sqlite3_finalize(stmt_insert_changeset_tag);
	sqlite3_close(db);
	return 0;
}

void sql_insert_elem(const struct OSM_Element *elem, char *action)
{
	sqlite3_stmt *stmt;
	if (elem->type == NODE)
		stmt = stmt_insert_node;
	else if (elem->type == WAY)
		stmt = stmt_insert_way;
	else
		stmt = stmt_insert_relation;
	// clang-format off
	sqlite3_bind_int64(stmt, 1, elem->id);
	sqlite3_bind_int64(stmt, 2, elem->version);
	sqlite3_bind_int64(stmt, 3, elem->changeset);
	sqlite3_bind_text(stmt,  4, action, -1, NULL);
	// clang-format on

	const int r = sqlite3_step(stmt);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
}

void sql_insert_changeset(struct OSM_Changeset *cs)
{
	sqlite3_stmt *stmt = stmt_insert_changeset;
	// clang-format off
	sqlite3_bind_int64(stmt,  1,  cs->id);
	sqlite3_bind_text(stmt,   2,  cs->created_at, -1, NULL);
	if (!cs->open)
		sqlite3_bind_text(stmt, 3, cs->closed_at, -1, NULL);
	else
		sqlite3_bind_null(stmt, 3);
	sqlite3_bind_int(stmt,    4,  cs->open);
	sqlite3_bind_text(stmt,   5,  cs->user, -1, NULL);
	sqlite3_bind_int64(stmt,  6,  cs->uid);
	sqlite3_bind_double(stmt, 7,  cs->min_lat);
	sqlite3_bind_double(stmt, 8,  cs->max_lat);
	sqlite3_bind_double(stmt, 9,  cs->min_lon);
	sqlite3_bind_double(stmt, 10, cs->max_lon);
	sqlite3_bind_int64(stmt,  11, cs->comments);
	// clang-format on
	const int r = sqlite3_step(stmt);
	assert(r == SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
}

void sql_insert_changeset_tag(long changeset, char *k, char *v)
{
	sqlite3_stmt *stmt = stmt_insert_changeset_tag;
	// clang-format off
	sqlite3_bind_int64(stmt, 1, changeset);
	sqlite3_bind_text(stmt,  2, k, -1, NULL);
	sqlite3_bind_text(stmt,  3, v, -1, NULL);
	// clang-format on
	const int r = sqlite3_step(stmt);
	assert(r == SQLITE_DONE);
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
}

void elem_attr_add(struct OSM_Element *elem, const char *attr_name, const char *attr_val)
{
	if (streq(attr_name, "id"))
		elem->id = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "version"))
		elem->version = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "changeset"))
		elem->changeset = strtol(attr_val, NULL, 10);
}

void changeset_attr_add(struct OSM_Changeset *cs, const char *attr_name, const char *attr_val)
{
	if (streq(attr_name, "id"))
		cs->id = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "uid"))
		cs->uid = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "comments_count"))
		cs->comments = strtol(attr_val, NULL, 10);
	else if (streq(attr_name, "min_lat"))
		cs->min_lat = strtod(attr_val, NULL);
	else if (streq(attr_name, "max_lat"))
		cs->max_lat = strtod(attr_val, NULL);
	else if (streq(attr_name, "min_lon"))
		cs->min_lon = strtod(attr_val, NULL);
	else if (streq(attr_name, "max_lon"))
		cs->max_lon = strtod(attr_val, NULL);
	else if (streq(attr_name, "created_at"))
		strcpy(cs->created_at, attr_val);
	else if (streq(attr_name, "closed_at"))
		strcpy(cs->closed_at, attr_val);
	else if (streq(attr_name, "user"))
		strcpy(cs->user, attr_val);

	if (streq(attr_name, "open")) {
		if (streq(attr_val, "true")) {
			cs->open = true;
			cs->closed_at[0] = '\0';
		} else {
			cs->open = false;
		}
	}
}

void parse_size(const size_t size, char *buf, const int buf_cap)
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

inline bool is_osm_element(const char *str)
{
	return streq(str, "node") || streq(str, "way") || streq(str, "relation");
}

inline bool streq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}
