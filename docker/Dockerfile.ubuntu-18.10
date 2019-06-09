# Container for building the Projecteur package 
# Images available at: https://hub.docker.com/r/jahnf/projecteur/tags

FROM ubuntu:18.10

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
  ca-certificates \
	g++ \
	make \
	cmake \
	git \
	pkg-config \
	qtdeclarative5-dev

RUN apt-get install -y --no-install-recommends udev
