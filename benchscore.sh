#!/bin/bash

#default output directory
BENCHDIR="./benchscores"

#default timeout
TIMEOUT=30 #seconds

#temporary files created in BENCHDIR
TMPFILE_NAME="bench.tmp"
ERRFILE_NAME="bench.err"

#original LoAT binary
LOATBIN=./src/loat

#temporary copy of the executable (to allow recompiling during benchmark)
LOATBENCHBIN=./loat_bench

#dump stdout,stderr (for debugging only)
DUMP=1
DUMPDIR="./benchlog"

#quiet mode
QUIET=0

#koat support (to compare benchmark results with koat)
KOAT=0
KOATBIN="koat" #NOTE: change this to point to the koat executable
KOATTOCPX="./src/koatToComplexity"

#helper for pretty output and time measurement
echolog() { echo "$@" >> "$LOGFILE"; }
echotable() { expand -t 10 <(echo -e "$@"); }
gettime() { date '+%s.%N' | sed -e 's/\(.*\.[0-9]\{3\}\).*/\1/'; }
getcommit() { git rev-parse HEAD | sed -e 's/^\(.\{10\}\).*/\1/'; }

exec_tool() {
	if [ $KOAT -eq 1 ]; then
		$(timeout $TIMEOUT koat --timeout $TIMEOUT "$1" > "$TMPFILE" 2> "$ERRFILE")
		return $?
	else
		if [ $DUMP -eq 1 ]; then
			$(timeout $TIMEOUT $LOATBENCHBIN --timeout $TIMEOUT --dot "$DUMPDIR/$dotfile" "$1" > "$TMPFILE" 2> "$ERRFILE")
			return $?
		else
			$(timeout $TIMEOUT $LOATBENCHBIN --timeout $TIMEOUT "$1" > "$TMPFILE" 2> "$ERRFILE")
			return $?
		fi
	fi
}

parse_result() {
	if [ $KOAT -eq 1 ]; then
		runtime=`cat $TMPFILE | grep "Complexity upper bound" | sed -e 's/Complexity upper bound \(.*$\)/\1/' | sed -e 's/?/INF/'`
		if [ "$runtime" != "" ]; then
			$KOATTOCPX --simple "$runtime" 2>/dev/null | grep -E "^Complexity:" | cut -d' ' -f2
		fi
	else
		cat $TMPFILE | grep -E "^\s*Complexity value:" | cut -d':' -f2 | sed -e 's/\s//g'
	fi
}

printscore() {
	totscore=$((totscore+score))
	END=$(gettime)
	DIFF=$(echo "$END - $START" | bc | awk '{printf "%6.3f", $0}')
	avg=$(echo "scale=2; $score/$count" | bc | awk '{printf "%.2f", $0}')
	scoretxt=$(echo "$score" | awk '{printf "%5d", $0}')
	counttxt=$(echo "$count" | awk '{printf "%3d", $0}')
	echo -ne "\e[1A"
	echo -ne "\r"
	echotable "$scoretxt\t$counttxt\t$avg\t$errors err\t$timeouts tout\t$DIFF s\t$olddir" | tee -a $OUTFILE
	echotable "\t\t\t\t\t\t\t" #clear old line
}

initscore() {
	olddir=`dirname $file`
	score=0
	count=0
	errors=0
	timeouts=0
	files=$(find "$olddir" -name '*.koat' | wc -l)
	START=$(gettime)
}

echowip() {
	echo -ne "\r$(basename $olddir):  "
	echo -ne $count | awk '{printf "%3d", $0}'
	echo -ne " / $files    "
	echo -ne "curr: $@"
	echo -ne "      "
}

echohelp() {
	echo "Usage: $0 [options] <sample-directories>"
	echo
	echo "Runs benchmarks on the given colon-separated list of directories containing .koat files"
	echo "Benchmark files (named after current date and commit hash) are written to: $BENCHDIR"
	echo
	echo "Available options:"
	echo -e " -o DIR  \t specify a different output directory (default: $BENCHDIR)"
	echo -e " -t VAL  \t specify the timout in seconds (default: $TIMEOUT, must be >= 10)"
	echo -e " -k      \t benchmark koat instead of loat (needs koat binary)"
	echo -e " -d DIR  \t dump stdout, stderr and dot output to given directory"
	echo -e " -q      \t dont create benchmark files, just interactive output"
	echo -e "         \t (temporary files are still used; can be combined with -d)"
	echo -e " -h      \t this help"
	echo
	echo "To benchmark the LoAT examples: $0 example"
}

#remember old IFS to restore to use find
OLDIFS=$IFS
totscore=0

#default filename includes date and commit id
FILEBASE_NAME="$(date +'%Y-%m-%d')_$(getcommit)"

#command line arguments
while getopts ":kd:qt:o:h" opt; do
	case $opt in
		t)  TIMEOUT=$OPTARG; echo "Changing timeout to $TIMEOUT" ;;
		o)  BENCHDIR="$OPTARG"; echo "Changing output dir to $BENCHDRI" ;;
		d)  DUMP=1; DUMPDIR="$OPTARG"; echo "Dumping program output to $DUMPDIR" ;;
		q)  QUIET=1 ;;
		k)  KOAT=1 ;;
		h)  echohelp; exit 0 ;;
		\?) echo "Invalid option: -$OPTARG, see -h"; exit 1 ;;
		:)  echo "Option -$OPTARG needs an argument, see -h"; exit 1 ;;
	esac
