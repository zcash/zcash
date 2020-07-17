package=crate_num_traits
$(package)_crate_name=num-traits
$(package)_version=0.2.8
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6ba9a427cfca2be13aa6f6403b0b7e7368fe982bfa16fccc450ce74c46cd9b32
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
