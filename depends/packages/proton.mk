package=proton
$(package)_version=0.26.0
$(package)_download_path=https://archive.apache.org/dist/qpid/proton/$($(package)_version)
$(package)_file_name=qpid-proton-$($(package)_version).tar.gz
$(package)_sha256_hash=0eddac870f0085b9aeb0c9da333bd3f53fedb7c872164171a7cc06761ddbbd75
$(package)_patches=minimal-build.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/minimal-build.patch && \
  mkdir -p build/proton-c/src
endef

define $(package)_config_cmds
  cd build; cmake .. -DCMAKE_CXX_STANDARD=11 -DCMAKE_INSTALL_PREFIX=/ -DSYSINSTALL_BINDINGS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_PYTHON=OFF -DBUILD_RUBY=OFF -DBUILD_GO=OFF -DBUILD_STATIC_LIBS=ON -DLIB_SUFFIX= -DENABLE_JSONCPP=
endef

define $(package)_build_cmds
  cd build; $(MAKE) VERBOSE=1
endef

define $(package)_stage_cmds
  cd build; $(MAKE) VERBOSE=1 DESTDIR=$($(package)_staging_prefix_dir) install
endef

