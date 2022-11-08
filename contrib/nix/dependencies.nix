# This overlay contains all the things that we do to upstream libraries to work
# for us.
final: previous: {
  ctaes =
    final.runCommand "ctaes" {
      src = final.fetchFromGitHub {
        owner = "bitcoin-core";
        repo = "ctaes";
        rev = "8012b062ea4931f10cc2fd2075fddc3782a57ee4";
        sha256 = "sha256-/uQQLO7ZKnimap4ual/dHNwd4nTvoo4+3qZ/HqjxLuA=";
      };
    } ''
      mkdir -p $out
      cp $src/ctaes.c $src/ctaes.h $out/
    '';

  leveldb =
    (previous.leveldb.override {
      static = true;
    })
    .overrideAttrs (old: {
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
      postInstall =
        old.postInstall
        + ''
          cp $src/helpers/memenv/memenv.h $dev/include
        '';
    });

  libsodium = previous.libsodium.overrideAttrs (old: {
    patches =
      old.patches
      ++ [
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

  tinyformat = final.stdenv.mkDerivation {
    pname = "tinyformat";
    version = "2.3.0";

    src = final.fetchFromGitHub {
      owner = "c42f";
      repo = "tinyformat";
      rev = "aef402d85c1e8f9bf491b72570bfe8938ae26727";
      sha256 = "sha256-Ka7fp5ZviTMgCXHdS/OKq+P871iYqoDOsj8HtJGAU3Y=";
    };

    patches = [./patches/tinyformat/bitcoin.patch];

    # ./patches/tinyformat/bitcoin.patch introduces C++11 features.
    CXXFLAGS = "-std=c++11";

    installPhase = ''
      runHook preInstall
      mkdir -p $out
      cp tinyformat.h $out/
      runHook postInstall
    '';
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

    nativeBuildInputs = [final.autoreconfHook];
  };

  zeromq = previous.zeromq.overrideAttrs (old: {
    patches = [../../depends/patches/zeromq/windows-unused-variables.diff];
  });
}
