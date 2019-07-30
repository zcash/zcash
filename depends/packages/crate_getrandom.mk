package=crate_getrandom
$(package)_crate_name=getrandom
$(package)_version=0.1.12
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=473a1265acc8ff1e808cd0a1af8cee3c2ee5200916058a2ca113c29f2d903571
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
