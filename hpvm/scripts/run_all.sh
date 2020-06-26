#!/bin/bash
set -e -x
cd "$(dirname "${0}")/.."
proj_root="${PWD}"
TIMEFORMAT="%R"

test_root=test/benchmarks
test_names="rw sparse_rw ed cava"

# for test_name in ${test_names}
# do
# 	[ -d "${test_root}/${test_name}" ]
# 	make clean -C "${test_root}/${test_name}"
# 	TIMEFORMAT="make ${test_name} %R"
# 	time make TARGET=seq -C "${test_root}/${test_name}" &
# done
# wait

# for test_name in ${test_names}
# do
# 	cd "${test_root}/${test_name}"
# 	TIMEFORMAT="run ${test_name} %R"
# 	time ./exe-seq > output || true &
# 	cd "${proj_root}"
# done
# wait

for test_name in ${test_names}
do
	TIMEFORMAT="process ${test_name} %R"
	time pypy "${proj_root}/scripts/data_processing.py" \
		 --name "${test_name}" \
		 < "${test_root}/${test_name}/output" \
		 > "${test_root}/${test_name}/results.csv" \
		&
done
wait

echo "program,mode,core,store,load,traffic" > results.csv
for test_name in ${test_names}
do
	cat "${test_root}/${test_name}/results.csv" >> "${proj_root}/results.csv"
done

cat "${proj_root}/results.csv"
