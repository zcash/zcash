# Platform Tier Policy

## General

ECC provides three tiers of platform support, modeled after the
[Rust Target Tier Policy](https://doc.rust-lang.org/stable/rustc/platform-tier-policy.html):

- The Zcash developers provide no guarantees about tier 3 platforms; they exist in the
  codebase, but may or may not build.
- ECC's continuous integration checks that tier 2 platforms will always build, but they
  may or may not pass tests.
- ECC's continuous integration checks that tier 1 platforms will always build and pass
  tests.

Adding a new tier 3 platform imposes minimal requirements; we focus primarily on avoiding
disruption to other ongoing Zcash development.

Tier 2 and tier 1 platforms place work on Zcash developers as a whole, to avoid breaking
the platform. The broader Zcash community may also feel more inclined to support
higher-tier platforms in their downstream uses of `zcashd` (though they are not obligated
to do so). Thus, these tiers require commensurate and ongoing efforts from the maintainers
of the platform, to demonstrate value and to minimize any disruptions to ongoing Zcash
development.

This policy defines the requirements for accepting a proposed platform at a given level of
support.

Each tier builds on all the requirements from the previous tier, unless overridden by a
stronger requirement.

While these criteria attempt to document the policy, that policy still involves human
judgment. Platforms must fulfill the spirit of the requirements as well, as determined by
the judgment of the approving reviewers. Reviewers and team members evaluating platforms
and platform-specific patches should always use their own best judgment regarding the
quality of work, and the suitability of a platform for the Zcash project. Neither this
policy nor any decisions made regarding platforms shall create any binding agreement or
estoppel by any party.

For a list of all supported platforms and their corresponding tiers ("tier 3", "tier 2",
or "tier 1"), see [Platform Support](../user/platform-support.md).

The availability or tier of a platform in Zcash releases is not a hard stability guarantee
about the future availability or tier of that platform. Higher-level platform tiers are an
increasing commitment to the support of a platform, and we will take that commitment and
potential disruptions into account when evaluating the potential demotion or removal of a
platform that has been part of a stable release. The promotion or demotion of a platform
will not generally affect existing stable releases, only current development and future
releases.

In this policy, the words "MUST" and "MUST NOT" specify absolute requirements that a
platform must meet to qualify for a tier. The words "SHOULD" and "SHOULD NOT" specify
requirements that apply in almost all cases, but for which the approving teams may grant
an exception for good reason. The word "MAY" indicates something entirely optional, and
does not indicate guidance or recommendations. This language is based on
[IETF RFC 2119](https://tools.ietf.org/html/rfc2119).

## Tier 3 platform policy

At this tier, ECC provides no official support for a platform, so we place minimal
requirements on the introduction of platforms.

A proposed new tier 3 platform MUST be reviewed and approved by a member of the ECC core
team based on these requirements.

- The platform MUST provide documentation for the Zcash community explaining how to build
  for the platform, using cross-compilation if possible. If the platform supports running
  binaries, or running tests (even if they do not pass), the documentation MUST explain
  how to run such binaries or tests for the platform, using emulation if possible or
  dedicated hardware if necessary.
- Tier 3 platforms MUST NOT impose burden on the authors of pull requests, or other
  developers in the community, to maintain the platform. In particular, do not post
  comments (automated or manual) on a PR that derail or suggest a block on the PR based on
  a tier 3 platform. Do not send automated messages or notifications (via any medium,
  including via @) to a PR author or others involved with a PR regarding a tier 3
  platform, unless they have opted into such messages.
- Patches adding or updating tier 3 platforms MUST NOT break any existing tier 2 or tier 1
  platform, and MUST NOT knowingly break another tier 3 platform without approval of the
  ECC core team.

If a tier 3 platform stops meeting these requirements, or the platform shows no signs of
activity and has not built for some time, or removing the platform would improve the
quality of the Zcash codebase, we MAY post a PR to remove it.

## Tier 2 platform policy

At this tier, the Zcash developers guarantee that a platform builds, and will reject
patches that fail to build for a platform. Thus, we place requirements that ensure the
platform will not block forward progress of the Zcash project.

A proposed new tier 2 platform MUST be reviewed and approved by the ECC core team based
on these requirements.

In addition, the ECC infrastructure team MUST approve the integration of the platform into
Continuous Integration (CI), and the tier 2 CI-related requirements. This review and
approval MAY take place in a PR adding the platform to CI, or simply by an infrastructure
team member reporting the outcome of a team discussion.

- A tier 2 platform MUST have value to people other than its proponents. (It MAY still be
  a niche platform, but it MUST NOT be exclusively useful for an inherently closed group.)
- A tier 2 platform MUST have a designated team of developers (the "platform maintainers")
  supporting it, without the need for a paid support contract.
- The platform MUST NOT place undue burden on Zcash developers not specifically concerned
  with that platform. Zcash developers are expected to not gratuitously break a tier 2
  platform, but are not expected to become experts in every tier 2 platform, and are not
  expected to provide platform-specific implementations for every tier 2 platform.
- The platform MUST build reliably in CI, for all components that ECC's CI considers
  mandatory.
- All requirements for tier 3 apply.

A tier 2 platform MAY be demoted or removed if it no longer meets these requirements.

## Tier 1 platform policy

At this tier, the Zcash developers guarantee that a platform builds and passes all tests,
and will reject patches that fail to build or pass the test suite on a platform. We hold
tier 1 platforms to our highest standard of requirements.

- Tier 1 platforms MUST have substantial, widespread interest within the Zcash community,
  and MUST serve the ongoing needs of multiple production users of Zcash across multiple
  organizations or projects. These requirements are subjective. A tier 1 platform MAY be
  demoted or removed if it becomes obsolete or no longer meets this requirement.
- The platform MUST build and pass tests reliably in CI, for all components that ECC's CI
  considers mandatory.
- Building the platform and running the test suite for the platform MUST NOT take
  substantially longer than other platforms, and SHOULD NOT substantially raise the
  maintenance burden of the CI infrastructure.
- Tier 1 platforms MUST NOT have a hard requirement for signed, verified, or otherwise
  "approved" binaries. Developers MUST be able to build, run, and test binaries for the
  platform on systems they control, or provide such binaries for others to run. (Doing so
  MAY require enabling some appropriate "developer mode" on such systems, but MUST NOT
  require the payment of any additional fee or other consideration, or agreement to any
  onerous legal agreements.)
- All requirements for tier 2 apply.

A tier 1 platform MAY be demoted or removed if it no longer meets these requirements but
still meets the requirements for a lower tier.
