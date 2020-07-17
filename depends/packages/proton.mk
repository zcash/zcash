package=proton
$(package)_version=0.30.0
$(package)_download_path=https://archive.apache.org/dist/qpid/proton/$($(package)_version)
$(package)_file_name=qpid-proton-$($(package)_version).tar.gz
$(package)_sha256_hash=e37fd8fb13391c3996f927839969a8f66edf35612392d0611eeac6e39e48dd33
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

