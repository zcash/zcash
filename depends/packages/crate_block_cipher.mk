package=crate_block_cipher
$(package)_crate_name=block-cipher
$(package)_version=0.8.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=f337a3e6da609650eb74e02bc9fac7b735049f7623ab12f2e4c719316fcc7e80
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
