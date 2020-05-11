#!/bin/sh
set -e -x
cd "$(dirname "${0}")/.."

dirs="simple simpler hpvm-cava pipeline"

cd test/benchmarks
for dir in ${dirs}
do
	cd "${dir}"
	make clean
	make
	dot -Tpng -O *.dot
	rm *-seq
	cd ..
done
