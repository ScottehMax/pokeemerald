#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f Makefile ]]; then
    echo "No Makefile found. Mount the pokeemerald checkout at /workspace." >&2
    exit 64
fi

if [[ ! -x tools/agbcc/bin/agbcc || ! -x tools/agbcc/bin/old_agbcc || ! -x tools/agbcc/bin/agbcc_arm ]]; then
    repo_dir="${PWD}"
    echo "Installing agbcc into ${repo_dir}/tools/agbcc"
    (cd /opt/agbcc && ./install.sh "${repo_dir}")
fi

if [[ "${POKEEMERALD_RUN_AS_OWNER:-1}" == "1" && "$(id -u)" == "0" ]]; then
    repo_uid="$(stat -c '%u' .)"
    repo_gid="$(stat -c '%g' .)"

    if [[ "${repo_uid}" != "0" ]]; then
        exec gosu "${repo_uid}:${repo_gid}" "$@"
    fi
fi

exec "$@"
