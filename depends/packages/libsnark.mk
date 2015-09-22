package=libsnark
$(package)_version=0.1
$(package)_download_path=https://github.com/scipr-lab/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=5876588a523fd033d6928f46452512327a75b32533e980daa800730446edd28c
$(package)_git_commit=2474695678b3529841eced49ea97ca683e91996c

$(package)_dependencies=crypto++ libgmp xbyak ate-pairing
$(package)_patches=1_cxxflags_prefix.patch 2_fix_Wl_flag.patch 3_ldflags_prefix.patch 4_make_depinst_optional.patch

$(package)_cppflags += -fPIC

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/1_cxxflags_prefix.patch && \
    patch -p1 < $($(package)_patch_dir)/2_fix_Wl_flag.patch && \
    patch -p1 < $($(package)_patch_dir)/3_ldflags_prefix.patch && \
    patch -p1 < $($(package)_patch_dir)/4_make_depinst_optional.patch
endef

define $(package)_build_cmds
  $(MAKE) lib CURVE=ALT_BN128 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 CXXFLAGS_PREFIX='$($(package)_cppflags)' LDFLAGS_PREFIX='$($(package)_ldflags)'
endef

define $(package)_stage_cmds
  $(MAKE) install STATIC=1 PREFIX=$($(package)_staging_dir)$(host_prefix) CURVE=ALT_BN128
endef
