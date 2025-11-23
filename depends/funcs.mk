# ==============================================================================
# Package Build System Definitions
# This file contains Make macros (defines) for defining package metadata,
# handling dependency resolution, downloading/caching sources, and generating
# build steps.
# ==============================================================================

# Define package-specific internal variables (toolchain, flags, paths).
# $(1) is the package name.
define int_vars
# Set defaults for vars which may be overridden per-package
$(1)_cc=$($($(1)_type)_CC)
$(1)_cxx=$($($(1)_type)_CXX)
$(1)_objc=$($($(1)_type)_OBJC)
$(1)_objcxx=$($($(1)_type)_OBJCXX)
$(1)_ar=$($($(1)_type)_AR)
$(1)_ranlib=$($($(1)_type)_RANLIB)
$(1)_libtool=$($($(1)_type)_LIBTOOL)
$(1)_nm=$($($(1)_type)_NM)
# Combine common and release-specific compiler flags (C)
$(1)_cflags=$($($(1)_type)_CFLAGS) $($($(1)_type)_$(release_type)_CFLAGS)
# Combine common and release-specific compiler flags (C++)
$(1)_cxxflags=$($($(1)_type)_CXXFLAGS) $($($(1)_type)_$(release_type)_CXXFLAGS)
# Combine common and release-specific linker flags, and add linker search path
$(1)_ldflags=$($($(1)_type)_LDFLAGS) $($($(1)_type)_$(release_type)_LDFLAGS) -L$($($(1)_type)_prefix)/lib
# Combine common and release-specific preprocessor flags, and add include path
$(1)_cppflags=$($($(1)_type)_CPPFLAGS) $($($(1)_type)_$(release_type)_CPPFLAGS) -I$($($(1)_type)_prefix)/include
# Initialize the package recipe hash
$(1)_recipe_hash:=
endef

# Recursively finds all dependencies for a given list of packages.
# $(1) is the package name (context), $(2) is the current list of dependencies.
define int_get_all_dependencies
$(sort $(foreach dep,$(2),$(2) $(call int_get_all_dependencies,$(1),$($(dep)_dependencies))))
endef

# Downloads a file and verifies its SHA256 checksum.
# $(1)=download_dir, $(2)=file_name (base), $(3)=url, $(4)=hash
define download_and_check_file
($(build_DOWNLOAD) "$(1)/$(2).temp" $(3) && \
  echo "$(4)  $(1)/$(2).temp" > $(1)/.$(2).hash && \
  $(build_SHA256SUM) -c $(1)/.$(2).hash)
endef

# Fetches a file, handling primary and fallback download paths.
# $(1)=package, $(2)=download_path, $(3)=download_file, $(4)=file_name, $(5)=sha256_hash
define fetch_file
(test -f $$($(1)_source_dir)/$(4) || \
  ( mkdir -p $$($(1)_download_dir) && echo "Fetching $(1) from $(2)/$(3) ..." && \
  ( $(call download_and_check_file,$$($(1)_download_dir),$(4),"$(2)/$(3)",$(5)) || \
    ( echo "Falling back to fetching $(1) from $(FALLBACK_DOWNLOAD_PATH)/$(4) ..." && \
      $(call download_and_check_file,$$($(1)_download_dir),$(4),"$(FALLBACK_DOWNLOAD_PATH)/$(4)",$(5)) ) ) && \
    mv $$($(1)_download_dir)/$(4).temp $$($(1)_source_dir)/$(4) && \
    rm -rf $$($(1)_download_dir) ))
endef

# Handles vendoring dependencies for a Rust crate using `cargo vendor`.
# $(1)=package, $(2)=crate_archive, $(3)=Cargo_lock_path, $(4)=Manifest_path, $(5)=vendor_archive_name
define vendor_crate_deps
(test -f $$($(1)_source_dir)/$(5) || \
  ( mkdir -p $$($(1)_download_dir)/$(1) && echo Vendoring dependencies for $(1)... && \
    tar -xf $(native_rust_cached) -C $$($(1)_download_dir) && \
    tar --strip-components=1 -xf $$($(1)_source_dir)/$(2) -C $$($(1)_download_dir)/$(1) && \
	cp $(3) $$($(1)_download_dir)/$(1)/Cargo.lock && \
	$$($(1)_download_dir)/native/bin/cargo vendor --locked --manifest-path $$($(1)_download_dir)/$(1)/$(4) $$($(1)_download_dir)/$(CRATE_REGISTRY) && \
	cd $$($(1)_download_dir) && \
	find $(CRATE_REGISTRY) | sort | tar --no-recursion -czf $$($(1)_download_dir)/$(5).temp -T - && \
    mv $$($(1)_download_dir)/$(5).temp $$($(1)_source_dir)/$(5) && \
    rm -rf $$($(1)_download_dir) ))
