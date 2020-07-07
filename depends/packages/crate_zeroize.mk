package=crate_zeroize
$(package)_crate_name=zeroize
$(package)_version=1.1.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=3cbac2ed2ba24cc90f5e06485ac8c7c1e5449fe8911aef4d8877218af021a5b8
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
