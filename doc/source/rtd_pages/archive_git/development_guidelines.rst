.. _development_guidelines:


Development Guidelines
======================



Code Management
---------------

We achieve our design goals primarily through this codebase as a
reference implementation. This repository is a fork of `Bitcoin Core`_
as of upstream release 0.11.2 (many later Bitcoin PRs have also been
ported to Zcash). It implements the `Zcash protocol`_ and a few other
distinct features.

`Bitcoin Core`: https://github.com/bitcoin/bitcoin
`Zcash protocol`: https://github.com/zcash/zips/blob/master/protocol/protocol.pdf

Testing
-------

Add unit tests for zcash under ``./src/gtest``. 

To list all tests, run ``./src/zcash-gtest --gtest_list_tests``.

To run a subset of tests, use a regular expression with the flag ``--gtest_filter``. Example:

```
./src/zcash-gtest --gtest_filter=DeprecationTest.*
```

For debugging: ``--gtest_break_on_failure``.

To run a subset of BOOST tests:
```
src/test/test_bitcoin -t TESTGROUP/TESTNAME
```

Continuous Integration
----------------------

Watch the `Buildbot`_. Because the Buildbot is watching you.

Buildbot: https://ci.z.cash/


Zkbot
-----

We use a homu instance called ``zkbot`` to merge *all* PRs. (Direct pushing to the "master" branch of the repo is not allowed.) Here's just a quick overview of how it works.

If you're on our team, you can do ``@zkbot <command>`` to tell zkbot to do things. Here are a few examples:

* ``r+ [commithash]`` this will test the merge and then actually commit the merge into the repo if the tests succeed.
* ``try`` this will test the merge and nothing else.
* ``rollup`` this is like ``r+`` but for insignificant changes. Use this when we want to test a bunch of merges at once to save buildbot time.

More instructions are found here: http://ci.z.cash:12477/

Workflow
--------

1. Fork our repository
2. Create a new branch with your changes
3. Submit a pull request


Releases
--------

Starting from Zcash v1.0.0-beta1, Zcash version numbers and release tags take one of the following forms:


    v<X>.<Y>.<Z>-beta<N>
    v<X>.<Y>.<Z>-rc<N>
    v<X>.<Y>.<Z>
    v<X>.<Y>.<Z>-<N>

Alpha releases used a different convention: ``v0.11.2.z<N>`` (because Zcash was forked from Bitcoin v0.11.2).


Commit messages
---------------

Commit messages should contain enough information in the first line to be able to scan a list of patches and identify which one is being searched for. Do not use "auto-close" keywords -- tickets should be closed manually. The auto-close keywords are "close[ds]", "resolve[ds]", and "fix(e[ds])?".


Misc Notes
----------

These are historical, probably bit-rotted, but also probably full of
important nuggets:

* `Notes from the Boulder Hack Fest <https://github.com/Electric-Coin-Company/design-docs/blob/master/20141119-ZECC-meeting-notes.txt>`_
* `zdep Google group <https://groups.google.com/forum/#!forum/zdep>`_ (requires login)
