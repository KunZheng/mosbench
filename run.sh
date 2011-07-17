#!/bin/sh

ROOT=/home/sbw/mosbench

make_config()
{
    (cd $ROOT;
	echo "#" > config.py;
	echo "# XXX DO NOT EDIT (generated from config.tmpl)" >> config.py;
	echo "#" >> config.py;
	cat config.tmpl | sed "s/CONFIG_SPACE/$1.merge(shared)/" >> config.py;
    )
}

case "$1" in
    exim)
	make_config "exim"
	(cd $ROOT && ./mkmounts tmpfs-separate 0 0)
	(cd $ROOT && make bench)
	;;

    metis)
	make_config "metis"
	(cd $ROOT && ./mkmounts hugetlb 0 0)
	(cd $ROOT && make bench)
	;;

    gmake)
	make_config "gmake"
	(cd $ROOT && ./mkmounts tmpfs-separate 0 0)
	(cd $ROOT && make bench)
	;;

    psearchy)
	make_config "psearchy"
	(cd $ROOT && ./mkmounts tmpfs-separate 0 0)
	(cd $ROOT && make bench)
	;;

    *)
	echo "ERROR: unknown $1"
	exit 2
	;;
esac
