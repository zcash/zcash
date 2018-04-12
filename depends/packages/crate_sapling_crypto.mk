package=crate_sapling_crypto
$(package)_crate_name=sapling-crypto
$(package)_download_path=https://github.com/zcash-hackworks/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=5eb4040bc223a689341b3f1a1fc53d6064c4c032b23ae0c2c653b063e1da24db
$(package)_git_commit=e554b473dd10885d232f42237c13282f5b6fee43
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
