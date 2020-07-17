package=crate_arrayvec
$(package)_crate_name=arrayvec
$(package)_version=0.4.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b8d73f9beda665eaa98ab9e4f7442bd4e7de6652587de55b2525e52e29c1b0ba
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
