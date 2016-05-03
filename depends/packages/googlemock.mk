# url=https://github.com/google/googlemock/archive/release-1.7.0.tar.gz

package=googlemock
$(package)_version=1.7.0
$(package)_dependencies=googletest

$(package)_download_path=https://github.com/google/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version).tar.gz
$(package)_sha256_hash=3f20b6acb37e5a98e8c4518165711e3e35d47deb6cdb5a4dd4566563b5efd232

define $(package)_build_cmds
  $(MAKE) -C make GTEST_DIR='$(host_prefix)' CXXFLAGS='-fPIC' gmock-all.o
endef


define $(package)_stage_cmds
  install -D ./make/gmock-all.o $($(package)_staging_dir)$(host_prefix)/lib/libgmock.a && \
  cp -a ./include $($(package)_staging_dir)$(host_prefix)/include
endef
