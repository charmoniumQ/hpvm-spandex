#!/bin/sh
set -e

###############################################################################
# About
###############################################################################

# This script automates many common tasks I need to do using docker,
# such as emulating the current user in the container. Some of these
# tasks require changes at build-time, run-time, or both, so this
# script controls building and running docker images.

# See "Inputs" for the tasks that this script can automate.

# Inputs can be supplied from the shell like so,
#
#     $ forward_x11=yes /path/to/docker.sh
#
# Or in the case where there is no shell,
#
#     $ env forward_x11=yes /path/to/docker.sh

# By default, the docker container has a user with the same UID, GID,
# username, and groupname as the host-user. This is useful so that the
# container-user and host-user have the same permission-level inside
# shared volumes.

###############################################################################
# Inputs
###############################################################################

# Name of the output image
image_out="${image:-$(basename $(pwd) | tr A-Z a-z)}"

# Base-image of the docker image
os="${os:-ubuntu}"
os_tag="${os_tag:-latest}"

# os_type in {debian, redhat}
# This determines the initial setup phase
os_type="${os_type:-debian}"

# Forward X11 outside of the docker container
forward_x11="${forward_x11:-no}"

# Uses --interactive --terminal
interactive="${interactive:-yes}"

# Emulate the current user in the Docker container.
# This adds a user with the same group, UID, and GID as the current user.
# It switches to them for the rest of the dockerfile build and execution.
# This makes files written to host-mounted volumes from inside the container readable outside the container
be_user="${be_user:-yes}"

# Extra packages to install
# If you are using a Dockerfile, probably put this there instead.
packages="${packages:-}"

# Use the current-working directory as the context
# If you don't use COPY or ADD, probably say "no"
context_cwd="${context_cwd:-no}"

# Mount the current-working directory of the host in the container
# and move to that directory.
mount_cwd="${mount_cwd:-yes}"

# Space-separated list of dirs
# Mounts these dirs in the same path in the container
sameplace_mounts="${sameplace_mounts:-}"

# Space-separated list of colon-separated pairs
mounts="${mounts:-}"

# Workdir
workdir="${workdir:-}"

# The dockerfile to apply after the generated dockerfile
# Defaults to "Dockerfile" if that file is present else no further dockerfile is applied.
dockerfile_in="${dockerfile:-}"

# The dockerfile commands to apply after everythign else
# Defaults to nothing
dockerfile_post_cmds="${dockerfile_post_cmds:-}"

# Output for the resulting dockerfile
# No output is generated if this is unset
dockerfile_out="${dockerfile_out:-$(mktemp)}"

# Add any other flags here
docker_run_args="${docker_run_args:-}"

# Command to run after upping, if any
command="${command:-/bin/bash}"

# The actual value of this variable is arbitrary.
# However, changes in this value can be used to invalidate the cache.
touch="${touch:-}"

# Silence the docker build (not default)
quiet="${quiet:-}"

###############################################################################
# Building
###############################################################################

if [ "${os_type}" = "debian" ]
then
	cat <<EOF > "${dockerfile_out}"
FROM ${os}:${os_tag}

RUN echo ${touch}

