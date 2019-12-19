package=crate_num_traits
$(package)_crate_name=num-traits
$(package)_version=0.2.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=d4c81ffc11c212fa327657cb19dd85eb7419e163b5b076bede2bdb5c974c07e4
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
