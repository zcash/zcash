package=crate_num_bigint
$(package)_crate_name=num-bigint
$(package)_version=0.2.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=3eceac7784c5dc97c2d6edf30259b4e153e6e2b42b3c85e9a6e9f45d06caef6e
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
