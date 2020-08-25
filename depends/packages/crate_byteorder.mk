package=crate_byteorder
$(package)_crate_name=byteorder
$(package)_version=1.3.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=08c48aae112d48ed9f069b33538ea9e3e90aa263cfa3d1c24309612b1f7472de
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
