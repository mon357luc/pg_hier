# Use the official Postgres image as the base
FROM postgres:latest

# Install gcc, build-essential, and postgresql-server-dev-all
RUN apt-get update && \
    apt-get install -y \
    gcc \
    build-essential \
    postgresql-server-dev-all && \
    rm -rf /var/lib/apt/lists/*