endef

# Calculates a SHA256 hash of all files that define a package's build recipe
# (meta files, package makefile, and patches).
# $(1) is the package name.
define int_get_build_recipe_hash
$(eval $(1)_all_file_checksums:=$(shell $(build_SHA256SUM) $(meta_depends) packages/$(1).mk $(addprefix $(PATCHES_PATH)/$(1)/,$($(1)_patches)) | cut -d" " -f1))
$(eval $(1)_recipe_hash:=$(shell echo -n "$($(1)_all_file_checksums)" | $(build_SHA256SUM) | cut -d" " -f1))
endef

# Generates a unique build ID for the package, incorporating versions and recipe hashes
# of all its recursive dependencies.
# $(1) is the package name.
define int_get_build_id
# 1. Add platform-specific dependencies (e.g., package_arm64_macos_dependencies)
$(eval $(1)_dependencies += $($(1)_$(host_arch)_$(host_os)_dependencies) $($(1)_$(host_os)_dependencies))
# 2. Get all recursive dependencies, including native toolchain components
$(eval $(1)_all_dependencies:=$(call int_get_all_dependencies,$(1),$($($(1)_type)_native_toolchain) $($($(1)_type)_native_binutils) $($(1)_dependencies)))
# 3. Create a list of dependency IDs (dep-version-hash)
$(foreach dep,$($(1)_all_dependencies),$(eval $(1)_build_id_deps+=$(dep)-$($(dep)_version)-$($(dep)_recipe_hash)))
# 4. Create the long, human-readable build ID string
$(eval $(1)_build_id_long:=$(1)-$($(1)_version)-$($(1)_recipe_hash)-$(release_type) $($(1)_build_id_deps))
# 5. Hash the long string to generate the final, truncated build ID
$(eval $(1)_build_id:=$(shell echo -n "$($(1)_build_id_long)" | $(build_SHA256SUM) | cut -c-$(HASH_LENGTH)))
final_build_id_long+=$($(package)_build_id_long)

# 6. Override platform-specific source file definitions
$(eval $(1)_file_name=$(if $($(1)_exact_file_name),$($(1)_exact_file_name),$(if $($(1)_file_name_$(host_arch)_$(host_os)),$($(1)_file_name_$(host_arch)_$(host_os)),$(if $($(1)_file_name_$(host_os)),$($(1)_file_name_$(host_os)),$($(1)_file_name)))))
$(eval $(1)_sha256_hash=$(if $($(1)_exact_sha256_hash),$($(1)_exact_sha256_hash),$(if $($(1)_sha256_hash_$(host_arch)_$(host_os)),$($(1)_sha256_hash_$(host_arch)_$(host_os)),$(if $($(1)_sha256_hash_$(host_os)),$($(1)_sha256_hash_$(host_os)),$($(1)_sha256_hash)))))
$(eval $(1)_download_file=$(if $($(1)_exact_download_file),$($(1)_exact_download_file),$(if $($(1)_download_file_$(host_arch)_$(host_os)),$($(1)_download_file_$(host_arch)_$(host_os)),$(if $($(1)_download_file_$(host_os)),$($(1)_download_file_$(host_os)),$(if $($(1)_download_file),$($(1)_download_file),$($(1)_file_name))))))
$(eval $(1)_download_path=$(if $($(1)_exact_download_path),$($(1)_exact_download_path),$(if $($(1)_download_path_$(host_arch)_$(host_os)),$($(1)_download_path_$(host_arch)_$(host_os)),$(if $($(1)_download_path_$(host_os)),$($(1)_download_path_$(host_os)),$($(1)_download_path)))))

