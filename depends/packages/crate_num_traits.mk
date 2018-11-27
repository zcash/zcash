package=crate_num_traits
$(package)_crate_name=num-traits
$(package)_version=0.2.5
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=630de1ef5cc79d0cdd78b7e33b81f083cbfe90de0f4b2b2f07f905867c70e9fe
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
