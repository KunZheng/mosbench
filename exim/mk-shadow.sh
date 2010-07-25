#!/bin/sh

if [ $# -ne "2" ]; then
    echo "usage: $0 start-num num"
    exit 1
fi

I=$1
N=$2
L=`expr $I + $N - 1`

for I in `seq $I $L`; do
    # XXX Solaris shadow
    echo "user$I:*LK*:6445::::::"
done
