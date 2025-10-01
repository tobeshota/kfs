FROM debian:bookworm

ARG TARGET=i686-elf
ENV BINUTILS_VERSION=2.41 \
    GCC_VERSION=13.2.0 \
    PREFIX=/usr/local \
    SRC_DIR=/usr/local/src \
    JOBS=8 \
    DEBIAN_FRONTEND=noninteractive

# Build deps only for binutils+gcc (C only) + ISO/QEMU tools for convenience
ENV BUILD_DEPS="build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo wget ca-certificates xz-utils grub-pc-bin grub-common xorriso mtools make libmpc3 libmpfr6 libgmp10"

RUN apt-get update \
	&& apt-get install -y --no-install-recommends ${BUILD_DEPS} \
	&& rm -rf /var/lib/apt/lists/*

RUN mkdir -p ${SRC_DIR}

# binutils + i686-elf-gcc (C only) in a single layer
RUN set -eu; \
        # download binutils
		cd ${SRC_DIR}; \
		TAR=binutils-${BINUTILS_VERSION}.tar.xz; \
		for url in \
				"https://sourceware.org/pub/binutils/releases/${TAR}" \
				"https://ftpmirror.gnu.org/binutils/${TAR}" \
				"https://ftp.gnu.org/gnu/binutils/${TAR}"; do \
				echo "Downloading: $url"; \
				wget -q "$url" && break || true; \
		done; \
        # install binutils
		test -f "$TAR"; \
		tar -Jxf "$TAR" --no-same-owner --no-same-permissions; \
		rm -f "$TAR"; \
		mkdir build-binutils; \
		cd build-binutils; \
		../binutils-${BINUTILS_VERSION}/configure \
			--target=${TARGET} \
			--prefix=${PREFIX} \
			--with-sysroot \
			--disable-nls \
			--disable-werror; \
		make -j ${JOBS}; \
		make install; \
        # download i686-elf-gcc
		cd ${SRC_DIR}; \
		TAR=gcc-${GCC_VERSION}.tar.xz; \
		for url in \
				"https://ftpmirror.gnu.org/gcc/gcc-${GCC_VERSION}/${TAR}" \
				"https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/${TAR}"; do \
				echo "Downloading: $url"; \
				wget -q "$url" && break || true; \
		done; \
        # install i686-elf-gcc
		test -f "$TAR"; \
		tar -Jxf "$TAR" --no-same-owner --no-same-permissions; \
		rm -f "$TAR"; \
		mkdir build-gcc; \
		cd build-gcc; \
		../gcc-${GCC_VERSION}/configure \
			--target=${TARGET} \
			--prefix=${PREFIX} \
			--disable-nls \
			--enable-languages=c \
			--without-headers; \
		make -j ${JOBS} all-gcc; \
		make -j ${JOBS} all-target-libgcc; \
		make install-gcc; \
		make install-target-libgcc; \
		cd /; \
        # cleanup source and build dirs
		rm -rf ${SRC_DIR}

# final settings
ENV PATH="/usr/local/bin:${PATH}"
CMD ["bash"]
