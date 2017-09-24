package=libsnark
$(package)_version=0.1
$(package)_download_path=https://github.com/zcash/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=dad153fe46e2e1f33557a195cbe7d69aed8b19ed9befc08ddcb8c6d3c025941f
$(package)_git_commit=9ada3f84ab484c57b2247c2f41091fd6a0916573

define $(package)_set_vars
    $(package)_build_env=CC="$($(package)_cc)" CXX="$($(package)_cxx)"
    $(package)_build_env+=CXXFLAGS="$($(package)_cxxflags) -DBINARY_OUTPUT -DSTATICLIB -DNO_PT_COMPRESSION=1 "
endef

$(package)_dependencies=libgmp libsodium

define $(package)_build_cmds
  $(MAKE) lib DEPINST=$(host_prefix) CURVE=ALT_BN128 MULTICORE=1 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 NO_SUPERCOP=1 FEATUREFLAGS=-DMONTGOMERY_OUTPUT OPTFLAGS="-O2 -march=x86-64"
endef

define $(package)_stage_cmds
  $(MAKE) install STATIC=1 DEPINST=$(host_prefix) PREFIX=$($(package)_staging_dir)$(host_prefix) CURVE=ALT_BN128 NO_SUPERCOP=1
endef
