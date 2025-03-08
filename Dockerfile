FROM postgres:latest

# Install necessary build dependencies
RUN apt-get update && \
    apt-get install -y \
    gcc \
    build-essential \
    postgresql-server-dev-all && \
    rm -rf /var/lib/apt/lists/*

# Ensure the permissions for the postgres user
RUN chown -R postgres:postgres /usr/share/postgresql /docker-entrypoint-initdb.d

# Switch back to postgres user to run PostgreSQL
USER postgres
