with import ./../../pkgs-pinned.nix;
{
  # Given a list of "selections" return the flattened result of selecting
  # them by hostPlatform details. This is used heavily for example to
  # select ./configure options by hostPlatform.
  selectByHostPlatform = lib.lists.concatMap (elem:
    with hostPlatform.parsed;
    let
      hostCPU = cpu.name;
      hostVendor = vendor.name;
      hostKernel = kernel.name;
      hostABI = abi.name;

      tj = builtins.toJSON;

      select = ({ vendor ? hostVendor, cpu ? hostCPU, kernel ? hostKernel, abi ? hostABI, values }:
        let
          mismatched = (
            candidate: target:
            !(builtins.isNull candidate || candidate == target)
          );
        in if mismatched vendor hostVendor
        then []
        else if mismatched cpu hostCPU
        then []
        else if mismatched kernel hostKernel
        then []
        else if mismatched abi hostABI
        then []
        # Everything matches:
        else values
      );
    in select (
      if builtins.isList elem # A list is shorthand for all platforms:
      then { values = elem; }
      else elem
    )
  );
}
