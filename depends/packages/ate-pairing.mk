package=ate-pairing
$(package)_version=0.1
$(package)_download_path=https://github.com/herumi/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=37c05b4a60653b912a0130d77ac816620890d65a51dd9629ed65c15b54c2d8e0
$(package)_dependencies=xbyak libgmp

$(package)_git_commit=dd7889f2881e66f87165fcd180a03cf659bcb073

define $(package)_build_cmds
  $(MAKE) -j SUPPORT_SNARK=1 INC_DIR='-I../include $($(package)_cppflags)' LIB_DIR='-L../lib $($(package)_ldflags)'
endef

define $(package)_stage_cmds
  cp -rv include/ lib/ $($(package)_staging_dir)$(host_prefix)
endef
