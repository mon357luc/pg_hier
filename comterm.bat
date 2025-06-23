@echo off

REM Terminal 1: Navigate to project and run docker compose up
cd /d C:\Users\Dupree\Documents\Github\Helloworldextension && docker compose up -d && start cmd.exe /k "docker exec -it -u root postgres_container bash" && start cmd.exe /k "docker exec -it postgres_container psql -d db"