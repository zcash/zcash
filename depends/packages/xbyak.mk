package=xbyak
$(package)_version=0.1
$(package)_download_path=https://github.com/herumi/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=467a9037c29bc417840177f3ff5d76910d3f688f2f216dd86ced4a7ac837bfb0
$(package)_dependencies=

$(package)_git_commit=62fd6d022acd83209e2a5af8ec359a3a1bed3a50

define $(package)_build_cmds
  echo 'xbyak build is unnecessary for consumer ate-pairing.'
endef

define $(package)_stage_cmds
  $(MAKE) install PREFIX=$($(package)_staging_dir)$(host_prefix)
endef
