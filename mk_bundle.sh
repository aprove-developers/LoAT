#!/bin/bash
ME=`basename "$0"`

function print {
    echo
    echo "$ME: $1"
    echo
}

function check {
    EXIT_CODE=$?
    if [[ $EXIT_CODE -ne 0 ]]; then
        print "$1 failed, exit code $EXIT_CODE"
        exit $EXIT_CODE
    fi
}

BUILD=build-static
BIN=loat-static
BUNDLE=bundle
TEMPLATE=bundle.template
OUT=loat.zip

if [[ ! -d $BUILD ]]; then
   print "configuring build..."
   mkdir $BUILD
   cd $BUILD
   cmake -DSTATIC=1 ..
   check "configuration"
else
    cd $BUILD
fi

print "building LoAT..."
make -j
check "build"
cd ..

if [[ -d $BUNDLE ]]; then
   rm -r $BUNDLE
fi

print "creating bundle..."
cp -r $TEMPLATE $BUNDLE
cp $BUILD/$BIN $BUNDLE/bin/
zip -r $OUT $BUNDLE
check "creating bundle"

print "bundle created successfully"

