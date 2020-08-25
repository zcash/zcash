package=crate_curve25519_dalek
$(package)_crate_name=curve25519-dalek
$(package)_version=2.1.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=5d85653f070353a16313d0046f173f70d1aadd5b42600a14de626f0dfb3473a5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
