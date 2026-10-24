#!/usr/bin/env python3
"""Bring the vendored engines (ThirdParty/fasegroove*/Source) up to HEAD of each plugin repo.

For every plugin, a three-way merge of Source/engine/* and Source/Guide*.h:
  base   = the plugin at the commit recorded in ThirdParty/ENGINE_VERSIONS (last sync),
  ours   = the vendored file (base + the Arranger's own changes, ThirdParty/ENGINE_PATCHES.md),
  theirs = the plugin at HEAD.
The Arranger's changes are kept; plugin changes since the last sync are applied. A plugin with a
conflict is left untouched and its conflicts are listed. ThirdParty/engine-patches/<plugin>.diff
holds the exact Arranger changes against the recorded commit: a vendored file edited without
recording them is reported. A file new in the plugin is added (then list it in CMakeLists.txt).

Usage: scripts/sync-engines.py [--check | --record [--synced]] [--worktree] [--repos DIR] [plugin ...]
  (no option)  apply the plugin changes, then record the new commit and patches
  --check      report only; exit 1 when a vendored engine is behind HEAD, conflicts or has
               unrecorded changes
  --record     record the current vendored files as the Arranger's changes (after editing them)
  --synced     with --record: the vendored files already hold the plugin's HEAD (a conflict
               resolved by hand), so HEAD becomes the last sync
  --worktree   take the plugin's working tree (changes not committed yet) instead of HEAD;
               nothing is recorded: after committing the plugin, run --record --synced
  --repos DIR  folder with the plugin repos (default: the folder holding this repo)
Plugins: piano, drums, guitar (default: all three). After applying, build and check the MIDI
(ctest, scripts/verify-midi.py) before committing.
"""
from pathlib import Path
import argparse
import difflib
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
third_party = root / 'ThirdParty'
versions_file = third_party / 'ENGINE_VERSIONS'
patches_dir = third_party / 'engine-patches'
plugins = ['piano', 'drums', 'guitar']
worktree = ':worktree'   # a "revision" meaning the plugin's working tree


def is_engine_file(rel):
    # Sources only: a stray .orig or .rej next to them is not part of the engine.
    if Path(rel).suffix not in ('.h', '.cpp'):
        return False
    return rel.startswith('Source/engine/') or (rel.count('/') == 1 and Path(rel).name.startswith('Guide') and rel.endswith('.h'))


def git(repo, *args, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], capture_output=True, check=False)
    if check and result.returncode != 0:
        raise SystemExit(f'git {" ".join(args)} in {repo}: {result.stderr.decode().strip()}')
    return result


def read_versions():
    versions = {}
    for line in versions_file.read_text().splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            name, sha = line.split()
            versions[name] = sha
    return versions


def write_versions(versions):
    lines = ['# Plugin commit each vendored engine was last synchronised with (scripts/sync-engines.py).']
    lines += [f'fasegroove{p} {versions[f"fasegroove{p}"]}' for p in plugins if f'fasegroove{p}' in versions]
    versions_file.write_text('\n'.join(lines) + '\n')


def files_in(folder):
    return {path.relative_to(folder).as_posix() for path in (folder / 'Source').rglob('*')
            if path.is_file() and is_engine_file(path.relative_to(folder).as_posix())}


def engine_files(repo, rev):
    if rev == worktree:
        return files_in(repo)
    names = git(repo, 'ls-tree', '-r', '--name-only', rev, '--', 'Source').stdout.decode().splitlines()
    return {n for n in names if is_engine_file(n)}


def vendored_files(name):
    return files_in(third_party / name)


def blob(repo, rev, path):
    if rev == worktree:
        return (repo / path).read_text() if (repo / path).is_file() else None
    result = git(repo, 'show', f'{rev}:{path}', check=False)
    return result.stdout.decode() if result.returncode == 0 else None


def local_patch(name, base_texts, files):
    """The Arranger's changes against the recorded commit, as one unified diff."""
    out = []
    for path in sorted(files | set(base_texts)):
        before = base_texts.get(path) or ''
        vendored = third_party / name / path
        after = vendored.read_text() if vendored.exists() else ''
        out += difflib.unified_diff(before.splitlines(True), after.splitlines(True), f'a/{path}', f'b/{path}')
    return ''.join(line if line.endswith('\n') else line + '\n\\ No newline at end of file\n' for line in out)


def merge(ours, base, theirs):
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for label, text in (('ours', ours), ('base', base), ('theirs', theirs)):
            p = Path(tmp) / label
            p.write_text(text)
            paths.append(str(p))
        result = subprocess.run(['git', 'merge-file', '-p', '-L', 'arranger', '-L', 'last sync', '-L', 'plugin HEAD', *paths],
                                capture_output=True)
        if result.returncode < 0 or result.returncode > 127:
            raise SystemExit(f'git merge-file failed: {result.stderr.decode().strip()}')
        return result.stdout.decode(), result.returncode


