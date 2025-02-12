This repository has a docker-compose file that automatically pulls all extensoins into the postgres server for use.

To start:
```

C:\path\to\git\repo\directory> docker compose up -d    # -d option to spin up the container detached

```

Then, in a SQL Client of your choice, connect to localhost:5432 using the DB and User defined in the .env file.

Adding extensions to the database can be done with the following script:

```

CREATE EXTENSION first_extension;

```