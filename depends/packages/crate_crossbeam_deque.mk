package=crate_crossbeam_deque
$(package)_crate_name=crossbeam-deque
$(package)_version=0.7.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=9f02af974daeee82218205558e51ec8768b48cf524bd01d550abe5573a608285
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
