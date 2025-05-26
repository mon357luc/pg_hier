#!/bin/bash
set -e

for ext_dir in /extensions_src/*; do
    if [ -d "$ext_dir" ]; then
        ext_name=$(basename "$ext_dir")
        echo "Cleaning, Building, and installing extension: $ext_name"
        cd "$ext_dir" && make clean && make && make install
        psql -d "$POSTGRES_DB" -U "$POSTGRES_USER" -c "DROP EXTENSION IF EXISTS $ext_name CASCADE;"
        psql -d "$POSTGRES_DB" -U "$POSTGRES_USER" -c "CREATE EXTENSION IF NOT EXISTS $ext_name;"
        echo "Extension $ext_name installed successfully."
    fi
done