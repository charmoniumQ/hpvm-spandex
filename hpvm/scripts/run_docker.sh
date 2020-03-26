#!/bin/sh
set -e -x
cd "$(dirname "${0}")/.."

env \
	be_user=yes \
	mount_cwd=yes \
	interactive=yes \
	packages="git wget xz-utils cmake make g++ python ocl-icd-opencl-dev" \
	command="${command:-./scripts/hpvm_build.sh}" \
	./scripts/docker.sh
