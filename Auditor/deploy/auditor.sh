#!/bin/bash

set -ex

# using etcd exclusevily load starting db schema once for cluster

function wait_pg_startup() {
  local DB_USER=${PGUSER:-"postgres"}
  local DB_NAME="auditordb"
  local MAX_RETRIES=30
  local COUNT=0

  echo "Waiting for PostgreSQL at $DB_HOST to start..."

  # Пытаемся подключиться, пока не получим код возврата 0
  # -c 'select 1' выполняет запрос и возвращает 0 при успехе
  until psql --host=haproxy -U "$DB_USER" -d "$DB_NAME" -c 'select 1' > /dev/null 2>&1; do
    COUNT=$((COUNT + 1))
    if [ $COUNT -ge $MAX_RETRIES ]; then
      echo "Error: PostgreSQL is not responding after $MAX_RETRIES attempts."
      exit 1
    fi
    echo "Postgres is unavailable - sleeping... (Attempt $COUNT/$MAX_RETRIES)"
    sleep 2
  done
}

function init_pg_schema() {
  export ETCDCTL_DIAL_TIMEOUT=3s
  export ETCDCTL_API=3
  FLAGS="--endpoints=[etcd1:2379,etcd2:2379,etcd3:2379] --insecure-transport=true"

  LOCK_NAME="install_lock"
  DONE_FLAG="/install_completed"

  if [[ `etcdctl member list ${FLAGS} | wc -l` -lt 1 ]]; then
    echo "There no running etcd cluster, cannot init auditord and postgres scheme"
    exit 1
  fi

  if etcdctl ${FLAGS} get $DONE_FLAG | grep -q "$DONE_FLAG"; then
    return 0
  fi
  etcdctl ${FLAGS} lock $LOCK_NAME -- bash -c "
    if ! etcdctl ${FLAGS} get $DONE_FLAG | grep -q '$DONE_FLAG'; then
      psql -U postgres -d auditordb --host=haproxy -f deploy/migrations/init.sql
      
      if [ \$? -eq 0 ]; then
        etcdctl ${FLAGS} put $DONE_FLAG 'true'
      fi
    fi
  "
  unset ETCDCTL_DIAL_TIMEOUT
  unset ETCDCTL_API
}

wait_pg_startup
init_pg_schema

# default search path for auditor config -- /etc/auditor/
/usr/bin/auditord -config /etc/auditor/config.yaml