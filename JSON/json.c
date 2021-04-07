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

char *parse_dquote(char *dquote, char *parse)
{
	for (char prev = '\0', cur = *dquote;
		 !(prev != '\\' && cur == '\"');
		 prev = cur, cur = *dquote)
	{
		*parse++ = *dquote++;
	}

	*parse = '\0';

	return dquote;
}

struct json *jsonify_string(char *string)
{
	struct json *new_json;
	union json_value value;

	new_json = (struct json *) malloc(sizeof(struct json));
	if (new_json == NULL)
		return NULL;

	string = strchr(string, '{');
	if (string == NULL) return NULL;
	else string++;

	if ((string = strchr(string, '\"')) == NULL) return NULL;
	else string++;

	string = parse_dquote(string, new_json->name);
	printf("name: %s\n", new_json->name);

	string = strchr(string, ':');
	if (string == NULL) return NULL;
	else string++;

	char check[1024];
	if (sscanf(string, " %lf", &value.n) == 1) {
		new_json->type = JSON_TYPE_NUMBER;
	} else if (sscanf(string, " %s", check) == 1
		&& (value.b = !strcmp(check, "true")
			       || !strcmp(check, "false")))
	{
		new_json->type = JSON_TYPE_BOOLEAN;
	} else if (sscanf(string, " %c", 

	new_json->value = value;

	return new_json;
}

struct json *link_json(struct json *first, struct json *next)
{
	return (first->next = next, first);
}

