package=native_libxml2
$(package)_version=2.13.8
$(package)_download_path=https://download.gnome.org/sources/libxml2/2.13
$(package)_file_name=libxml2-$($(package)_version).tar.xz
$(package)_sha256_hash=277294cb33119ab71b2bc81f2f445e9bc9435b893ad15bb2cd2b0e859a0ee84a

# The prebuilt LLVM release binaries we download in native_clang.mk dynamically
# link ld.lld / lld against libxml2.so.2. libxml2 is only used by LLD's
# COFF/Windows-manifest code path, which a native (ELF) build never exercises,
# but the SONAME is still recorded as a hard NEEDED entry, so ld.lld fails to
# even start if libxml2.so.2 is absent ("error while loading shared libraries:
# libxml2.so.2"), and no host package can be linked.
#
# Distributions that have moved to a newer libxml2 (>= 2.14) ship the library
# under the bumped SONAME libxml2.so.16 and no longer provide libxml2.so.2, so
# the downloaded toolchain is unusable there out of the box (e.g. Arch Linux).
# We vendor a minimal libxml2 from the 2.13.x series -- the last whose SONAME is
# still libxml2.so.2 -- into the native lib directory. ld.lld's $ORIGIN/../lib
# RUNPATH then resolves it regardless of what the host system ships.
#
# This is a native package, so it is compiled with the builder's own system
# compiler (default_build_CC), NOT with the downloaded clang/ld.lld it unblocks
# -- there is no bootstrap cycle. It is wired as a dependency of native_clang
# (linux only) in native_clang.mk so it is staged before any host link, and is
# added to native_packages (linux only) in packages.mk. See also the sibling
# native_libtinfo5 shim, which exists for the same class of reason.

define $(package)_set_vars
  # Disable every optional feature that would introduce an external runtime
  # dependency (icu / lzma / zlib / zstd) or interpreter (python); ld.lld only
  # calls the core tree and parser API, so the result needs nothing but libc.
  $(package)_config_opts=--without-icu --without-lzma --without-zlib --without-zstd
  $(package)_config_opts+=--without-python --without-http --without-modules
  $(package)_config_opts+=--disable-static --enable-shared
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libxml2.la
endef

# Stage only the versioned shared object under its SONAME, mirroring
# native_libtinfo5. Installing headers / the .so dev symlink / pkg-config files
# into the native prefix is unnecessary and could be picked up by other builds.
define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp .libs/libxml2.so.2.* $($(package)_staging_prefix_dir)/lib/libxml2.so.2
endef
