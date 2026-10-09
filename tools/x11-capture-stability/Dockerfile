# Debian trixie: GCC 14 and CMake 3.31 meet Cimmerian's toolchain floor.
# Xvfb is the X server; xcompmgr adds a compositing manager on top of it.
FROM debian:trixie
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       g++ cmake make libx11-dev libxtst-dev xvfb xcompmgr \
    && rm -rf /var/lib/apt/lists/*
