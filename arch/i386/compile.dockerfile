FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

# 最小限の必要パッケージのみインストール
# build-essential: gcc, make等の基本ビルドツール
# gcc-multilib: -m32フラグで32bitコンパイルに必要
# grub-pc-bin: grub-mkrescue Legacy BIOS用GRUBモジュール
# grub-efi-amd64-bin: grub-mkrescue UEFI用GRUBモジュール（EFI/BOOT/BOOTX64.EFI生成に必要）
# grub-common: GRUB共通ファイル
# xorriso: ISO9660イメージ作成
# mtools: FATファイルシステムツール (GRUB用)
# qemu-system-x86: QEMU x86エミュレータ (単体テスト用)
RUN apt-get update \
		&& apt-get install -y --no-install-recommends \
			build-essential \
			gcc-multilib \
			grub-pc-bin \
			grub-efi-amd64-bin \
			grub-common \
			xorriso \
			mtools \
			qemu-system-x86 \
		&& rm -rf /var/lib/apt/lists/*
RUN ln -sf /usr/bin/gcc /usr/local/bin/i686-elf-gcc

ENV PATH="/usr/local/bin:${PATH}"
CMD ["bash"]
