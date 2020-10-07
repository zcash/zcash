# Given parsed nix packages, print out a set of consistency failures,
# then return a success/failure bool.
#
# WARNING! This checks *only* for dependency sha256 source matches. It
# does not check any build flags, patches, etc which could be critical
# to safety/correctness.
let
  inherit (builtins) attrNames;
  inherit (import ../../util) nixpkgs;

  parsedDependsPackages = import ./parseDependsPackages.nix;

  uniqueNames = srcAttrs: attrsToRemove:
    let
      removeNames = attrNames attrsToRemove;
      uniqueAttrs = removeAttrs srcAttrs removeNames;
    in
      attrNames uniqueAttrs;

in
  nixPackages:
    let
      # Note we deviate from the normal naming convention for readability
      # of the JSON output:
      inconsistencies = {
        packages_missing_from_nix = uniqueNames parsedDependsPackages nixPackages;
        unexpected_nix_packages = uniqueNames nixPackages parsedDependsPackages;
        inconsistent_sha256_hashes = 
          let
            inherit (builtins) getAttr intersectAttrs;
            subdeps = intersectAttrs nixPackages parsedDependsPackages;
            checkHash = name: makefileHash:
              let
                nixPkg = getAttr name nixPackages;
              in
                if makefileHash != nixPkg.sha256
                then {
                  depends = makefileHash;
                  nix = nixPkg.sha256;
                }
                else null;

            inherit (builtins) mapAttrs;
            inherit (nixpkgs.lib.attrsets) filterAttrs;
          in
            filterAttrs (_: v: v != null) (mapAttrs checkHash subdeps);
      };

      inherit (builtins) length;
    in
      if (
        length inconsistencies.packages_missing_from_nix == 0 ||
        length inconsistencies.unexpected_nix_packages == 0 ||
        length (attrNames inconsistencies.inconsistent_sha256_hashes) == 0
      )
      then true
      else
        let
          inherit (builtins) toJSON;
          inherit (nixpkgs.lib.debug) traceSeq;
          errorMsg = toJSON { inherit inconsistencies; };
        in
          traceSeq errorMsg false
