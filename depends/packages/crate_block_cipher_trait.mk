package=crate_block_cipher_trait
$(package)_crate_name=block-cipher-trait
$(package)_version=0.6.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=1c924d49bd09e7c06003acda26cd9742e796e34282ec6c1189404dee0c1f4774
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
