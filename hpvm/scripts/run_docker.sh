#!/bin/sh
set -e

hpvm="$(realpath "$(dirname "${0}")/..")"
default_cmd="/bin/bash"

if [ "${1}" = "--help" ]; then
	cat <<EOF
Usage: ${0} [--skip-build] cmd...

Run cmd in a docker container containing:
- built HPVM at ${hpvm}/build
- built HPVM binaries (clang, opt, etc.) on the \$PATH
- the current working-directory at $(pwd).

If no cmd is passed, cmd is set to '${default_cmd}'.
EOF
	exit 1
fi

if [ "${1}" = "--skip-build" -o "${1}" = "-s" ]; then
	command=""
	shift
else
	command="${hpvm}/scripts/hpvm_build.sh && "
fi

command="${command}export PATH=${hpvm}/build/bin:\${PATH} && "

if [ -n "${1}" ]; then
	command="${command}${*}"
else
	command="${command}${default_cmd}"
fi

env \
	be_user=yes \
	os=ubuntu \
	os_tag=20.04 \
	mount_cwd=yes \
	interactive=yes \
	sameplace_mounts="${hpvm}" \
	packages="git wget xz-utils cmake make g++ python ocl-icd-opencl-dev software-properties-common ninja-build" \
	command="sh -c '${command}'" \
	quiet=yes \
	"${hpvm}/scripts/docker.sh"
