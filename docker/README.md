# Projecteur Dockerfiles

Docker configuration files for build containers used in _Projecteur_ CI builds.

Example for creating an image:
```
docker build -f Dockerfile.ubuntu-20.10 --tag jahnf/projecteur:ubuntu-20.10 .
```

Images used in the CI build can be found on docker hub:
https://hub.docker.com/r/jahnf/projecteur

