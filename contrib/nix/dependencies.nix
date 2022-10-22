# This overlay contains all the things that we do to upstream libraries to work
# for us.
final: previous: {
  leveldb = (previous.leveldb.override {
    static = true;
  }).overrideAttrs (old: {
    version = "1.22.0";

    # Replace upstream with Bitcoin’s subtree fork, pinned to the same revision
    # as the “depends/” build.
    src = final.fetchFromGitHub {
      owner = "bitcoin-core";
      repo = "leveldb-subtree";
      rev = "f8ae182c1e5176d12e816fb2217ae33a5472fdd7";
      sha256 = "sha256-CYy9+8YrWO4ZsdKFOyrK5TnwyWqwLjoJLgAyEndl3jc=";
    };

    # We rely on the internal memenv library. It seems like the original
    # upstream _may_ have fixed this issue? See
    # https://github.com/google/leveldb/issues/236
    postInstall = old.postInstall + ''
      cp $src/helpers/memenv/memenv.h $dev/include
    '';
  });

  libsodium = previous.libsodium.overrideAttrs (old: {
    patches = old.patches ++ [
      ../../depends/patches/libsodium/1.0.15-pubkey-validation.diff
      ../../depends/patches/libsodium/1.0.15-signature-validation.diff
    ];
  });

  llvmPackages = final.llvmPackages_14;

  python = previous.python3;

  pythonPackages = previous.python3Packages;

  snappy = previous.snappy.override {
    static = true;
  };

  univalue = final.stdenv.mkDerivation {
    pname = "univalue";
    version = "1.0.3";
    src = final.fetchFromGitHub {
      owner = "bitcoin-core";
      repo = "univalue-subtree";
      # Pinned to the same revision as the “depends/” build.
      rev = "98fadc090984fa7e070b6c41ccb514f69a371c85";
      sha256 = "sha256-MyV+KBuGLRtgJB9rB62x+D1b1cpy4RxJmXI2SBocxZI=";
    };

    nativeBuildInputs = [ final.autoreconfHook ];
  };

  zeromq = previous.zeromq.overrideAttrs (old: {
    patches = [ ../../depends/patches/zeromq/windows-unused-variables.diff ];
  });
}
