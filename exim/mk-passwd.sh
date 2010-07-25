#!/bin/sh

if [ $# -ne "3" ]; then
    echo "usage: $0 start-num start-uid num"
    exit 1
fi

I=$1
S=$2
N=$3
L=`expr $S + $N - 1`

for U in `seq $S $L`; do
    # XXX Solaris passwd
    echo "user$I:x:$U:$U::/export/home/exim-users/user$I:/bin/sh"
    I=`expr $I + 1`
done
