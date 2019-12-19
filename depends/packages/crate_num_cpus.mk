package=crate_num_cpus
$(package)_crate_name=num_cpus
$(package)_version=1.11.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=76dac5ed2a876980778b8b85f75a71b6cbf0db0b1232ee12f826bccb00d09d72
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
