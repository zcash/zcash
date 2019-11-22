package=crate_num_integer
$(package)_crate_name=num-integer
$(package)_version=0.1.41
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b85e541ef8255f6cf42bbfe4ef361305c6c135d10919ecc26126c4e5ae94bc09
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
