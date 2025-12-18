#!/bin/bash

set -e

DB_NAME="auditordb"
DB_USER="postgres"

echo "Patroni bootstrap: Creating $DB_NAME..."

psql -U "$DB_USER" -d postgres --host=127.0.0.1 -c "CREATE DATABASE $DB_NAME;" || `echo "Database already exists" && exit 1`

echo "Patroni bootstrap: Creating $DB_NAME successfully"