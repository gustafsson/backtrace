#!/bin/bash

set -e

if [ "$(basename `pwd`)" != "backtrace" ]; then
	echo "Run from the backtrace directory".
	false
fi

(
	make clean
	make -j12
	rm -rf trace_perf/dump
	for a in `seq 100`; do
		./backtrace-unittest
	done

	cd trace_perf
	./make_dump_summary.py
) > /dev/null
