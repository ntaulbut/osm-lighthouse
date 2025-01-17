#ifndef LOAD_H

#include <stdbool.h>
#include <stdio.h>

#define KB_BYTES 1024
#define MB_BYTES 1048576
#define GB_BYTES 1073741824

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

bool streq(char *s1, char *s2);

void parse_size(size_t size, char *buf, int buf_cap);

void elem_attr_add(struct OSM_Element *elem, char *attr_name, char *attr_val);

bool is_osm_element(char *str);

void sql_insert_elem(const struct OSM_Element *elem, char *action);

#endif
