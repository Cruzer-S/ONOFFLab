#include "json.h"

struct json {
	char name[1024];

	enum json_type type;
	union json_value value;
	struct json *next;
};

struct json* just_json(struct json *json)
{ return json; }

union json_value from_number(double value)
{ return (union json_value) { .n = value }; }

union json_value from_string(char *value)
{ return (union json_value) { .s = value }; }

union json_value from_boolean(bool value)
{ return (union json_value) { .b = value }; }

union json_value from_json(struct json *value)
{ return (union json_value) { .o = value }; }

struct json *make_json(char *name,
		               enum json_type type, union json_value value, struct json *next)
{
	struct json *new_json;

	new_json = malloc(sizeof(struct json));
	if (new_json == NULL)
		return NULL;

	strcpy(new_json->name, name);
	new_json->type = type;
	new_json->next = next;
	new_json->value = value;

	return new_json;
}

void show_json(struct json *root)
{
	static int depth = 0;

	printf("{\n");
	for (struct json *cur = root;
		 cur != NULL;
		 cur = cur->next)
	{
		for (int k = 0; k < depth; k++)
			printf("\t");
		printf("\"%s\": ", cur->name);

		switch ((int) cur->type) {
		case JSON_TYPE_BOOLEAN:
			printf("%s", (cur->value.b) ? "true" : "false");
			break;

		case JSON_TYPE_NUMBER:
			printf("%g", cur->value.n);
			break;

		case JSON_TYPE_STRING:
			printf("\"%s\"", cur->value.s);
			break;

		case JSON_TYPE_OBJECT:
			depth++;
			show_json(cur->value.o);
			depth--;
			break;
		}
		printf((cur->next ? ",\n" : "\n"));
	}

	for (int k = 0; k < depth - 1; k++)
		printf("\t");
	printf("}");

	depth = 0;
}

int stringify_json(struct json *root, char *str)
{
	static int depth = 0;

	((depth == 0) ? strcpy : strcat)(str, "{\n");

	for (struct json *cur = root;
		 cur != NULL;
		 cur = cur->next)
	{
		for (int k = 0; k < depth; k++)
			strcat(str, "\t");
		strcat(str, "\"");
		strcat(str, cur->name);
		strcat(str, "\": ");

		switch ((int) cur->type) {
		case JSON_TYPE_BOOLEAN:
			strcat(str, (cur->value.b) ? "true" : "false");
			break;

		case JSON_TYPE_NUMBER: ;
			char float_number[30];
			sprintf(float_number, "%g", cur->value.n);
			strcat(str, float_number);
			break;

		case JSON_TYPE_STRING:
			strcat(str, "\"");
			strcat(str, cur->value.s);
			strcat(str, "\"");
			break;

		case JSON_TYPE_OBJECT:
			depth++;
			stringify_json(cur->value.o, str);
			depth--;
			break;
		}

		strcat(str, ((cur->next) ? ",\n" : "\n"));
	}

	for (int k = 0; k < depth - 1; k++)
		strcat(str, "\t");
	strcat(str, "}");

	return 0;
}

struct json *jsonify_string(char *str, struct json *root)
{
	char name[1024], string[1024];
	union json_value value;
	enum json_type type;

	str = strchr(str, '{');
	if (str == NULL) return NULL;
	else str++;

	for (struct json *first = root, *next;
		 first != NULL;
		 first = next)
	{
		str = strchr(str, '\"');
		if (str == NULL) break;
		else str++;

		if (sscanf(str, "%[\"]", name) != 1)
			return NULL;

		if ((str = strchr(str, '\"')) == NULL) return NULL;
		else if ((str = strchr(str, ':')) == NULL) return NULL;
		else str++;

		type = JSON_TYPE_NUMBER;
		if (sscanf(str, " %lf", &value.n) != 1) {
			if (sscanf(str, " %s", value.s) != 1)
				return NULL;

			if (! (value.b = strcmp(string, "true")) || !strcmp(string, "false")) {
				type = JSON_TYPE_BOOLEAN;
			} else if ((str = strchr(string, '{')) != NULL) {
				type = JSON_TYPE_OBJECT;
			} else if (strchr(string, '\"') != NULL) {
				type = JSON_TYPE_STRING;
			} else return NULL;
		}

		if (type != JSON_TYPE_OBJECT) {
			next = make_json(name, type, value, NULL);
		}

		link_json(first, next);
	}

	return root;
}

struct json *link_json(struct json *first, struct json *next)
{
	return (first->next = next, first);
}