# I am putting the packages necessary for adding other packages here.
# Without these, other packages could not be immediately installed by the user.
# sudo is necessary once I de-escalate priveleges.
# curl and gnupg2 is necessary for adding apt keys.
# apt-transport-https and ca-certificates for HTTPS apt sources.
# nano is necessary to debug
ENV DEBIAN_FRONTEND=noninteractive TZ=America/Chicago  LANG=en_US.utf8
RUN \
    apt-get update && \
    apt-get upgrade -y && \
    apt-get autoremove -y && \
    apt-get install -y locales sudo gnupg2 curl apt-transport-https ca-certificates nano tzdata && \
    rm -rf /var/lib/apt/lists/* && \
    localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 && \
    apt-get update && \
true
EOF
else if [ "${os_type}" = "redhat" ]
	 then
		 cat <<EOF > "${dockerfile_out}"
FROM ${os}:${os_tag}
RUN \
    yum update -y && \
    yum install -y sudo curl nano && \
true
EOF
	 else
		 echo "Unrecognized os_type ${os_type}"
		 exit
	 fi
fi

if [ "${be_user}" = "yes" ]
then
	# this is a hack because on Mac, the user is in the staff group
	# and staff already exists, so I append the name 'docker' to the group
	GROUP="$(id -g -n)docker"
	if [ -z "${GID}" ]
	then
		# some shells set GID
		# moreover, these shells don't like it when you set the GID
		GID="$(id -g)"
	fi
	if [ -z "${UID}" ]
	then
		UID="$(id -u)"
	fi
    cat <<EOF >> "${dockerfile_out}"
RUN groupadd --non-unique --gid "${GID}" "${GROUP}" && \
    useradd --non-unique --base-dir /home --gid "${GID}" --create-home --uid "${UID}" -o --shell /bin/bash "${USER}" && \
    echo "${USER} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
true
# In the case where be_user = yes, I de-escalate priveleges here.
# This is so that if you write files in the Dockerfile, they will be accessible to the end-user, who is running non-root.
# Root can still be used in the Dockerfile via sudo.
USER ${USER}:${GROUP}
ENV HOME=/home/${USER}
EOF
fi

if [ -n "${packages}" ]
then
	if [ "${os_type}" = "debian" ]
	then
		installer=apt-get
	else if [ "${os_type} = redhat" ]
		 then
			 installer=yum
		 else
			 echo "Unrecognized os_type ${os_type}"
			 exit
		 fi
	fi
	# Could be be_user=yes or not. In either case, using 'sudo' is safe.
	cat <<EOF >> "${dockerfile_out}"
RUN sudo ${installer} install -y ${packages}
EOF
fi

if [ "${context_cwd}" = "yes" ]
then
	context="$(pwd)"
else
	context="$(mktemp -d)"
fi

# fill in default arg in the case where dockerfile is empty
if [ -z "${dockerfile_in}" ]
then
	if [ -f "Dockerfile" ]
	then
		dockerfile_in="Dockerfile"
	fi
fi

if [ -n "${dockerfile_in}" ]
then
   cat "${dockerfile_in}" >> "${dockerfile_out}"
fi

if [ -n "${dockerfile_post_cmds}" ]
then
	echo "${dockerfile_post_cmds}" >> "${dockerfile_out}"
fi

if [ "${quiet}" = yes ]
then
	docker_build_args="${docker_build_args} --quiet"
fi

docker build ${docker_build_args} --tag="${image_out}" --file="${dockerfile_out}" "${context}"

###############################################################################
# Running
###############################################################################

# You should almost always want these args
docker_run_args="${docker_run_args} --rm --init"

if [ "${be_user}" = "yes" ]; then
	docker_run_args="${docker_run_args} --user=${USER}:${GROUP} --env HOME=${HOME}"
fi

if [ "${forward_x11}" = "yes" ]; then
	# https://stackoverflow.com/a/25280523/1078199
    XSOCK=/tmp/.X11-unix
    XAUTH=/tmp/.docker.xauth
    xauth nlist "${DISPLAY}" | sed -e 's/^..../ffff/' | xauth -f "${XAUTH}" nmerge -
    docker_run_args="${docker_run_args} --network=host --env DISPLAY=${DISPLAY} --env XAUTHORITY=${XAUTH} --volume ${XSOCK}:${XSOCK} --volume ${XAUTH}:${XAUTH}"
fi

if [ "${interactive}" = "yes" ]; then
        docker_run_args="${docker_run_args} --interactive --tty"
fi

if [ "${mount_cwd}" = "yes" ]; then
	wd="$(pwd)"
	# I don't think we need to realpath this.
	# If we do:
	# MacOS does not have `realpath` (gnu coreutils)
	# if realpath "${wd}"
	# then
	# 	wd="$(realpath ${wd})"
	# fi
    docker_run_args="${docker_run_args} --volume ${wd}:${wd}"
	if [ -z "${workdi}" ]
	then
		docker_run_args="${docker_run_args} --workdir ${wd}"
	fi
fi

for sameplace_mount in ${sameplace_mounts}
do
	docker_run_args="${docker_run_args} --volume ${sameplace_mount}:${sameplace_mount}"
done

if [ -n "${workdir}" ]
then
	docker_run_args="${docker_run_args} --workdir ${workdir}"
fi

for mount in ${mounts}
do
	docker_run_args="${docker_run_args} --volume ${mount}"
done

if [ ! -z "${network}" ]
then
	docker_run_args="${docker_run_args} --network=${network}"
fi

if [ ! -z "${dockerfile_out}" ]
then
	echo "# docker run ${docker_run_args} ${image_out} ${command}" >> "${dockerfile_out}"
fi

eval 'docker run ${docker_run_args} ${image_out} '${command}''
