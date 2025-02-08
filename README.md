Docker image to create environment suited for create postgres extensions.

To start:
```

C:\> cd path\to\git\repo\directory
C:\path\to\git\repo\directory> docker image build
C:\path\to\git\repo\directory> docker compose up -v
C:\path\to\git\repo\directory> docker container exec -it postgres-container bash
root@002c2207264f:/# cd docker-entrypoint-initdb.d/my_extension/
root@002c2207264f:/docker-entrypoint-initdb.d/my_extension# make
root@002c2207264f:/docker-entrypoint-initdb.d/my_extension# make install

```

In a sql client, call

```

CREATE EXTENSION first_extension;

```