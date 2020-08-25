package=crate_curve25519_dalek
$(package)_crate_name=curve25519-dalek
$(package)_version=3.0.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c8492de420e9e60bc9a1d66e2dbb91825390b738a388606600663fc529b4b307
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
