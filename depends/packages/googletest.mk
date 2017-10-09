package=googletest
$(package)_version=1.7.0
$(package)_download_path=https://github.com/google/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version).tar.gz
$(package)_sha256_hash=f73a6546fdf9fce9ff93a5015e0333a8af3062a152a9ad6bcb772c96687016cc

ifeq ($(build_os),darwin)
define $(package)_set_vars
    $(package)_build_env=AR="$($(package)_ar)" RANLIB="$($(package)_ranlib)" CC="$($(package)_cc)" CXX="$($(package)_cxx)" CXXFLAGS="$($(package)_cxxflags)"
endef
endif

ifeq ($(build_os),darwin)
$(package)_install=ginstall
define $(package)_build_cmds
    $(MAKE) -C make gtest.a
endef
else
$(package)_install=install
define $(package)_build_cmds
  $(MAKE) -C make CXXFLAGS=-fPIC gtest.a
endef
endif

define $(package)_stage_cmds
  $($(package)_install) -D ./make/gtest.a $($(package)_staging_dir)$(host_prefix)/lib/libgtest.a && \
  cp -a ./include $($(package)_staging_dir)$(host_prefix)/include
endef
