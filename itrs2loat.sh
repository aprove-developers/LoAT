#!/bin/bash

itrsToLoAT="$(dirname $0)/src/itrsToLoAT"

if [ $# -ne 2 ]; then
    echo "Usage: $0 <indir> <outdir>"
    echo "Abstracts from itrs' to files that LoAT can handle"
    exit 1
fi

if [ ! -d "$1" ]; then
    echo "$1 is not a valid directory"
    exit 1
fi

if [ ! -d "$2" ]; then
    echo "$2 is not a valid directory"
    exit 1
fi

# make sure that indir and outdir end with a '/'
indir=${1%/}
outdir=${2%/}

files="$indir/*"

for file in $files
do
    if [ -f "$file" ]; then
        infile="$file"
        outfile="$outdir/$(basename $file .itrs).koat"
        printf "$(basename $infile)"

        # run the program
        eval $itrsToLoAT "$infile" "$outfile" &> /dev/null
        retval=$?

        # check if it was successful
        if [ $retval -ne 0 ]; then
            printf " [Error]"
        else
            printf " [Success]"
        fi
    printf "\n"

    fi

done