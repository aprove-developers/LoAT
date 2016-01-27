#!/bin/bash

dot2pdf="$(dirname $0)/dot2pdf.sh"

if [ $# -ne 1 ]; then
	echo "Usage: $0 <file.dot>"
	echo "Splits each subgraph into separate file fileN.dot"
	exit 1
fi

awk '/^subgraph/ { file = sprintf("splitdot_tmp%d.out", ++i); } /^subgraph/,/^\}$/ { print > file }' $1
file="${1%.dot}"
for a in splitdot_tmp*.out; do
	n=$(echo $a | sed -e 's/splitdot_tmp\([0-9]*\)\.out/\1/')
	(echo "digraph {"; cat $a; echo "}") > "$file$n".dot
	$dot2pdf "$file$n".dot
	rm "$file$n".dot
	rm "$a"
done