# 7. Compute package-specific paths (build, source, staging, cache)
$(1)_build_subdir?=.
$(1)_source_dir:=$(SOURCES_PATH)
$(1)_source:=$$($(1)_source_dir)/$($(1)_file_name)
$(1)_staging_dir=$(base_staging_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_build_id)
$(1)_staging_prefix_dir:=$$($(1)_staging_dir)$($($(1)_type)_prefix)
$(1)_extract_dir:=$(base_build_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_build_id)
$(1)_download_dir:=$(base_download_dir)/$(1)-$($(1)_version)
$(1)_build_dir:=$$($(1)_extract_dir)/$$($(1)_build_subdir)
$(1)_cached_checksum:=$(BASE_CACHE)/$(host)/$(1)/$(1)-$($(1)_version)-$($(1)_build_id).tar.gz.hash
$(1)_patch_dir:=$(base_build_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_build_id)/.patches-$($(1)_build_id)
$(1)_prefixbin:=$($($(1)_type)_prefix)/bin/
$(1)_cached:=$(BASE_CACHE)/$(host)/$(1)/$(1)-$($(1)_version)-$($(1)_build_id).tar.gz
$(1)_all_sources=$($(1)_file_name) $($(1)_extra_sources)

# 8. Define timestamp files (stamps) for tracking build state
$(1)_fetched=$(SOURCES_PATH)/download-stamps/.stamp_fetched-$(1)-$($(1)_file_name).hash
$(1)_extracted=$$($(1)_extract_dir)/.stamp_extracted
$(1)_preprocessed=$$($(1)_extract_dir)/.stamp_preprocessed
$(1)_cleaned=$$($(1)_extract_dir)/.stamp_cleaned
$(1)_built=$$($(1)_build_dir)/.stamp_built
$(1)_configured=$$($(1)_build_dir)/.stamp_configured
$(1)_staged=$$($(1)_staging_dir)/.stamp_staged
$(1)_postprocessed=$$($(1)_staging_prefix_dir)/.stamp_postprocessed
# Fix download path for use in shell commands (escaping colons)
$(1)_download_path_fixed=$(subst :,\:,$$($(1)_download_path))


# 9. Define default commands for each step
# The default behavior for tar will try to set ownership when running as uid 0 and may not succeed.
# --no-same-owner disables this behavior, which is critical in containerized environments.
$(1)_fetch_cmds ?= $(call fetch_file,$(1),$(subst \:,:,$$($(1)_download_path_fixed)),$$($(1)_download_file),$($(1)_file_name),$($(1)_sha256_hash))
$(1)_extract_cmds ?= mkdir -p $$($(1)_extract_dir) && \
  echo "$$($(1)_sha256_hash)  $$($(1)_source)" > $$($(1)_extract_dir)/.$$($(1)_file_name).hash && \
  $(build_SHA256SUM) -c $$($(1)_extract_dir)/.$$($(1)_file_name).hash && \
  tar --no-same-owner --strip-components=1 -xf $$($(1)_source)
$(1)_preprocess_cmds ?=
$(1)_build_cmds ?=
$(1)_config_cmds ?=
$(1)_stage_cmds ?=
$(1)_set_vars ?=

# 10. Add the fetch stamp to the global list of source files to track
all_sources+=$$($(1)_fetched)
endef

# Attaches package-specific, architecture-specific, and release-specific
# configuration flags and environment variables.
# $(1) is the package name.
define int_config_attach_build_config
$(eval $(call $(1)_set_vars,$(1)))

# Append CFLAGS based on release type, host arch, host OS, and combinations
$(1)_cflags+=$($(1)_cflags_$(release_type))
$(1)_cflags+=$($(1)_cflags_$(host_arch)) $($(1)_cflags_$(host_arch)_$(release_type))
$(1)_cflags+=$($(1)_cflags_$(host_os)) $($(1)_cflags_$(host_os)_$(release_type))
$(1)_cflags+=$($(1)_cflags_$(host_arch)_$(host_os)) $($(1)_cflags_$(host_arch)_$(host_os)_$(release_type))

