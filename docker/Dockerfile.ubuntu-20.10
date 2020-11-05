# Container for building the Projecteur package
# Images available at: https://hub.docker.com/r/jahnf/projecteur/tags

FROM ubuntu:20.10

RUN apt-get update && mkdir /build
RUN DEBIAN_FRONTEND="noninteractive" \
 apt-get install -y --no-install-recommends \
  ca-certificates \
  g++ \
  make \
  cmake \
  udev \
  git \
  pkg-config \
  qtdeclarative5-dev \
  qttools5-dev-tools \
  qttools5-dev \
  qt5-default \
  libqt5x11extras5-dev \
  libusb-1.0-0-dev \
  && rm -rf /var/lib/apt/lists/*
