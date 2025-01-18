#ifndef LOAD_H

#include <stdbool.h>
#include <stdio.h>

#define KB_BYTES 1024
#define MB_BYTES 1048576
#define GB_BYTES 1073741824

struct OSM_Element;
struct OSM_Changeset;

bool streq(const char *s1, const char *s2);

void parse_size(size_t size, char *buf, int buf_cap);

void elem_attr_add(struct OSM_Element *elem, const char *attr_name, const char *attr_val);

void sql_insert_changeset(struct OSM_Changeset *changeset);

void sql_insert_changeset_tag(long changeset, char *k, char *v);

void changeset_attr_add(struct OSM_Changeset *cs, const char *attr_name, const char *attr_val);

bool is_osm_element(const char *str);

void sql_insert_elem(const struct OSM_Element *elem, char *action);

#endif
