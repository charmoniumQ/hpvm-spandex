#!/bin/sh
set -e -x
cd "$(dirname "${0}")/.."

env \
	be_user=yes \
	os=ubuntu \
	os_tag=20.04 \
	mount_cwd=yes \
	interactive=yes \
	packages="git wget xz-utils cmake make g++ python ocl-icd-opencl-dev software-properties-common" \
	command="${command:-./scripts/hpvm_build.sh}" \
	./scripts/docker.sh
# sudo add-apt-repository ppa:pypy/ppa
# sudo apt update
# sudo apt install pypy pypy-dev libagg-dev libfreetype6-dev
# curl https://bootstrap.pypa.io/get-pip.py | pypy
# pypy -m pip install --user numpy click 'dask[bag]' matplotlib pandas
