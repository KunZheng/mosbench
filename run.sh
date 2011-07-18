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
	(cd $ROOT && sudo ./mkmounts tmpfs-separate)
	(cd $ROOT && make bench)
	;;

    metis)
	make_config "metis"
	(cd $ROOT && sudo ./mkmounts hugetlb)
	(cd $ROOT && make bench)
	;;

    gmake)
	make_config "gmake"
	(cd $ROOT && sudo ./mkmounts tmpfs-separate)
	(cd $ROOT && make bench)
	;;

    psearchy)
	make_config "psearchy"
	(cd $ROOT && sudo ./mkmounts tmpfs-separate)
	(cd $ROOT && make bench)
	;;

    postgres)
	make_config "postgres"
	(cd $ROOT && sudo ./mkmounts tmpfs-separate)
	(cd $ROOT && make bench)
	;;

    *)
	echo "ERROR: unknown $1"
	exit 2
	;;
esac
