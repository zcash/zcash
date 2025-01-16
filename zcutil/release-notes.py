#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re, os, os.path
import subprocess
import argparse
from operator import itemgetter

TEMP_RELEASE_NOTES_HEADER = [
    '(note: this is a temporary file, to be added-to by anybody, and moved to\n',
    'release-notes at release time)\n',
    '\n',
    'Notable changes\n',
    '===============\n',
    '\n',
]

RELEASE_NOTES_CHANGELOG_HEADING = [
    'Changelog\n',
    '=========\n',
    '\n',
]

author_aliases = {
    'Ariel': 'Ariel Gabizon',
    'arielgabizon': 'Ariel Gabizon',
    'bambam': 'Benjamin Winston',
    'bitcartel': 'Simon Liu',
    'Charlie OKeefe': 'Charlie O\'Keefe',
    'Daira Emma Hopwood': 'Daira-Emma Hopwood',
    'Daira Hopwood': 'Daira-Emma Hopwood',
    'Duke Leto': 'Jonathan \"Duke\" Leto',
    'ebfull': 'Sean Bowe',
    'ewillbefull@gmail.com': 'Sean Bowe',
    'Eirik0': 'Eirik Ogilvie-Wigley',
    'EthanHeilman': 'Ethan Heilman',
    'MarcoFalke': 'Marco Falke',
    'mdr0id': 'Marshall Gaucher',
    'Nathan Wilcox': 'Nate Wilcox',
    'NicolasDorier': 'Nicolas Dorier',
    'Nicolas DORIER': 'Nicolas Dorier',
    'Patick Strateman': 'Patrick Strateman',
    'paveljanik': 'Pavel Janík',
    'Sasha': 'sasha',
    'Simon': 'Simon Liu',
    'str4d': 'Jack Grigg',
    'therealyingtong': 'Ying Tong Lai',
    'ying tong': 'Ying Tong Lai',
    'Yasser': 'Yasser Isa',
    'zancas': 'Zancas Wilcox',
    'Za Wilcox': 'Zancas Wilcox',
    'zebambam': 'Benjamin Winston',
}

def apply_author_aliases(name):
    if name in author_aliases:
        return author_aliases[name]
    else:
        return name

def parse_authors(line):
    commit_search = re.search(' \((\d+)\):', line)
    if commit_search:
        commits = int(commit_search.group(1))
        name = re.sub(' \(\d+\)|:|\n|\r\n$', '', line)
        return name, commits
    else:
        return None, 0

def alias_authors_in_release_notes(line):
    for key in author_aliases:
        if re.match(key, line):
            line = line.replace(key, author_aliases[key])
            break
    return line

## Returns dict of {'author': #_of_commits} from a release note
def authors_in_release_notes(filename):
    note = os.path.join(doc_dir, 'release-notes', filename)
    with open(note, mode='r', encoding="utf-8", errors="replace") as f:
        authors = {}
        for line in f:
            name, commits = parse_authors(line)
            if commits > 0:
                name = apply_author_aliases(name)
                authors[name] = authors.get(name, 0) + commits
        return authors

## Sums commits made by contributors in each zcashd release note in ./doc/release-notes and writes to authors.md
def document_authors():
    print("Writing contributors documented in release-notes directory to authors.md.")
    authors_file = os.path.join(doc_dir, 'authors.md')
    with open(authors_file, mode='w', encoding="utf-8", errors="replace") as f:
        f.write('Zcash Contributors\n==================\n\n')
        total_contrib = {}
        for notes in os.listdir(os.path.join(doc_dir, 'release-notes')):
            # Commits are duplicated across beta, RC and final release notes,
            # except for the pre-launch release notes.
            if ('-beta' in notes or '-rc' in notes) and '1.0.0-' not in notes:
                continue
            authors = authors_in_release_notes(notes)
            for author in authors:
                commits = int(authors[author])
                total_contrib[author] = total_contrib.get(author, 0) + commits

        sorted_contrib = sorted(total_contrib.items(), key=itemgetter(1, 0), reverse=True)
        for n, c in sorted_contrib:
            if c != 0:
                f.write("* {0} ({1})\n".format(n, c))

## Writes release note to ./doc/release-notes based on git shortlog when current version number is specified
def generate_release_note(version, prev, clear):
    filename = 'release-notes-{0}.md'.format(version)
    print("Automatically generating release notes for {0} from git shortlog. Should review {1} for accuracy.".format(version, filename))
    if prev:
        latest_tag = prev
    else:
        # fetches latest tags, so that latest_tag will be correct
        subprocess.Popen(['git fetch -t'], shell=True, stdout=subprocess.PIPE).communicate()[0]
        latest_tag = subprocess.Popen(['git describe --abbrev=0'], shell=True, stdout=subprocess.PIPE).communicate()[0].strip()
    print("Previous release tag: ", latest_tag)
    notes = subprocess.Popen(['git shortlog --no-merges {0}..HEAD'.format(latest_tag)], shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE).communicate()[0]
    lines = notes.decode("utf-8", "replace").split('\n')
    lines = [alias_authors_in_release_notes(line) for line in lines]
    temp_release_note = os.path.join(doc_dir, 'release-notes.md')
    with open(temp_release_note, mode='r', encoding="utf-8", errors="replace") as f:
        notable_changes = f.readlines()
        # Assumes that all notable changes are appended to the default header
        if len(notable_changes) > 6:
            notable_changes = notable_changes[3:] + ['\n']
        else:
            notable_changes = []
    release_note = os.path.join(doc_dir, 'release-notes', filename)
    with open(release_note, mode='w', encoding="utf-8", errors="replace") as f:
        f.writelines(notable_changes)
        f.writelines(RELEASE_NOTES_CHANGELOG_HEADING)
        f.writelines('\n'.join(lines))
    if clear:
        # Clear temporary release notes file
        with open(temp_release_note, mode='w', encoding="utf-8", errors="replace") as f:
            f.writelines(TEMP_RELEASE_NOTES_HEADER)

def main(version, prev, clear):
    if version != None:
        generate_release_note(version, prev, clear)
    document_authors()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', help='Upcoming version, without leading v')
    parser.add_argument('--prev', help='Previous version, with leading v')
    parser.add_argument('--clear', help='Wipe doc/release-notes.md', action='store_true')
    args = parser.parse_args()

    root_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    doc_dir = os.path.join(root_dir, 'doc')
    version = None
    prev = None
    clear = False
    if args.version:
        version = args.version
        prev = args.prev
        clear = args.clear
    main(version, prev, clear)
