package=crate_bit_vec
$(package)_crate_name=bit-vec
$(package)_version=0.6.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=5f0dc55f2d8a1a85650ac47858bb001b4c0dd73d79e3c455a842925e68d29cd3
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
