#!/bin/sh

set -e

list=(
"steampipe-plugin-$1"
)

if [[ ! ${MY_PATH} ]];
then
  MY_PATH="`dirname \"$0\"`"              # relative
  MY_PATH="`( cd \"$MY_PATH\" && pwd )`"  # absolutized and normalized
fi

echo $MY_PATH

mkdir $MY_PATH/go-cache || true
mkdir $MY_PATH/go-mod-cache || true

GOCACHE=$(docker run --memory="10g" --memory-swap="16g" --rm --name sp_fdw_builder postgres_fdw_builder:15 go env GOCACHE)
MODCACHE=$(docker run --memory="10g" --memory-swap="16g" --rm --name sp_fdw_builder postgres_fdw_builder:15 go env GOMODCACHE)

for item in "${list[@]}"; do
  plugin_name=${item##*-}

  echo "Processing plugin: ${plugin_name}"

  # Step 1: Switch to steampipe-postgres-fdw directory
  cd $GITHUB_WORKSPACE || exit 1

  # Step 2: Run Docker commands for Postgres FDW Builder v15
  echo "Building Postgres FDW 15 for plugin: ${plugin_name}"
  docker run --memory="10g" --memory-swap="16g" --rm --name sp_fdw_builder -v "$(pwd)":/tmp/ext postgres_fdw_builder:15 make clean
  docker run --memory="10g" --memory-swap="16g" --rm --name sp_fdw_builder -v "$(pwd)":/tmp/ext postgres_fdw_builder:15 make standalone plugin="${plugin_name}"

  # Step 3: Check if build-Linux directory is created
  if [ ! -d "build-Linux" ] || [ ! -f "build-Linux/postgres_postgres_${plugin_name}.so" ]; then
    echo "Error: build-Linux directory or postgres_postgres_${plugin_name}.so not found."
    exit 1
  fi

  # Step 4: Move and tar for Postgres v15
  echo "Move and tar for Postgres v15 for plugin: ${plugin_name}"
  mv build-Linux postgres_postgres_${plugin_name}.pg15.linux_arm64
  tar -czvf postgres_postgres_${plugin_name}.pg15.linux_arm64.tar.gz postgres_postgres_${plugin_name}.pg15.linux_arm64

  # Step 5: Check if tar.gz file is created for v15
  if [ ! -f "postgres_postgres_${plugin_name}.pg15.linux_arm64.tar.gz" ]; then
    echo "Error: Tar file for Postgres v15 not created."
    exit 1
  fi

  echo "Processing completed for plugin: ${plugin_name}"
done

echo "All plugins processed successfully."
