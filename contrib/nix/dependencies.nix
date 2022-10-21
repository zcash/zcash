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

  univalue = final.stdenv.mkDerivation {
    pname = "univalue";
    version = "1.0.3";
    src = final.fetchFromGitHub {
      owner = "bitcoin-core";
      repo = "univalue-subtree";
      rev = "98fadc090984fa7e070b6c41ccb514f69a371c85";
      sha256 = "sha256-MyV+KBuGLRtgJB9rB62x+D1b1cpy4RxJmXI2SBocxZI=";
    };

    nativeBuildInputs = [ final.autoreconfHook ];
  };

  zeromq = previous.zeromq.overrideAttrs (old: {
    patches = [ ../../depends/patches/zeromq/windows-unused-variables.diff ];
  });
}
