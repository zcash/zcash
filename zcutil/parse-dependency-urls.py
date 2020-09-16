#!/usr/bin/env python3
"""
Parse dependency name, version, url, and sha256 from `./depends` makefiles.
"""

import re
import sys
import json
from pathlib import Path

SKIP = ['packages.mk', 'vendorcrate.mk']

def main():
    result = []

    zcutil = Path(sys.argv[0]).resolve().parent
    depends = (zcutil / '..' / 'depends' / 'packages').resolve()
    #info(f'Scanning packages: {depends}')
    for p in depends.glob('*.mk'):
        if p.name in SKIP:
            continue

        #info(f'Parsing: {p}')
        try:
            pkginfos = extract_source_info(p)
        except Exception as e:
            warn(f'Skipping {p}: {e}')
            raise
        else:
            result.extend(pkginfos)

    json.dump(result, sys.stdout, indent=2, sort_keys=True)


def extract_source_info(path):
    paramrgx = re.compile(r'^(?P<key>\S+)=(?P<value>.*)$')
    rawparams = {}
    for line in path.open('r'):
        m = paramrgx.match(line)
        if m:
            rawparams[m.group('key')] = m.group('value') 

    resolver = Resolver(rawparams)

    pkgbase = resolver['package']
    version = resolver['$(package)_version']
    urlbase = resolver['$(package)_download_path'].rstrip('/')
    found = False

    for platform in ['default', 'linux', 'darwin', 'freebsd']:
        suffix = '' if platform == 'default' else f'_{platform}'
 
        try:
            urlfile = resolver[f'$(package)_file_name{suffix}']
        except KeyError:
            pass
        else:
            try:
                sha256 = resolver[f'$(package)_sha256_hash{suffix}']
            except KeyError as e:
                e.args += (f'Found url for {platform!r} but no sha256 hash.',)
                raise
            else:
                found = True
                yield {
                    'package': f'{pkgbase}{suffix}',
                    'version': version,
                    'url': f'{urlbase}/{urlfile}',
                    'sha256': sha256,
                }

    assert found, 'Could not find $(package)_file_name* make variables'


class Resolver:
    def __init__(self, rawparams):
        self._rp = rawparams

    def __getitem__(self, key):
        # truly awful hack
        try:
            rawval = self._rp[key]
        except KeyError as e:
            e.args += (f'Missing key {key!r} in params {self._rp.keys()}',)
            raise
        else:
            return self._cook(rawval)

    def _cook(self, rawval):
        cooked = ''
        state = 'base'
        depth = None
        key = None
        for i, c in enumerate(rawval):
            if state == 'base':
                if c == '$':
                    state = 'expecting-expansion'
                else:
                    cooked += c
            elif state == 'expecting-expansion':
                assert c == '(', f"expected '(', found {c!r} at col {i+1} in {rawval!r}; cooked: {cooked!r}"
                state = 'expansion' 
                depth = 1
                key = ''
            elif state == 'expansion':
                assert isinstance(depth, int), repr(locals())
                assert isinstance(key, str), repr(locals())
                if c == '(':
                    depth += 1
                    key += c
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        try:
                            cooked += self._rp[key]
                        except KeyError as e:
                            e.args += (f'rawval: {rawval!r}; cooked: {cooked!r}; key: {key!r}',)
                            raise
                        state = 'base'
                        depth = None
                        key = None
                    else:
                        key += c
                else:
                    key += c

        return cooked
            

def info(msg):
    sys.stderr.write(f'{msg}\n')


def warn(s):
    info(f'WARNING: {s}')
    

if __name__ == '__main__':
    main()
