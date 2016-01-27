#!/bin/bash

if [ $# -eq 0 ]; then
	echo "Usage: $0 <files>"
	echo "Compiles file.dot to file.pdf for each given filename"
	exit 1
fi

while [ $# -ne 0 ]; do
	echo -n "$1: "
	if [ ${1: -4} == ".dot" ]; then
#		dot $1 | gvpack -array1 | dot -Tpdf -o ${1%.dot}.pdf
		ccomps -x $1 | dot | gvpack -array_u3 | neato -Tpdf -n2 -o ${1%.dot}.pdf
	else
		echo "ERROR: Not a dot file, skipped: $1"
	fi
	echo "done"
	shift
done
