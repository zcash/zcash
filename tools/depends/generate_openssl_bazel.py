"""Generates a BUILD.bazel file suitable for ZCash given an openssl source
directory. Run this script in the openssl source directory. It creates a
BUILD.bazel file there.
"""

import os
import subprocess
import generator_util

prefix = os.getcwd() + "/prefix"

openssl_config_opts = [
    "--prefix=" + prefix,
    "--openssldir=" + prefix + "/etc/openssl",
    "no-afalgeng",
    "no-asm",
    "no-async",
    "no-bf",
    "no-blake2",
    "no-camellia",
    "no-capieng",
    "no-cast",
    "no-chacha",
    "no-cmac",
    "no-cms",
    "no-comp",
    "no-crypto-mdebug",
    "no-crypto-mdebug-backtrace",
    "no-ct",
    "no-des",
    "no-dgram",
    "no-dsa",
    "no-dso",
    "no-dtls",
    "no-dtls1",
    "no-dtls1-method",
    "no-dynamic-engine",
    "no-ec2m",
    "no-ec_nistp_64_gcc_128",
    "no-egd",
    "no-engine",
    "no-err",
    "no-gost",
    "no-heartbeats",
    "no-idea",
    "no-md2",
    "no-md4",
    "no-mdc2",
    "no-multiblock",
    "no-nextprotoneg",
    "no-ocb",
    "no-ocsp",
    "no-poly1305",
    "no-posix-io",
    "no-psk",
    "no-rc2",
    "no-rc4",
    "no-rc5",
    "no-rdrand",
    "no-rfc3779",
    "no-rmd160",
    "no-scrypt",
    "no-sctp",
    "no-seed",
    "no-shared",
    "no-sock",
    "no-srp",
    "no-srtp",
    "no-ssl",
    "no-ssl3",
    "no-ssl3-method",
    "no-ssl-trace",
    "no-stdio",
    "no-tls",
    "no-tls1",
    "no-tls1-method",
    "no-ts",
    "no-ui",
    "no-unit-test",
    "no-weak-ssl-ciphers",
    "no-whirlpool",
    "no-zlib",
    "no-zlib-dynamic",
    "-DPURIFY"]

