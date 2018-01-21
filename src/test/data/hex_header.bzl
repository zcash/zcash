def json_hex_header(name, src, visibility = None):
    hdr = "%s.h" % src

    native.genrule(
        name = "%s_header" % name,
        srcs = [src],
        outs = [hdr],
        # With the hexdump that is in most Linuxes, -v has to be passed. But here
        # a version of hexdump that doesn't support it and always behaves as if
        # it is passed is used.
        cmd = r"""(echo "namespace json_tests{" && echo "static unsigned const char $$(basename "$@" | cut -f 1 -d '.')[] = {" && $(location @hexdump//:hexdump) -e '8/1 "0x%02x, " "\n"' $(location """ + src + r""") | sed -e 's:0x  ,::g' && echo "};};") > $@""",
        tools = ["@hexdump//:hexdump"],
    )

    native.cc_library(
        name = name,
        visibility = visibility,
        hdrs = [hdr],
        includes = ["."],
        include_prefix = "test/data",
    )

def raw_hex_header(name, src, visibility = None):
    hdr = "%s.h" % src

    native.genrule(
        name = "%s_header" % name,
        srcs = [src],
        outs = [hdr],
        # With the hexdump that is in most Linuxes, -v has to be passed. But here
        # a version of hexdump that doesn't support it and always behaves as if
        # it is passed is used.
        cmd = r"""(echo "namespace alert_tests{" && echo "static unsigned const char $$(basename "$@" | cut -f 1 -d '.')[] = {" && $(location @hexdump//:hexdump) -e '8/1 "0x%02x, " "\n"' $(location """ + src + r""") | sed -e 's:0x  ,::g' && echo "};};") > $@""",
        tools = ["@hexdump//:hexdump"],
    )

    native.cc_library(
        name = name,
        visibility = visibility,
        hdrs = [hdr],
        includes = ["."],
        include_prefix = "test/data",
    )
