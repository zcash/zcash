import os
import re
import subprocess
from pathlib import Path


IMP_RGX = re.compile(
  r'''
  inherit\s+\(
    import\s+(?P<INHERIT_IMPORT>\.[^ \n);]+)
  \)
  (?P<INHERIT_SUBIMPORTS>(\s+[A-Za-z0-9_-]+)+)
  |
  import\s+(?P<SIMPLE_IMPORT>\.[^ \n);]+)
  ''',
  re.VERBOSE,
)


def main():
    src = Path(os.environ['src'])
    out = Path(os.environ['out'])
    dotbin = Path(os.environ['graphviz']) / 'bin' / 'dot'
    dotpath = out / 'zc-nix-imports.dot'

    out.mkdir()

    print(f'Parsing nix imports from {str(src)!r}...')
    db = parse_imports(src)

    print(f'Writing {str(dotpath)!r}...')
    with dotpath.open('w') as f:
      def p(s):
          print(s, file=f)

      p('digraph G {')
      p('  rankdir=LR;')
      p('  node [shape=box];')
      for (mod, imps) in db.items():
          for imp in imps:
              p(f'  "{mod}" -> "{imp}";')
      p('}')

    for fmt in ['png', 'pdf']:
      print(f'Generating {fmt} from {str(dotpath)!r}...')
      subprocess.check_call([dotbin, '-O', f'-T{fmt}', dotpath])


def parse_imports(basedir):
    basedir = basedir.resolve()

    db = {}

    for nixpath in find_nix_files(basedir):
        with nixpath.open('r') as f:
            nixsrc = f.read()

        imps = []
        for m in IMP_RGX.finditer(nixsrc):
            simpimp = m.group('SIMPLE_IMPORT')
            if simpimp is not None:
              dstpath = nixpath.parent.joinpath(simpimp).resolve()
              imps.append(chop_default(dstpath).relative_to(basedir))
            else:
              inhimp = m.group('INHERIT_IMPORT')
              assert inhimp is not None, m.groups()
              impbase = nixpath.parent.joinpath(inhimp).resolve()
              for subimp in m.group('INHERIT_SUBIMPORTS').split():
                subpath = impbase / f'{subimp}.nix'
                if subpath.is_file():
                  imps.append(subpath.relative_to(basedir))
        db[chop_default(nixpath).relative_to(basedir)] = imps

    return db


def chop_default(p):
    if p.name == 'default.nix':
        return p.parent
    else:
        return p


def find_nix_files(basedir):
    for p in basedir.iterdir():
        if p.is_file() and p.name.endswith('.nix'):
            yield p
        elif p.is_dir():
            for nixfile in find_nix_files(p):
                yield nixfile


if __name__ == '__main__':
    main()
