def _fetch_sprout_params_impl(repository_ctx):
    repository_ctx.download(
        "https://z.cash/downloads/sprout-proving.key",
        output = 'sprout-proving.key',
        sha256 = '8bc20a7f013b2b58970cddd2e7ea028975c88ae7ceb9259a5344a16bc2c0eef7',
    )
    repository_ctx.download(
        "https://z.cash/downloads/sprout-verifying.key",
        output = 'sprout-verifying.key',
        sha256 = '4bd498dae0aacfd8e98dc306338d017d9c08dd0918ead18172bd0aec2fc5df82',
    )
    repository_ctx.file("BUILD.bazel", content=r"""
filegroup(
    name = "sprout_proving_key",
    visibility = ["//visibility:public"],
    srcs = [
        "sprout-proving.key",
    ],
)

filegroup(
    name = "sprout_verifying_key",
    visibility = ["//visibility:public"],
    srcs = [
        "sprout-verifying.key",
    ],
)

filegroup(
    name = "sprout_params",
    visibility = ["//visibility:public"],
    srcs = [
        ":sprout_proving_key",
        ":sprout_verifying_key",
    ],
)
""")

fetch_sprout_params = repository_rule(
    implementation = _fetch_sprout_params_impl,
)
