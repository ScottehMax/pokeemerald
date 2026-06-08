#!/usr/bin/env bash
set -euo pipefail

if [[ "${POKEEMERALD_RUN_AS_OWNER:-1}" == "1" && "$(id -u)" == "0" ]]; then
    repo_uid="$(stat -c '%u' .)"
    repo_gid="$(stat -c '%g' .)"

    if [[ "${repo_uid}" != "0" ]]; then
        exec gosu "${repo_uid}:${repo_gid}" "$0" "$@"
    fi
fi

if [[ ! -f Makefile ]]; then
    echo "No Makefile found. Mount the pokeemerald-expansion checkout at /workspace." >&2
    exit 64
fi

exec "$@"
