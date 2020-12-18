#!/bin/sh
set -e -x

hpvm="$(realpath "$(dirname "${0}")/..")"

env \
	be_user=yes \
	os=ubuntu \
	os_tag=20.04 \
	mount_cwd=yes \
	interactive=yes \
	sameplace_mounts="${hpvm} ${real_pwd}" \
	packages="git wget xz-utils cmake make g++ python ocl-icd-opencl-dev software-properties-common ninja-build" \
	command="sh -c '${hpvm}/scripts/hpvm_build.sh && ${*}'" \
	"${hpvm}/scripts/docker.sh"
