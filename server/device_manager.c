#include "device_manager.h"

struct device {
	int sock;
	int id;
} static dlist[MAX_DEVICE];

static struct device *cur = dlist;

int register_device(int sock, int id)
{
	if (!find_device(id))
		return -1;

	if (cur - dlist >= MAX_DEVICE)
		return -2;

	cur->sock = sock;
	cur->id = id;

	cur++;

	return 0;
}

int find_device(int id)
{
	for (struct device *dp = dlist;
		 dp < cur; dp++)
		if (dp->id == id)
			return dp->sock;

	return -1;
}

int count_device(void)
{
	return cur - dlist;
}