# This is a build file written in Perl, executed by the OpenSSL config script
BAZEL_tmpl_contents = generator_util.build_header()
BAZEL_tmpl_contents += r"""
## {- join("\n## ", @autowarntext) -}
{-
     use Text::ParseWords ();

     sub shell_string_to_skylark_array {
         return to_skylark_array(Text::ParseWords::shellwords(@_));
     }

     sub to_skylark_array {
         return "[".join(",", map { "\"$_\"" } @_)."]";
     }

     '';
-}

# Parameters used to generate the file
openssl_platform = "{- $config{target} -}"
configure_args = [{- join(", ",quotify_l(@{$config{perlargv}})) -}]
srcdir = "{- $config{sourcedir} -}"

version = "{- $config{version} -}"
major = "{- $config{major} -}"
minor = "{- $config{minor} -}"

cflags = \
  {- shell_string_to_skylark_array(join(" ",(map { "-D".$_} @{$target{defines}}, @{$config{defines}}))) -} + \
  {- to_skylark_array(grep(!/^(-O\d)|(-m64)$/, Text::ParseWords::shellwords($target{cflags}))) -} + \
  {- shell_string_to_skylark_array($config{cflags}) -} + \
  [
    "-DOPENSSLDIR=\\\"apps\\\"",
    "-DENGINESDIR=\\\"engines\\\"",
    "-DNO_WINDOWS_BRAINDEATH",  # Makes cversion.c not include buildinf.h, which we don't want anyway
  ]
ldflags = {- shell_string_to_skylark_array($target{lflags}) -}

lib_cflags = cflags + ["-fPIC"]
lib_ldflags = ldflags
bin_cflags = cflags + {- shell_string_to_skylark_array($target{bin_cflags}) -}
bin_ldflags = ldflags

configdata_contents = {-
  my $configdata_contents = do {
      open( my $fh, "configdata.pm" ) or die "Can't open configdata.pm: $!";
      local $/ = undef;
      <$fh>;
  };

  "r\"\"\"$configdata_contents\"\"\""
-}
genrule(
    name = "configdata",
    outs = ["configdata.pm"],
    cmd = "cat > \$@ << 'BAZEL_EOF'\n" + configdata_contents.replace("$", "$$") + "\nBAZEL_EOF",
)

{-
  use File::Basename;
  use File::Spec::Functions qw/:DEFAULT abs2rel rel2abs/;

  # Helper function to figure out dependencies on libraries
  # It takes a list of library names and outputs a list of dependencies
  our $libext = $target{lib_extension} || ".a";
  sub compute_lib_depends {
      return map { $_.$libext } @_;
  }

  sub generatesrc {
      my %args = @_;
      my $deps = join(" ", @{$args{generator_deps}}, @{$args{deps}});

      if ($args{src} =~ /^test\//) {
          return "";
      }
      if ($args{src} =~ /^apps\//) {
          return "";
      }
      if ($args{src} =~ /\.[sS]$/) {
          if ($args{generator}->[0] !~ /\.pl$/) {
              die "Generator type for $args{src} unknown: $generator\n";
          }
          return <<"EOF";
genrule(
    name = "$args{src}-genrule",
    srcs = [
        "$args{generator}->[0]",
        "crypto/perlasm/x86_64-xlate.pl",
        "crypto/ec/ecp_nistz256_table.c",
    ],
    outs = ["$args{src}"],
    cmd = "/usr/bin/perl \$(location $args{generator}->[0]) $target{perlasm_scheme} \\\"\$@\\\"",
)
EOF
      } elsif ($args{src} =~ /\.h$/) {
          return <<"EOF";
genrule(
    name = "$args{src}-genrule",
    outs = ["$args{src}"],
    # Disable sandboxing, due to dependencies to files in the directory
    # called external, which Bazel cannot handle.
    local = 1,
    srcs = [
        "$args{generator}->[0]",
        "util/dofile.pl",
        "configdata.pm",
    ],
    cmd = "PERLLIB=\\\"\$\$(dirname \$(location configdata.pm)):external/perl/perl/lib\\\" /usr/bin/perl -Mconfigdata \$(location util/dofile.pl) \$(location $args{generator}->[0]) > \\\"\$@\\\"",
)
EOF
      } else {
          die "Generator type for $args{src} unknown\n";
      }
  }

  sub src2obj {
      return "";
  }
  sub libobj2shlib {
      return "";
  }
  sub obj2dso {
      return "";
  }
  sub obj2lib {
      my %args = @_;
      (my $lib = $args{lib}) =~ s/^lib//;
      my $srcs = "";
      foreach (@{$unified_info{sources}->{$args{lib}}}) {
          foreach (@{$unified_info{sources}->{$_}}) {
              $srcs .= "\n        \"$_\",";
          }
      }
      my $deps = join(", ", map { (my $l = $_) =~ s/^lib//; "\":$l\"" } reducedepends(resolvedepends($args{lib})));
      # TODO(per-gron): This is not very nice; it assumes that the package is
      # being imported with a given name.
      my $external_dir = "external/openssl";
      # Avoid exporting the same header files in multiple targets.
      my $header_glob = $lib eq "crypto" ? "glob([\"include/**/*.h\"])" : "[]";
      return <<"EOF";
cc_library(
    name = "$lib",
    copts = lib_cflags + [
        "-I$external_dir/include",
        "-I$external_dir/crypto/include",
        "-I$external_dir/crypto/modes",
        "-I$external_dir/.",
        "-I\$(GENDIR)/$external_dir/include",
        "-I\$(GENDIR)/$external_dir/crypto/include",
        "-I\$(GENDIR)/$external_dir/crypto",
        "-Wno-maybe-uninitialized",
    ],
    linkopts = lib_ldflags,
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = $header_glob + [
        # Needs to be declared separately because it's generated by a genrule.
        "include/openssl/opensslconf.h",
    ],
    textual_hdrs = glob(["crypto/des/ncbc_enc.c", "crypto/**/LPdir_*.c"]),
    srcs = glob(["$lib/**/*.h", "*.h"]) + [$srcs
        # TODO(per-gron): Don't hard code these
        "crypto/include/internal/bn_conf.h",
        "crypto/include/internal/dso_conf.h",
    ],
    deps = [$deps]
)
EOF
  }
  sub obj2bin {
      # Unimplemented
      return "";
  }
  sub in2script {
      return "";
  }
  sub generatedir {
      return "";
  }
  ""    # Important!  This becomes part of the template result.
-}
"""

generator_util.write_file("Configurations/BUILD.bazel.tmpl", BAZEL_tmpl_contents)

os.environ["BUILDFILE"] = "BUILD.bazel"  # For the config script
subprocess.call(["./config"] + openssl_config_opts)
