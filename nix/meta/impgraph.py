import os
import re
import subprocess
from pathlib import Path


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
      print('digraph G {', file=f)
      for (mod, imps) in db.items():
          for imp in imps:
              print(f'  "{mod}" -> "{imp}";', file=f)
      print('}', file=f)

    for fmt in ['png', 'pdf']:
      print(f'Generating {fmt} from {str(dotpath)!r}...')
      subprocess.check_call([dotbin, '-O', f'-T{fmt}', dotpath])


def parse_imports(basedir):
    imprgx = re.compile(r'import\s+(\.[^ \n);]+)')
    basedir = basedir.resolve()

    db = {}

    for nixpath in find_nix_files(basedir):
        with nixpath.open('r') as f:
            nixsrc = f.read()

        imps = []
        for m in imprgx.finditer(nixsrc):
            relpath = m.group(1)
            dstpath = nixpath.parent.joinpath(relpath).resolve()
            imps.append(chop_default(dstpath).relative_to(basedir))
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
