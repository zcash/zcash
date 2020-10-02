# Trace a value to debug log, converted to JSON, then return it.
v: (import ./nixpkgs.nix).lib.debug.traceSeq (builtins.toJSON v) v
