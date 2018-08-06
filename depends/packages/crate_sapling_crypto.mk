package=crate_sapling_crypto
$(package)_crate_name=sapling-crypto
$(package)_download_path=https://github.com/zcash-hackworks/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=ae3a122b1f1ce97b4e80e0e8542e19aa1516e99e6c72875688c886af1a881558
$(package)_git_commit=21084bde2019c04bd34208e63c3560fe2c02fb0e
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
