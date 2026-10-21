# Local image mirroring CI (Qt 6.10.3 via aqt on Ubuntu 22.04). Tag: rb-build:qt610
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
      build-essential cmake ninja-build git python3-pip ca-certificates \
      libgl1-mesa-dev libegl1 libxkbcommon0 libxkbcommon-x11-0 libfontconfig1 \
      libfreetype6 libglib2.0-0 libdbus-1-3 libxcb-cursor0 \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install --no-cache-dir aqtinstall==3.3.0 \
    && aqt install-qt linux desktop 6.10.3 linux_gcc_64 -m qtmultimedia -O /opt/Qt
