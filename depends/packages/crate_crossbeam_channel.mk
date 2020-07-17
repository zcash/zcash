package=crate_crossbeam_channel
$(package)_crate_name=crossbeam-channel
$(package)_version=0.3.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c8ec7fcd21571dc78f96cc96243cab8d8f035247c3efd16c687be154c3fa9efa
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
