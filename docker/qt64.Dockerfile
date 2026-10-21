# Local dev/test image: distro Qt (oldest supported Qt 6 here). Tag: rb-build:24.04
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
      build-essential cmake ninja-build git \
      qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev libgl-dev \
      qt6-tools-dev qt6-l10n-tools \
    && rm -rf /var/lib/apt/lists/*