def sync(plugin, repos, mode, versions, synced=False, use_worktree=False):
    name = f'fasegroove{plugin}'
    repo = repos / name
    if not (repo / '.git').exists():
        print(f'MISSING   {name}: no repo at {repo}')
        return False
    base_rev = versions.get(name)
    if base_rev is None:
        print(f'MISSING   {name}: no commit in {versions_file.relative_to(root)}')
        return False
    head = worktree if use_worktree else git(repo, 'rev-parse', 'HEAD').stdout.decode().strip()
    shown = 'worktree' if use_worktree else head[:7]
    files = vendored_files(name)
    base_files = engine_files(repo, base_rev)
    base_texts = {p: blob(repo, base_rev, p) for p in base_files}
    patch_file = patches_dir / f'{name}.diff'

    if mode == 'record':
        if synced and base_rev != head:
            base_rev = versions[name] = head
            write_versions(versions)
            base_texts = {p: blob(repo, head, p) for p in engine_files(repo, head)}
        patches_dir.mkdir(exist_ok=True)
        patch_file.write_text(local_patch(name, base_texts, files))
        print(f'recorded  {name} at {base_rev[:7]}')
        return True

    ok = True
    recorded = patch_file.read_text() if patch_file.exists() else None
    if recorded != local_patch(name, base_texts, files):
        print(f'UNRECORDED {name}: vendored files changed without updating {patch_file.relative_to(root)} '
              '(describe it in ThirdParty/ENGINE_PATCHES.md, then run --record)' + (': nothing applied' if mode == 'apply' else ''))
        ok = False

    head_files = engine_files(repo, head)
    changes, conflicts, notes = {}, [], []
    for path in sorted(head_files | base_files):
        base, theirs = base_texts.get(path), blob(repo, head, path) if path in head_files else None
        if base == theirs:
            continue
        vendored = third_party / name / path
        if path not in files:
            if base is None:
                changes[path] = theirs   # new in the plugin: added
            else:
                notes.append(f'{path}: changed in the plugin, not vendored (removed here on purpose?)')
            continue
        if theirs is None:
            notes.append(f'{path}: removed in the plugin, still vendored')
            continue
        ours = vendored.read_text()
        merged, count = merge(ours, base or '', theirs)
        if count:
            conflicts.append(f'{path}: {count} conflict(s)')
        elif merged != ours:
            changes[path] = merged

    if conflicts:
        print(f'CONFLICT  {name} {base_rev[:7]}..{shown}: nothing applied')
        for c in conflicts:
            print(f'    {c}')
        ok = False
    elif changes and mode == 'check':
        print(f'BEHIND    {name} {base_rev[:7]}..{shown}: {len(changes)} file(s) would change')
        for path, merged in changes.items():
            vendored = third_party / name / path
            if not vendored.exists():
                print(f'    {path} (new)')
                continue
            changed = sum(1 for l in difflib.ndiff(vendored.read_text().splitlines(), merged.splitlines()) if l[:1] in '+-')
            print(f'    {path} ({changed} lines)')
        ok = False
    elif mode == 'apply' and ok:
        for path, merged in changes.items():
            target = third_party / name / path
            if not target.exists():
                notes.append(f'{path}: added' + (', list it in CMakeLists.txt' if path.endswith('.cpp') else ''))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(merged)
        if use_worktree:
            if changes:
                notes.append(f'nothing recorded: commit {name}, then scripts/sync-engines.py --record --synced {plugin}')
        elif base_rev != head:
            versions[name] = head
            write_versions(versions)
            head_texts = {p: blob(repo, head, p) for p in head_files}
            patch_file.write_text(local_patch(name, head_texts, files))
        print(f'{"applied" if changes else "in sync"}   {name} at {shown}'
              + (f': {len(changes)} file(s) updated, rebuild and check the MIDI' if changes else ''))
    elif ok:
        print(f'in sync   {name} at {shown}' + ('' if base_rev == head else f' (recorded {base_rev[:7]}, its later changes are already here)'))
    for n in notes:
        print(f'    note: {n}')
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--check', action='store_const', dest='mode', const='check')
    group.add_argument('--record', action='store_const', dest='mode', const='record')
    parser.add_argument('--synced', action='store_true')
    parser.add_argument('--worktree', action='store_true')
    parser.add_argument('--repos', type=Path, default=root.parent)
    parser.add_argument('plugin', nargs='*')
    args = parser.parse_args()
    if args.synced and args.mode != 'record':
        parser.error('--synced goes with --record')
    if args.worktree and args.mode == 'record':
        parser.error('--worktree records nothing: commit the plugin, then --record --synced')
    if unknown := [p for p in args.plugin if p not in plugins]:
        parser.error(f'unknown plugin {unknown[0]} (choose from {", ".join(plugins)})')
    versions = read_versions()
    ok = True
    for plugin in args.plugin or plugins:
        ok = sync(plugin, args.repos.resolve(), args.mode or 'apply', versions, args.synced, args.worktree) and ok
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
