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
            echo "Building and installing extension: $ext_dir"\n\
            cd $ext_dir && make && make install\n\
        fi\n\
    done' > /docker-entrypoint-initdb.d/install-extensions.sh

RUN chmod +x /docker-entrypoint-initdb.d/install-extensions.sh

RUN chown -R postgres:postgres /usr/share/postgresql

USER postgres
