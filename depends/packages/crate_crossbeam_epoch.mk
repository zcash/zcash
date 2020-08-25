package=crate_crossbeam_epoch
$(package)_crate_name=crossbeam-epoch
$(package)_version=0.8.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=058ed274caafc1f60c4997b5fc07bf7dc7cca454af7c6e81edffe5f33f70dace
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
