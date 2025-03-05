#!/bin/bash

for ext_dir in /docker-entrypoint-initdb.d/*; do
    if [ -d "$ext_dir" ]; then
        echo "Building and installing extensions in: $ext_dir"
        cd "$ext_dir" && make && make install
    fi
done