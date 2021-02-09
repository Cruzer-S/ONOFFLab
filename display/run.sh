#/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: <target>" >&2
	exit 1
fi

if [ ! -f "$(pwd /a.out)" ]; then
	gcc main.c
fi

./a.out $1 8 4 12

if [ $? -eq 0 ]; then
	echo "Convert Successfully. => $1.out"
else
	echo "Failed to convert $1"
fi

rm a.out

exit 0
