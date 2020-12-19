#!/bin/sh
set -e -x

hpvm="$(realpath "$(dirname "${0}")/..")"
if [ -n "${1}" ]; then
	next_command="${*}"
else
	next_command="/bin/bash"
fi

env \
	be_user=yes \
	os=ubuntu \
	os_tag=20.04 \
	mount_cwd=yes \
	interactive=yes \
	sameplace_mounts="${hpvm}" \
	packages="git wget xz-utils cmake make g++ python ocl-icd-opencl-dev software-properties-common ninja-build" \
	command="sh -c '${hpvm}/scripts/hpvm_build.sh && ${next_command}'" \
	"${hpvm}/scripts/docker.sh"
