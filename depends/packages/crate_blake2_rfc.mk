package=crate_blake2_rfc
$(package)_crate_name=blake2-rfc
$(package)_download_path=https://github.com/gtank/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=8a873cc41f02e669e8071ab5919931dd4263f050becf0c19820b0497c07b0ca3
$(package)_git_commit=7a5b5fc99ae483a0043db7547fb79a6fa44b88a9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
