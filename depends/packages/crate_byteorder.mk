package=crate_byteorder
$(package)_crate_name=byteorder
$(package)_version=1.3.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a7c3dd8985a7111efc5c80b44e23ecdd8c007de8ade3b96595387e812b957cf5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
