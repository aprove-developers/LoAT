#!/bin/bash

itrsToLoAT="$(dirname $0)/src/itrsToLoAT"
xtc2tpdb="../LoAT_stuff/TPDB/xml/xtc2tpdb.xsl"
tmpfile="tmp.trs"
tmpfile2="tmp2.trs"

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

eval "rm -f" "$tmpfile" "$tmpfile2"

# make sure that indir and outdir end with a '/'
indir=${1%/}
outdir=${2%/}

files="$indir/*"

for file in $files
do
    if [ -f "$file" ]; then
        infile="$file"
        outfile="$outdir/$(basename $file .xml).koat"
        printf "$(basename $infile)"

        # xml to .trs
        eval "xsltproc" "$xtc2tpdb" "$infile" > "$tmpfile"

        # add "(GOAL COMPLEXITY)", and remove "(STRATEGY INNERMOST)"
        # substitute function symbol names like *,+,- by strings
        eval "echo \"(GOAL COMPLEXITY)\"" > "$tmpfile2"
        eval "cat $tmpfile \
              | sed 's/#/iwashash/g' \
              | sed 's/+/iwasplus/g' \
              | sed 's/-(/iwasminus(/g' \
              | sed 's/*/iwastimes/g' \
              | sed 's/\//iwasdiv/g' \
              | grep -v \"(STRATEGY INNERMOST)\"" >> "$tmpfile2"

        # run the program
        eval $itrsToLoAT "$tmpfile2" "$outfile" &> /dev/null
        retval=$?

        # check if it was successful
        if [ $retval -ne 0 ]; then
            printf " [Error]"
        else
            printf " [Success]"
        fi

        eval "rm -f" "$tmpfile" "$tmpfile2"
        printf "\n"
    fi

done