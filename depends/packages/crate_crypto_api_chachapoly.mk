package=crate_crypto_api_chachapoly
$(package)_crate_name=crypto_api_chachapoly
$(package)_version=0.4.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=d930b6a026ce9d358a17f9c9046c55d90b14bb847f36b6ebb6b19365d4feffb8
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
