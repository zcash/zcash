package=crate_num_integer
$(package)_crate_name=num-integer
$(package)_version=0.1.39
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=e83d528d2677f0518c570baf2b7abdcf0cd2d248860b68507bdcb3e91d4c0cea
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
