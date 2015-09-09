package=libsnark
$(package)_version=0.1
$(package)_download_path=https://github.com/scipr-lab/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=3203a037e6020c929f7da99fcefd6de5e363224c955586d22575c57e6345be59
$(package)_git_commit=c9c0d51f74816ea8e6db052410acafbdb0d31a64

$(package)_dependencies=crypto++ libgmp xbyak ate-pairing
$(package)_patches=1_cxxflags_prefix.patch 2_fix_Wl_flag.patch 3_ldflags_prefix.patch 4_make_depinst_optional.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/1_cxxflags_prefix.patch && \
    patch -p1 < $($(package)_patch_dir)/2_fix_Wl_flag.patch && \
    patch -p1 < $($(package)_patch_dir)/3_ldflags_prefix.patch && \
    patch -p1 < $($(package)_patch_dir)/4_make_depinst_optional.patch
endef

define $(package)_build_cmds
  $(MAKE) lib CURVE=ALT_BN128 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 CXXFLAGS_PREFIX='$($(package)_cppflags)' LDFLAGS_PREFIX='$($(package)_ldflags)'
endef

define $(package)_stage_cmds
  $(MAKE) install PREFIX=$($(package)_staging_dir)$(host_prefix) CURVE=ALT_BN128
endef
