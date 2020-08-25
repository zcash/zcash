package=crate_libc
$(package)_crate_name=libc
$(package)_version=0.2.76
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=755456fae044e6fa1ebbbd1b3e902ae19e73097ed4ed87bb79934a867c007bc3
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
