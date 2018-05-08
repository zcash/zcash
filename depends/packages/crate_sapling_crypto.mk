package=crate_sapling_crypto
$(package)_crate_name=sapling-crypto
$(package)_download_path=https://github.com/zcash-hackworks/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=5062b9e752066ad959f14063d496b0a156ce96004a13a6823494249793c01f96
$(package)_git_commit=7beeb52730e24724ee10ea2458ecf7776cb59c58
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
