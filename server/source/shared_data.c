#include "shared_data.h"

inline int shared_data_compare(void *s1, void *s2)
{
	struct shared_data *sdp1 = s1,
					   *sdp2 = s2;

	return sdp1->id == sdp2->id;
}

inline int shared_data_hash(void *key)
{
	struct shared_data *sdp = key;

	return sdp->id;
}

