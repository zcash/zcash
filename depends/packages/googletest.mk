package=googletest
$(package)_version=1.12.1
$(package)_download_path=https://github.com/google/$(package)/archive/refs/tags
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version).tar.gz
$(package)_sha256_hash=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2
$(package)_build_subdir=build
$(package)_dependencies=native_cmake

ifneq ($(host_os),darwin)
$(package)_dependencies+=libcxx
endif

define $(package)_set_vars
$(package)_cxxflags+=-std=c++17
$(package)_cxxflags_linux=-fPIC
$(package)_cxxflags_freebsd=-fPIC

ifeq ($(host_os),freebsd)
  $(package)_ldflags+=-static-libstdc++ -lcxxrt
else
  $(package)_ldflags+=-static-libstdc++ -lc++abi
endif

endef

define $(package)_preprocess_cmds
  mkdir build
endef

define $(package)_config_cmds
  $($(package)_cmake) ..
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
