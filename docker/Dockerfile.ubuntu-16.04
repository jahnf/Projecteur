# Container for building the Projecteur package 
# Images available at: https://hub.docker.com/r/jahnf/projecteur/tags

FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
  ca-certificates \
	g++ \
	make \
	udev \
	git \
	pkg-config \
	qtdeclarative5-dev \
  wget

RUN cd /tmp && wget https://cmake.org/files/v3.14/cmake-3.14.4-Linux-x86_64.sh \
	&& chmod +x cmake-3.14.4-Linux-x86_64.sh \
	&& ./cmake-3.14.4-Linux-x86_64.sh --skip-license --prefix=/usr/local \
	&& rm cmake-3.14.4-Linux-x86_64.sh && cd /

RUN mkdir /build && cd /build
