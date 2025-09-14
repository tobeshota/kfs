FROM i386/ubuntu

WORKDIR /work

RUN apt update && \
	apt install -y gcc-multilib make lcov
