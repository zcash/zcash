with import ./../../../pkgs-pinned.nix;
let
  reporoot = ./../../../../..;
  cargoinfo =
    with builtins;
    let
      cargopath = "${reporoot}/Cargo.toml";
      toml = fromTOML (readFile cargopath);
    in toml.package;
in
  rustPlatform.buildRustPackage rec {
    pname = cargoinfo.name;
    version = cargoinfo.version;
    src = reporoot;
    cargoSha256 = "sha256:1n2gl25qfw1q9my9qdg3y98vpsiqn06pmza0dhg1y0f7shri67xs";
    verifyCargoDeps = true;

    meta = with stdenv.lib; {
      description = cargoinfo.description;
      homepage = cargoinfo.homepage;
      license = cargoinfo.license;
      maintainers = [ maintainers.tailhook ];
      platforms = platforms.all;
    };
  }
