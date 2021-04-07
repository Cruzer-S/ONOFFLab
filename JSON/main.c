#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "json.h"

int main(void)
{
	struct json *my_json;
	char json_str[1024];

	my_json =
	MAKE_JSON("print",
		MAKE_JSON("temperature",
			MAKE_JSON("room",    50.0,
			MAKE_JSON("bed",     50.0,
			MAKE_JSON("nozzle", 210.0,
			NULL))),
		MAKE_JSON("speed", 50.0,
		MAKE_JSON("layer", 0.2,
		MAKE_JSON("position",
			MAKE_JSON("x", 50.0,
			MAKE_JSON("y", -0.45,
			MAKE_JSON("z", 50.0,
			NULL))),
		NULL)))),
	NULL);

	// show_json(my_json);

	stringify_json(my_json, json_str);

	printf("%s", json_str);

	/*
	my_json = make_json("print", JSON_TYPE_STRING, (union json_value) { .s = "next" }, NULL);
	my_json =
	make_json("print", JSON_TYPE_OBJECT, (union json_value) { .o =
		make_json("temperature", JSON_TYPE_OBJECT, (union json_value) { .o =
			make_json("room",   JSON_TYPE_NUMBER, (union json_value) { .n =  50.0 },
			make_json("bed",    JSON_TYPE_NUMBER, (union json_value) { .n =  50.0 },
			make_json("nozzle", JSON_TYPE_NUMBER, (union json_value) { .n = 210.0 }, NULL)))
			},
		make_json("speed", JSON_TYPE_NUMBER, (union json_value) { .n = 100.0 },
		make_json("layer", JSON_TYPE_NUMBER, (union json_value) { .n = 0.2 },
		make_json("position", JSON_TYPE_OBJECT, (union json_value) { .o =
			make_json("x", JSON_TYPE_NUMBER, (union json_value) { .n = 50.0  },
			make_json("y", JSON_TYPE_NUMBER, (union json_value) { .n = -0.45 },
			make_json("z", JSON_TYPE_NUMBER, (union json_value) { .n = 50.0  }, NULL)))
			},
		NULL))))
	}, NULL);
	*/

	return 0;
}
