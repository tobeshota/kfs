FROM ubuntu:24.04
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
 && apt-get install -y doxygen graphviz \
 && rm -rf /var/lib/apt/lists/*
