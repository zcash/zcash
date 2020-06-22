package=crate_ed25519_zebra
$(package)_crate_name=ed25519-zebra
$(package)_version=0.4.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a76f15c88332faad36abb368aca89deb5cc4f440e5181c8848f8bdd049848f7b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
