package=crate_zip32
$(package)_crate_name=zip32
$(package)_download_path=https://github.com/zcash-hackworks/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=b0b011ea96524f0d918a44c7ab8a3dec6270879d1ff03d7dbda6c676d25caa7e
$(package)_git_commit=176470ef41583b5bd0bd749bd1b61d417aa8ec79
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
