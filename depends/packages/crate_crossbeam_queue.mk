package=crate_crossbeam_queue
$(package)_crate_name=crossbeam-queue
$(package)_version=0.2.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=774ba60a54c213d409d5353bda12d49cd68d14e45036a285234c8d6f91f92570
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
