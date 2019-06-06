# Container for building the Projecteur package 
# Images available at: https://hub.docker.com/r/jahnf/projecteur/tags

FROM opensuse/leap:15.0

RUN zypper --non-interactive in --no-recommends \
  pkg-config \
  udev \
  gcc-c++ \
  tar \
  make \
  cmake \
  git \
  wget \
  libqt5-qtdeclarative-devel \
  rpmbuild


