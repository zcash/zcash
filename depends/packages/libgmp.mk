package=libgmp

ifeq ($(host_os),mingw32)
$(package)_download_path=https://github.com/joshuayabut/$(package)/archive
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=193836c1acc9dc00fe2521205d7bbe1ba13263f6cbef6f02584bf6f8b34b108f
$(package)_git_commit=053c03b1cab347671d936f43ef66b48ab5e380ee
$(package)_dependencies=
$(package)_config_opts=--enable-cxx --disable-shared
else ifeq ($(build_os),darwin)
$(package)_download_path=https://github.com/ca333/$(package)/archive
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=59b2c2b5d58fdf5943bfde1fa709e9eb53e7e072c9699d28dc1c2cbb3c8cc32c
$(package)_git_commit=aece03c7b6967f91f3efdac8c673f55adff53ab1
$(package)_dependencies=
$(package)_config_opts=--enable-cxx --disable-shared
else
$(package)_version=6.1.1
$(package)_download_path=https://ftp.gnu.org/gnu/gmp
$(package)_file_name=gmp-$($(package)_version).tar.bz2
$(package)_sha256_hash=a8109865f2893f1373b0a8ed5ff7429de8db696fc451b1036bd7bdf95bbeffd6
$(package)_dependencies=
$(package)_config_opts=--enable-cxx --disable-shared
endif

define $(package)_config_cmds
  $($(package)_autoconf) --host=$(host) --build=$(build)
endef

ifeq ($(build_os),darwin)
define $(package)_build_cmds
	$(MAKE)
endef
else
define $(package)_build_cmds
  $(MAKE) CPPFLAGS='-fPIC'
endef
endif

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install ; echo '=== staging find for $(package):' ; find $($(package)_staging_dir)
endef