# Append CXXFLAGS
$(1)_cxxflags+=$($(1)_cxxflags_$(release_type))
$(1)_cxxflags+=$($(1)_cxxflags_$(host_arch)) $($(1)_cxxflags_$(host_arch)_$(release_type))
$(1)_cxxflags+=$($(1)_cxxflags_$(host_os)) $($(1)_cxxflags_$(host_os)_$(release_type))
$(1)_cxxflags+=$($(1)_cxxflags_$(host_arch)_$(host_os)) $($(1)_cxxflags_$(host_arch)_$(host_os)_$(release_type))

# Append CPPFLAGS
$(1)_cppflags+=$($(1)_cppflags_$(release_type))
$(1)_cppflags+=$($(1)_cppflags_$(host_arch)) $($(1)_cppflags_$(host_arch)_$(release_type))
$(1)_cppflags+=$($(1)_cppflags_$(host_os)) $($(1)_cppflags_$(host_os)_$(release_type))
$(1)_cppflags+=$($(1)_cppflags_$(host_arch)_$(host_os)) $($(1)_cppflags_$(host_arch)_$(host_os)_$(release_type))

# Append LDFLAGS
$(1)_ldflags+=$($(1)_ldflags_$(release_type))
$(1)_ldflags+=$($(1)_ldflags_$(host_arch)) $($(1)_ldflags_$(host_arch)_$(release_type))
$(1)_ldflags+=$($(1)_ldflags_$(host_os)) $($(1)_ldflags_$(host_os)_$(release_type))
$(1)_ldflags+=$($(1)_ldflags_$(host_arch)_$(host_os)) $($(1)_ldflags_$(host_arch)_$(host_os)_$(release_type))

# Append build options (e.g., specific flags passed directly to make)
$(1)_build_opts+=$$($(1)_build_opts_$(release_type))
$(1)_build_opts+=$$($(1)_build_opts_$(host_arch)) $$($(1)_build_opts_$(host_arch)_$(release_type))
$(1)_build_opts+=$$($(1)_build_opts_$(host_os)) $$($(1)_build_opts_$(host_os)_$(release_type))
$(1)_build_opts+=$$($(1)_build_opts_$(host_arch)_$(host_os)) $$($(1)_build_opts_$(host_arch)_$(host_os)_$(release_type))

# Append configuration options (e.g., flags passed to configure/cmake)
$(1)_config_opts+=$$($(1)_config_opts_$(release_type))
$(1)_config_opts+=$$($(1)_config_opts_$(host_arch)) $$($(1)_config_opts_$(host_arch)_$(release_type))
$(1)_config_opts+=$$($(1)_config_opts_$(host_os)) $$($(1)_config_opts_$(host_os)_$(release_type))
$(1)_config_opts+=$$($(1)_config_opts_$(host_arch)_$(host_os)) $$($(1)_config_opts_$(host_arch)_$(host_os)_$(release_type))

# Append configuration environment variables
$(1)_config_env+=$$($(1)_config_env_$(release_type))
$(1)_config_env+=$($(1)_config_env_$(host_arch)) $($(1)_config_env_$(host_arch)_$(release_type))
$(1)_config_env+=$($(1)_config_env_$(host_os)) $($(1)_config_env_$(host_os)_$(release_type))
$(1)_config_env+=$($(1)_config_env_$(host_arch)_$(host_os)) $($(1)_config_env_$(host_arch)_$(host_os)_$(release_type))

# Set standard environment variables for configuration
$(1)_config_env+=PKG_CONFIG_LIBDIR=$($($(1)_type)_prefix)/lib/pkgconfig
$(1)_config_env+=PKG_CONFIG_PATH=$($($(1)_type)_prefix)/share/pkgconfig
$(1)_config_env+=CMAKE_MODULE_PATH=$($($(1)_type)_prefix)/lib/cmake
$(1)_config_env+=PATH="$(build_prefix)/bin:$(PATH)"
# Set standard environment variables for build and stage
$(1)_build_env+=PATH="$(build_prefix)/bin:$(PATH)"
$(1)_stage_env+=PATH="$(build_prefix)/bin:$(PATH)"

