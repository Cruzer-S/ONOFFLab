#include "u_json.h"

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
	struct json *root;

	root = malloc(sizeof(struct json));
	if (root == NULL)
		return NULL;

	strcpy(root->name, name);
	root->type = type;
	root->next = next;
	root->value = value;

	return root;
}

void show_json(struct json *root)
{
	static int depth = 1;

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

		case JSON_TYPE_OBJECT: ;
			int prev = depth;
			depth++;
			show_json(cur->value.o);
			depth = prev;
			break;
		}
		printf((cur->next ? ",\n" : "\n"));
	}

	for (int k = 0; k < depth - 1; k++)
		printf("\t");
	printf("}");

	depth = 1;
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

char *skip_whitespace(char *str)
{
	while (isspace(*str)) str++;

	return str;
}

struct json *jsonify_string(char *string)
{
	static char *str;
	struct json *root;
	union json_value value;

	if (string != NULL) str = string;

	root = (struct json *) malloc(sizeof(struct json));
	if (root == NULL) return NULL;
	else root->next = NULL;

	str = strchr(str, '{');
	if (str == NULL) {
		free(root);
		return NULL;
	} else str++;

	for (struct json *cur = root, *next = NULL;
		 cur != NULL;
		 cur->next = next, cur = next)
	{
		if ((str = strchr(str, '\"')) == NULL) {
			free(cur);
			return NULL;
		} else str++;

		str = parse_dquote(str, cur->name);
		printf("name: %s\n", cur->name);

		str = strchr(str, ':');
		if (str == NULL) {
			free(cur);
			return NULL;
		} else str++;

		str = skip_whitespace(str);

		 if (*str == '{') {
			cur->type = JSON_TYPE_OBJECT;
			if ((value.o = jsonify_string(NULL)) == NULL) {
				free(cur);
				return NULL;
			}
		} else if (*str == '\"') {
			char temp[1024];

			cur->type = JSON_TYPE_STRING;
			str = parse_dquote(temp, str);

			cur->value.s = strdup(temp);
		} else if (!strncmp(str, "true", 4)) {
			cur->value.b = true;
			cur->type = JSON_TYPE_BOOLEAN;
			str += 4;
		} else if (!strncmp(str, "false", 5)) {
			cur->value.b = false;
			cur->type = JSON_TYPE_BOOLEAN;
			str += 5;
		} else {
			int n;
			if (sscanf(str, "%lf%n", &value.n, &n) == 1) {
				cur->type = JSON_TYPE_NUMBER;
				str += n;
			} else {
				free(cur);
				return NULL;
			}
		}

		cur->value = value;

		str = skip_whitespace(str);

		if (*str == ',') {
			next = (struct json *) malloc(sizeof(struct json));
			if (next == NULL) {
				free(cur);
				return NULL;
			} else next->next = NULL;
			str++;
		} else next = NULL;
	}

	str = skip_whitespace(str);

	if ((str = strchr(str, '}')) == NULL) {
		free(root);
		return NULL;
	} else str++;

	return root;
}

struct json *link_json(struct json *first, struct json *next)
{
	return (first->next = next, first);
}

