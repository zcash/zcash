package=crate_ed25519_zebra
$(package)_crate_name=ed25519-zebra
$(package)_download_path=https://github.com/ebfull/$($(package)_crate_name)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=e0b432e0161abe9cb27cf96dd42c1486d32297e2fdbf725b70e7ae1f5b53d052
$(package)_git_commit=8c97acde89b6446f7f2505069fc7b9ac3a541417
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_unpackaged_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef