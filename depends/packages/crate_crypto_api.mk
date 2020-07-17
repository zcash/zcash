package=crate_crypto_api
$(package)_crate_name=crypto_api
$(package)_version=0.2.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=2f855e87e75a4799e18b8529178adcde6fd4f97c1449ff4821e747ff728bb102
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
