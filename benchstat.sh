#!/bin/sh

BENCHDIR="./benchscores"

echohelp() {
	echo "Usage: $0 [options]"
	echo -e " -e \t show stats for example benchmarks"
	echo -e " -h \t this help"
}

if [ $# -gt 1 ]; then echohelp; exit 1; fi

#command line arguments
while getopts ":aeh" opt; do
	case $opt in
		e)  BENCHDIR="./benchexample" ;;
		h)  echohelp; exit 0 ;;
		\?) echo "Invalid option: -$OPTARG, see -h"; exit 1 ;;
		:)  echo "Option -$OPTARG needs an argument, see -h"; exit 1 ;;
	esac
done

echo
for a in `ls -1 $BENCHDIR/20*.log | sort`; do
	#Filename
	echo -n "`basename $a`:"

	#Total score
	echo -n " score "
	count=`cat "$a" | grep -E '^/|./' | wc -l`
	score=`cat "${a%.log}.txt" | grep "Total Score" | sed -e 's/[^0-9]*//'`
	echo $score / $count

	#Commit date + message (or aprove)
	commit=`basename $a`
	commit="${commit%\.log}"
	commit="${commit##*_}"
	echo `git show -s --format=%ci $commit` : `git log --format=%B -n 1 $commit | head -n 1`

	#stats
	cat "$a" | cut -f2 | grep -E '^([0-9]*|error|timeout)$' | sort | uniq -c | awk '{ print "\t" $2 "\t" $1 }' | sort -n
	echo
done
