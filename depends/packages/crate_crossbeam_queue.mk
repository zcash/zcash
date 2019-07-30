package=crate_crossbeam_queue
$(package)_crate_name=crossbeam-queue
$(package)_version=0.1.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=7c979cd6cfe72335896575c6b5688da489e420d36a27a0b9eb0c73db574b4a4b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
