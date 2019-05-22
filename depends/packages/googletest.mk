package=googletest
$(package)_version=1.8.0
$(package)_download_path=https://github.com/google/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version).tar.gz
$(package)_sha256_hash=58a6f4277ca2bc8565222b3bbd58a177609e9c488e8a72649359ba51450db7d8

define $(package)_set_vars
$(package)_cxxflags+=-std=c++11
$(package)_cxxflags_linux=-fPIC
$(package)_cxxflags_freebsd=-fPIC
endef

define $(package)_build_cmds
  $(MAKE) -C googlemock/make CC="$($(package)_cc)" CXX="$($(package)_cxx)" AR="$($(package)_ar)" CXXFLAGS="$($(package)_cxxflags)" gmock.a && \
  $(MAKE) -C googletest/make CC="$($(package)_cc)" CXX="$($(package)_cxx)" AR="$($(package)_ar)" CXXFLAGS="$($(package)_cxxflags)" gtest.a
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/lib && \
  install ./googlemock/make/gmock.a $($(package)_staging_dir)$(host_prefix)/lib/libgmock.a && \
  install ./googletest/make/gtest.a $($(package)_staging_dir)$(host_prefix)/lib/libgtest.a && \
  cp -a ./googlemock/include $($(package)_staging_dir)$(host_prefix)/ && \
  cp -a ./googletest/include $($(package)_staging_dir)$(host_prefix)/
endef
