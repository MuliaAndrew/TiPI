#!/bin/bash
set -x

echo "$(id -u) $(id -g)"

readonly CONTAINER_IP=$(hostname --ip-address)
readonly CONTAINER_API_ADDR="${CONTAINER_IP}:8008"
readonly CONTAINER_POSTGRE_ADDR="${CONTAINER_IP}:5432"

export PATRONI_NAME="${PATRONI_NAME:-$(hostname)}"
export PATRONI_RESTAPI_CONNECT_ADDRESS="$CONTAINER_API_ADDR"
export PATRONI_RESTAPI_LISTEN="$CONTAINER_API_ADDR"
export PATRONI_POSTGRESQL_CONNECT_ADDRESS="$CONTAINER_POSTGRE_ADDR"
# export PATRONI_POSTGRESQL_LISTEN="0.0.0.0.5432"
export PATRONI_approle_PASSWORD="$POSTGRES_APP_ROLE_PASS"
export PATRONI_approle_OPTIONS="${PATRONI_admin_OPTIONS:-createdb, createrole}"

exec /usr/bin/patroni /configs/patroni.yml