# Default Autoconf configuration command template
$(1)_autoconf=./configure --host=$($($(1)_type)_host) --prefix=$($($(1)_type)_prefix) $$($(1)_config_opts) CC="$$($(1)_cc)" CXX="$$($(1)_cxx)"
ifneq ($($(1)_nm),)
$(1)_autoconf += NM="$$($(1)_nm)"
endif
ifneq ($($(1)_ranlib),)
$(1)_autoconf += RANLIB="$$($(1)_ranlib)"
endif
ifneq ($($(1)_ar),)
$(1)_autoconf += AR="$$($(1)_ar)"
endif
ifneq ($($(1)_cflags),)
$(1)_autoconf += CFLAGS="$$($(1)_cflags)"
endif
ifneq ($($(1)_cxxflags),)
$(1)_autoconf += CXXFLAGS="$$($(1)_cxxflags)"
endif
ifneq ($($(1)_cppflags),)
$(1)_autoconf += CPPFLAGS="$$($(1)_cppflags)"
endif
ifneq ($($(1)_ldflags),)
$(1)_autoconf += LDFLAGS="$$($(1)_ldflags)"
endif

# Default CMake configuration command template
$(1)_cmake=env CC="$$($(1)_cc)" \
               CFLAGS="$$($(1)_cppflags) $$($(1)_cflags)" \
               CXX="$$($(1)_cxx)" \
               CXXFLAGS="$$($(1)_cppflags) $$($(1)_cxxflags)" \
               LDFLAGS="$$($(1)_ldflags)" \
             $(build_prefix)/bin/cmake -DCMAKE_INSTALL_PREFIX:PATH="$$($($(1)_type)_prefix)"
ifeq ($($(1)_type),build)
# For 'build' tools, ensure RPATH points to the installed lib directory
$(1)_cmake += -DCMAKE_INSTALL_RPATH:PATH="$$($($(1)_type)_prefix)/lib"
else
# For 'host' packages, configure for cross-compilation
ifneq ($(host),$(build))
$(1)_cmake += -DCMAKE_SYSTEM_NAME=$($(host_os)_cmake_system)
$(1)_cmake += -DCMAKE_C_COMPILER_TARGET=$(host)
$(1)_cmake += -DCMAKE_CXX_COMPILER_TARGET=$(host)
endif
endif
endef

# Creates the phased build rules (targets and commands) for a package.
# This macro defines .stamp files to control the build workflow.
# $(1) is the package name.
define int_add_cmds
$($(1)_fetched):
	$(AT)echo "Fetching and verifying source for $(1)..."
	$(AT)mkdir -p $$(@D) $(SOURCES_PATH)
	$(AT)rm -f $$@
	$(AT)touch $$@
	$(AT)cd $$(@D); $(call $(1)_fetch_cmds,$(1))
	$(AT)cd $($(1)_source_dir); $(foreach source,$($(1)_all_sources),$(build_SHA256SUM) $(source) >> $$(@);)
	$(AT)touch $$@

$($(1)_extracted): | $($(1)_fetched)
	$(AT)echo "Extracting $(1)..."
	$(AT)mkdir -p $$(@D)
	$(AT)cd $$(@D); $(call $(1)_extract_cmds,$(1))
	$(AT)touch $$@

$($(1)_preprocessed): | $($(1)_dependencies) $($(1)_extracted)
	$(AT)echo "Preprocessing and applying patches for $(1)..."
	$(AT)mkdir -p $$(@D) $($(1)_patch_dir)
	$(AT)$(foreach patch,$($(1)_patches),cd $(PATCHES_PATH)/$(1); cp $(patch) $($(1)_patch_dir) ;)
	$(AT)cd $$(@D); $(call $(1)_preprocess_cmds, $(1))
	$(AT)touch $$@

$($(1)_configured): | $($(1)_preprocessed)
	$(AT)echo "Configuring $(1)..."
	# Unpack dependencies' cached artifacts into the host prefix for linkage
	$(AT)rm -rf $(host_prefix); mkdir -p $(host_prefix)/lib; cd $(host_prefix); $(foreach package,$($(1)_all_dependencies), tar --no-same-owner -xf $($(package)_cached); )
	$(AT)mkdir -p $$(@D)
	$(AT)+cd $$(@D); $($(1)_config_env) $(call $(1)_config_cmds, $(1))
	$(AT)touch $$@

