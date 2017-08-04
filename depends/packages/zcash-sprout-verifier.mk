package=zcash-sprout-verifier
$(package)_version=0.1
$(package)_download_path=https://github.com/plutomonkey/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=eee070262f7b224176d3cb2ee2193e31ed73d4748241620ac4fe308093e87a0b
$(package)_git_commit=9af377de0b130c8516b2d936a1c2cc12f354ca02
$(package)_dependencies=rust

define $(package)_build_cmds
  cargo build --release
endef

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/lib/ && \
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp target/release/libzcash_sprout_verifier.a $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp include/zcash_sprout_verifier.h $($(package)_staging_dir)$(host_prefix)/include/
endef
