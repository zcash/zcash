import re, sys, os, os.path
import subprocess
import argparse
from itertools import islice
from operator import itemgetter

author_aliases = {
    'Simon': 'Simon Liu',
    'bitcartel': 'Simon Liu',
    'EthanHeilman': 'Ethan Heilman',
}

def apply_author_aliases(name):
    if name in author_aliases:
        return author_aliases[name]
    else:
        return name

def parse_authors(line):
    commit_search = re.search('(\d+)', line)
    if commit_search:
        commits = commit_search.group(1)
    else:
        commits = 0
    name = re.sub(' \(\d+\)|:|\n|\r\n$', '', line)
    return name, commits

def alias_authors_in_release_notes(line):
    for key in author_aliases:
        if re.match(key, line):
            line = line.replace(key, author_aliases[key])
            break
    return line

## Returns dict of {'author': #_of_commits} from a release note
def authors_in_release_notes(filename):
    note = os.path.join(doc_dir, 'release-notes', filename)
    with open(note, 'r') as f:
        authors = {}
        line = f.readline()
        first_name, commits = parse_authors(line)
        authors[apply_author_aliases(first_name)] = commits
        for line in f:
            if line in ['\n', '\r\n']:
                for author in islice(f, 1):
                    name, commits = parse_authors(author)
                    authors[apply_author_aliases(name)] = commits
        return authors

## Sums commits made by contributors in each Zcash release note in ./doc/release-notes and writes to authors.md
def document_authors():
    print "Writing contributors documented in release-notes directory to authors.md."
    authors_file = os.path.join(doc_dir, 'authors.md')
    with open(authors_file, 'w') as f:
        f.write('Zcash Contributors\n==================\n\n')
        total_contrib = {}
        for notes in os.listdir(os.path.join(doc_dir, 'release-notes')):
            authors = authors_in_release_notes(notes)
            for author in authors:
                commits = int(authors[author])
                if author in total_contrib:
                    total_contrib[author] += commits
                else:
                    total_contrib[author] = commits
        sorted_contrib = sorted(total_contrib.items(), key=itemgetter(1, 0), reverse=True)
        for n, c in sorted_contrib:
            if c != 0:
                f.write("{0} ({1})\n".format(n, c))

## Writes release note to ./doc/release-notes based on git shortlog when current version number is specified
def generate_release_note(version, filename):
    print "Automatically generating release notes for {0} from git shortlog. Should review {1} for accuracy.".format(version, filename)
    latest_tag = subprocess.Popen(['git describe --abbrev=0'], shell=True, stdout=subprocess.PIPE).communicate()[0].strip()
    print "Previous release tag: ", latest_tag
    notes = subprocess.Popen(['git shortlog --no-merges {0}..HEAD'.format(latest_tag)], shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE).communicate()[0]
    lines = notes.split('\n')
    lines = [alias_authors_in_release_notes(line) for line in lines]
    release_note = os.path.join(doc_dir, 'release-notes', 'release-notes-{0}.md'.format(version))
    with open(release_note, 'w') as f:
        f.writelines('\n'.join(lines))

def main(version, filename):
    if version != None:
        generate_release_note(version, filename)
    document_authors()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--version')
    args = parser.parse_args()

    root_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    doc_dir = os.path.join(root_dir, 'doc')
    version = None
    filename = None
    if args.version:
        version = args.version
        filename = 'release-notes-{0}.md'.format(version)
    main(version, filename)