$($(1)_built): | $($(1)_configured)
	$(AT)echo "Building $(1)..."
	$(AT)mkdir -p $$(@D)
	$(AT)+cd $$(@D); $($(1)_build_env) $(call $(1)_build_cmds, $(1))
	$(AT)touch $$@

$($(1)_staged): | $($(1)_built)
	$(AT)echo "Staging $(1) (installing)..."
	$(AT)mkdir -p $($(1)_staging_dir)/$(host_prefix)
	$(AT)cd $($(1)_build_dir); $($(1)_stage_env) $(call $(1)_stage_cmds, $(1))
	# Remove the temporary extraction/build directory after staging
	$(AT)rm -rf $($(1)_extract_dir)
	$(AT)touch $$@

$($(1)_postprocessed): | $($(1)_staged)
	$(AT)echo "Postprocessing $(1) (stripping, fixing permissions, etc.)..."
	$(AT)cd $($(1)_staging_prefix_dir); $(call $(1)_postprocess_cmds)
	$(AT)touch $$@

$($(1)_cached): | $($(1)_dependencies) $($(1)_postprocessed)
	$(AT)echo "Caching $(1) to artifact store..."
	# Create gzipped tarball from staged files, ensuring files are sorted and no recursion is used
	$(AT)cd $$($(1)_staging_dir)/$(host_prefix); find . | sort | tar --no-recursion -czf $$($(1)_staging_dir)/$$(@F) -T -
	# Move artifact to final cache location
	$(AT)mkdir -p $$(@D)
	$(AT)rm -rf $$(@D) && mkdir -p $$(@D)
	$(AT)mv $$($(1)_staging_dir)/$$(@F) $$(@)
	# Clean up temporary staging directory
	$(AT)rm -rf $($(1)_staging_dir)

$($(1)_cached_checksum): $($(1)_cached)
	$(AT)echo "Calculating final cache checksum for $(1)..."
	$(AT)cd $$(@D); $(build_SHA256SUM) $$(<F) > $$(@)

# Main package target depends on the final cached checksum file
.PHONY: $(1)
$(1): | $($(1)_cached_checksum)
# Secondary targets (stamps) are marked as intermediate files
.SECONDARY: $($(1)_cached) $($(1)_postprocessed) $($(1)_staged) $($(1)_built) $($(1)_configured) $($(1)_preprocessed) $($(1)_extracted) $($(1)_fetched)

endef

# ==============================================================================
# Execution Flow: Define, Configure, and Create Targets for All Packages
# ==============================================================================

# These functions set up the build targets for each package. They must be
# executed in specific order so that each step's output (like the build-id)
# is available globally before moving to the next phase.

# 1. Set the type for host/build packages.
$(foreach native_package,$(native_packages),$(eval $(native_package)_type=build))
$(foreach package,$(packages),$(eval $(package)_type=$(host_arch)_$(host_os)))

# 2. Set overridable defaults (toolchain, flags, etc.).
$(foreach package,$(all_packages),$(eval $(call int_vars,$(package))))

# 3. Include package configuration files (where package-specific variables are set).
$(foreach package,$(all_packages),$(eval include packages/$(package).mk))

# 4. Compute a hash of all files that comprise this package's build recipe (for caching).
$(foreach package,$(all_packages),$(eval $(call int_get_build_recipe_hash,$(package))))

# 5. Generate a unique build ID for this package, incorporating its recursive dependencies' hashes.
$(foreach package,$(all_packages),$(eval $(call int_get_build_id,$(package))))

# 6. Compute final, architecture-specific, and release-specific flags and environment variables.
$(foreach package,$(all_packages),$(eval $(call int_config_attach_build_config,$(package))))

# 7. Create the phased build targets (fetch, extract, configure, build, stage, cache).
$(foreach package,$(all_packages),$(eval $(call int_add_cmds,$(package))))

# 8. Special exception: all non-native packages depend on the native toolchain artifacts.
$(foreach package,$(packages),$(eval $($(package)_unpacked): |$($($(host_arch)_$(host_os)_native_toolchain)_cached) $($($(host_arch)_$(host_os)_native_binutils)_cached) ))