done

#skip options
shift $((OPTIND-1))
if [ $# -eq 0 ]; then
	echo "List of sample directories missing, see -h"
	exit 1
fi
if [ $# -gt 1 ]; then
	echo "Additioanl arguments, see -h"
	exit 1
fi
SAMPLEDIRS="$1"
echo "Benchmarking koat files in the directores: $SAMPLEDIRS"

if [ $DUMP -eq 1 ]; then
	if [ ! -d $DUMPDIR ]; then
		echo "Error: dump dir $DUMPDIR does not exist, aborting"
		exit 1
	fi
fi

if [ $KOAT -eq 1 ]; then
	echo "Benchmarking external KoAT (binary: $KOATBIN)"
	FILEBASE_NAME="$(date +'%Y-%m-%d')_koat"
fi

FILEBASE="$BENCHDIR/$FILEBASE_NAME"
TMPFILE="$BENCHDIR/$TMPFILE_NAME"
ERRFILE="$BENCHDIR/$ERRFILE_NAME"

LOGFILE="$FILEBASE.log"
OUTFILE="$FILEBASE.txt"

if [ $QUIET -eq 1 ]; then
	echo "Quiet mode, no output files generated"
	LOGFILE="/dev/null"
	OUTFILE="/dev/null"
else
	if [ -e $LOGFILE -o -e $OUTFILE ]; then
		echo "Error: file does already exist: $LOGFILE or $OUTFILE"
		echo -n "Overwrite? (y/N): "
		read -n 1 answer
		if [ "$answer" != "y" ]; then
			exit 42
		fi
	fi
fi

echo "Tmpfile: $TMPFILE"
echo "Errfile: $ERRFILE"
echo "Logfile: $LOGFILE"
echo "Output:  $OUTFILE"
echo

echo "===== Complexity Score Benchmark ======" | tee $OUTFILE 2>/dev/null | tee $LOGFILE 2>/dev/null
if [ $? -ne 0 ]; then
	echo "Error: Unable to write to logfile $LOGFILE or $OUTFILE"
	exit 1
fi
echo "Launched: $(date)" >> $LOGFILE

if [ $KOAT -eq 0 ]; then
	if [ -f $LOATBENCHBIN ]; then
		echo "Error: benchmark executable does already exist: $LOATBENCHBIN"
		echo "If no benchmark is running, please delete this file (happens if this script is aborted)"
		exit 1
	fi
	cp $LOATBIN $LOATBENCHBIN
	$LOATBENCHBIN --cfg >> $LOGFILE
fi

echo "test" > $TMPFILE && echo "test" > $ERRFILE
if [ $? -ne 0 ]; then
	echo "Error: Unable to write to temporary files $TMPFILE, $ERRFILE"
	exit 1
fi

echo "Timeout is $TIMEOUT seconds" | tee -a $LOGFILE
echotable "Score\tSamples\tAverage\tErrors\tTimeouts\tTime\tDirectory" | tee -a $OUTFILE
echo

IFS=":"
for d in $SAMPLEDIRS; do
	if [ ! -d "$d" ]; then
		echo "Error: sample directory does not exist: $d"
		exit 1
	fi

	olddir=""
	IFS=$OLDIFS
	for file in `find "$d" -regex '.*\.koat\|.*\.cint' | sort`; do
		if [ "$olddir" != `dirname $file` ]; then
			if [ ! -z "$olddir" ]; then
				printscore
			fi
			initscore
		fi

		if [ $DUMP -eq 1 ]; then
			dumpfile="$(basename $file)"
			dumpfile="${dumpfile%.*}"
			dumpfile="$(basename `dirname $file`)_$dumpfile"
			dotfile="$dumpfile".dot
			dumperr="$dumpfile".err
			dumpfile="$dumpfile".log
		fi

		exec_tool "$file"
		timecode=$?

		if [ $DUMP -eq 1 ]; then
			cp "$TMPFILE" "$DUMPDIR/$dumpfile"
			cp "$ERRFILE" "$DUMPDIR/$dumperr"
		fi

		val=`parse_result`
		count=$((count+1))

		re='^(-?[0-9]+|EXP|INF)$'
		if [[ $val =~ $re ]]; then
			if [ "$val" == "INF" ]; then val=10000; fi
			if [ "$val" == "EXP" ]; then val=1000; fi
			if [ $val -eq -1 ]; then val=1000000; fi #see when complexity fails
			score=$((score+val))
			echolog -e "$file\t$val"
			echowip "$val"
		elif [ $timecode -eq 124 ]; then
			timeouts=$((timeouts+1))
			echolog -e "$file\ttimeout"
			echowip "tout"
		else
			errors=$((errors+1))
			echolog -e "$file\terror"
			echowip "err"
		fi
	done
	printscore
done

echo >> $OUTFILE
echo "Total Score: $totscore" | tee -a $OUTFILE | tee -a $LOGFILE
echo "===== Complexity Score Benchmark ======" | tee -a $OUTFILE | tee -a $LOGFILE

if [ $KOAT -eq 0 ]; then
	rm $LOATBENCHBIN
fi
