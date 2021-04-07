#ifndef JSON_H__
#define JSON_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/***********************************************/
#define MAKE_JSON(NAME, VALUE, NEXT) make_json(	\
/**********************************************/\
		NAME,									\
/**********************************************/\
		_Generic((VALUE),						\
			int          : JSON_TYPE_NUMBER,	\
			double       : JSON_TYPE_NUMBER,	\
			char        *: JSON_TYPE_STRING,	\
			bool         : JSON_TYPE_BOOLEAN,	\
			struct json *: JSON_TYPE_OBJECT		\
		),										\
/**********************************************/\
		_Generic((VALUE),						\
			int			 : from_number,			\
			double		 : from_number,			\
			char		*: from_string,			\
			bool		 : from_boolean,		\
			struct json *: from_json			\
		)(VALUE),								\
/**********************************************/\
		NEXT)
/***********************************************/
enum json_type {
	JSON_TYPE_NUMBER,
	JSON_TYPE_BOOLEAN,
	JSON_TYPE_STRING,
	JSON_TYPE_OBJECT
};

union json_value {
	double n;
	bool b;
	char *s;
	struct json *o;
};

struct json;

struct json* just_json(struct json *json);
union json_value from_number(double value);
union json_value from_string(char *value);
union json_value from_boolean(bool value);
union json_value from_json(struct json *value);

struct json *make_json(char *name,
		               enum json_type type, union json_value value,
					   struct json *next);
struct json *link_json(struct json *first, struct json *next);
void show_json(struct json *root);
int stringify_json(struct json *root, char *str);
struct json *jsonfy_string(char *str, struct json *root);

#endif
