package=native_cdrkit
$(package)_version=1.1.11
$(package)_download_path=https://distro.ibiblio.org/fatdog/source/600/c
$(package)_file_name=cdrkit-$($(package)_version).tar.bz2
$(package)_sha256_hash=b50d64c214a65b1a79afe3a964c691931a4233e2ba605d793eb85d0ac3652564
$(package)_patches=cdrkit-deterministic.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/cdrkit-deterministic.patch && \
  sed -i.old '1i cmake_minimum_required(VERSION 3.5)' CMakeLists.txt && \
  sed -i.old '/Install libcap-dev/d' wodim/CMakeLists.txt && \
  sed -i.old '1i #include "checksum.h"' genisoimage/genisoimage.c
endef

# Starting with 10.1, GCC defaults to -fno-common, resulting in linking errors.
# Pass -fcommon to retain the legacy behaviour.
define $(package)_config_cmds
  cmake -DCMAKE_INSTALL_PREFIX=$(build_prefix) -DCMAKE_C_FLAGS="-fcommon -Wno-error=implicit-function-declaration" .
endef

define $(package)_build_cmds
  $(MAKE) genisoimage
endef

# Older versions of cmake do not generate install target properly, but we
# need to support them because that's what is in xenial and we use xenial
# for reproducible builds. So we just fallback on installing everything.
define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cp genisoimage/genisoimage $($(package)_staging_prefix_dir)/bin/
endef

define $(package)_postprocess_cmds
  rm -f bin/isovfy bin/isoinfo bin/isodump bin/isodebug bin/devdump
endef
