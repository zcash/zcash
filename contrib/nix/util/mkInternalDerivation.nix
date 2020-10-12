# Create an "internal derivation".
let
  inherit (import ./nixpkgs.nix) writeScript;
  inherit (import ./config.nix) zcash;
  version = import ../version.nix;
  zcstdenv = import ./zcstdenv.nix;

  mkName = subname: "${zcash.pname}-${version.string}.${subname}";
in
  {
    subname,
    builder,

    # These should all be null:
    name ? null,
    pname ? null,
    version ? null,

    ...
  } @ args:
    assert name == null;
    assert pname == null;
    assert version == null;
    zcstdenv.mkDerivation (args // {
      name = mkName subname;
      builder =
        if builtins.isString builder
        then writeScript (mkName "${subname}-builder.sh") builder
        else
          assert builtins.isPath builder;
          builder;
    })
