# This overlay contains all the things that we do to upstream libraries to work
# for us.

final: previous: {
  libsodium = previous.libsodium.overrideAttrs (old: {
    patches = old.patches ++ [
      ../../depends/patches/libsodium/1.0.15-pubkey-validation.diff
      ../../depends/patches/libsodium/1.0.15-signature-validation.diff
    ];
  });

  llvmPackages = final.llvmPackages_14;

  zeromq = previous.zeromq.overrideAttrs (old: {
    patches = [ ../../depends/patches/zeromq/windows-unused-variables.diff ];
  });
}
