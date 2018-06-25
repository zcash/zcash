package=crate_sapling_crypto
$(package)_crate_name=sapling-crypto
$(package)_download_path=https://github.com/zcash-hackworks/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=480ffc31b399a9517178289799f9db01152bedbc534a6a2d2dbac245421fb6fe
$(package)_git_commit=6abfcca25ae233922ecc18a4d2d0b5cb7aab7c8c
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
