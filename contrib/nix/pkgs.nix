/*
NIXPKGS version
Any archive of nixpkgs can be used.
The simplest update solution is to look at
http://github.com/NixOS/nixpkgs and pick the latest commit for
the desired branch. The archive can then be fetched at:
https://github.com/NixOS/nixpkgs/archive/COMMIT_NUMBER.tar.gz;
and the control sum computed using `sha256`.
*/

let
  # we need unstable because some packages have been added recently
  sha256 = "1q0dqy3j13axgz9rqbwvfi989vh84x6fc1y6am2schb3yh31kp9n";
  rev = "e858db900443414fd8dd2f78c3ecb47df3febc44";
in
import (fetchTarball {
  inherit sha256;
  url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
})
