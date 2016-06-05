#!/bin/bash

fromdir="../LoAT_stuff/TPDB/Runtime_Complexity_Innermost_Rewriting"
todir="example/recursion/fromtrs"
xml2loat="./xml2loat.sh"

eval "rm -rf $todir"
eval "mkdir $todir"

dirs="$fromdir/*"

for dir in $dirs
do
    if [ -d "$dir" ]; then
        dirname="$(basename "$dir")"
        echo "$dirname"

        eval "mkdir $todir/$dirname"
        eval "$xml2loat $fromdir/$dirname $todir/$dirname"
    fi
done