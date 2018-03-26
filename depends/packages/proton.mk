package=proton
$(package)_version=0.17.0
$(package)_download_path=http://apache.cs.utah.edu/qpid/proton/$($(package)_version)
$(package)_file_name=qpid-proton-$($(package)_version).tar.gz
$(package)_sha256_hash=6ffd26d3d0e495bfdb5d9fefc5349954e6105ea18cc4bb191161d27742c5a01a
$(package)_patches=minimal-build.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/minimal-build.patch && \
  mkdir -p build/proton-c/src
endef

define $(package)_config_cmds
  cd build; cmake .. -DCMAKE_CXX_STANDARD=11 -DCMAKE_INSTALL_PREFIX=/ -DSYSINSTALL_BINDINGS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_PYTHON=OFF -DBUILD_PHP=OFF -DBUILD_JAVA=OFF -DBUILD_PERL=OFF -DBUILD_RUBY=OFF -DBUILD_JAVASCRIPT=OFF -DBUILD_GO=OFF
endef

define $(package)_build_cmds
  cd build; $(MAKE) VERBOSE=1
endef

define $(package)_stage_cmds
  cd build; $(MAKE) VERBOSE=1 DESTDIR=$($(package)_staging_prefix_dir) install
endef

