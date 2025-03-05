# Use the official Postgres image as the base
FROM postgres:latest

# Install gcc, build-essential, and postgresql-server-dev-all
RUN apt-get update && \
    apt-get install -y \
    gcc \
    build-essential \
    postgresql-server-dev-all && \
    rm -rf /var/lib/apt/lists/* 

USER root

RUN echo '#!/bin/bash\n\
    for ext_dir in /docker-entrypoint-initdb.d/*; do\n\
        if [ -d "$ext_dir" ]; then\n\
            echo "Building and installing extensions in: $ext_dir"\n\
            cd $ext_dir\n\
            for sub_dir in $ext_dir/*; do\n\
                if [ -d "$sub_dir" ]; then\n\
                    cd $sub_dir && make && make install\n\
                fi\n\
            done\n\
        fi\n\
    done' > /docker-entrypoint-initdb.d/install-extensions.sh

RUN ls -l /docker-entrypoint-initdb.d/

RUN chmod +x /docker-entrypoint-initdb.d/install-extensions.sh

RUN chown -R postgres:postgres /usr/share/postgresql

USER postgres
