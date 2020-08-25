package=crate_equihash
$(package)_crate_name=equihash
$(package)_version=0.1.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=4127688f6177e3f57521881cb1cfd90d1228214f9dc43b8efe6f6c6948cd8280
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
