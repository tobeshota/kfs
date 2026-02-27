FROM ubuntu:24.04
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
 && apt-get upgrade -y \
 && apt-get install -y clang-format shfmt \
 && rm -rf /var/lib/apt/lists/*
