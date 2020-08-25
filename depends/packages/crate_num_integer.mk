package=crate_num_integer
$(package)_crate_name=num-integer
$(package)_version=0.1.43
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=8d59457e662d541ba17869cf51cf177c0b5f0cbf476c66bdc90bf1edac4f875b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
