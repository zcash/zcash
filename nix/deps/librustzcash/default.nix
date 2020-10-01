let
  inherit (import ./../../util) nixpkgs importTOML srcDir;
  inherit (nixpkgs) stdenv;
  cargoinfo = (importTOML (srcDir + "/Cargo.toml").package;
in
  rustPlatform.buildRustPackage rec {
    pname = cargoinfo.name;
    version = cargoinfo.version;
    src = srcDir;
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
