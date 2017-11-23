package=googletest
$(package)_version=1.8.0
$(package)_download_path=https://github.com/google/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version).tar.gz
$(package)_sha256_hash=58a6f4277ca2bc8565222b3bbd58a177609e9c488e8a72649359ba51450db7d8

define $(package)_build_cmds
  $(MAKE) -C googlemock/make CXXFLAGS='-fPIC' gmock.a
endef

define $(package)_stage_cmds
  install -D ./googlemock/make/gmock.a $($(package)_staging_dir)$(host_prefix)/lib/libgmock.a && \
  cp -a ./googletest/include $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./googlemock/include/gmock $($(package)_staging_dir)$(host_prefix)/include
endef
