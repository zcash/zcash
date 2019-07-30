package=crate_crossbeam_epoch
$(package)_crate_name=crossbeam-epoch
$(package)_version=0.7.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=fedcd6772e37f3da2a9af9bf12ebe046c0dfe657992377b4df982a2b54cd37a9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
