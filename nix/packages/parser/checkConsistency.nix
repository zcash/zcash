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
  { nixPackages, allowInconsistency ? false }:
    let
      pkgsMissing = uniqueNames parsedDependsPackages nixPackages;
      pkgsUnexpected = uniqueNames nixPackages parsedDependsPackages;
      hashMismatches =
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

      inherit (builtins) length;
    in
      if (
        length pkgsMissing == 0 &&
        length pkgsUnexpected == 0 &&
        length (attrNames hashMismatches) == 0
      )
      then true
      else
        let
          inherit (builtins) toJSON;
          inherit (nixpkgs.lib.debug) traceSeq;

          errorMsgHashes =
            if length (attrNames hashMismatches) == 0
            then
              "Note: All existing packages have matching hashes."
            else
              "Packages with mismatched sha256 hashes:\n" + (
                let
                  inherit (nixpkgs.lib.attrsets) mapAttrsToList;
                  inherit (nixpkgs.lib.strings) concatStrings;
                  describeMismatch = n: v: "  ${n}:\n    depends ${v.depends}\n    vs nix  ${v.nix}\n";
                in
                  concatStrings (mapAttrsToList describeMismatch hashMismatches)
              );

          errorMsg = ''
            FAILED CONSISTENCY CHECKS between ./depends and ./nix packages:

            Missing nix packages: ${toJSON pkgsMissing}
            Unexpected nix packages: ${toJSON pkgsUnexpected}
            ${errorMsgHashes}

            ${if allowInconsistency
              then "WARNING: continuing anyway due to '--arg allowInconsistency true'..."
              else ""
            }
          '';
        in
          traceSeq errorMsg allowInconsistency
