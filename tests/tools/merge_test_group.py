#!/usr/bin/env python3
"""Merge a group of one-test-per-program check programs into a single program.

    tests/tools/merge_test_group.py <suite> <subdir> <title> <tags> <ldadd> prog...

Each test program here links the whole static stack -- around 150 MB of binary
and five seconds of linker -- to run tests that take milliseconds, and CI does
that twice, once for `check` and again for `distcheck`. A merged program keeps
every test's file, suite and failure count; what it drops is the link.

What this does: renames each program's main to <prog>_main (every branch of a
conditional build, and wraps one that takes argv), writes tests/<suite>.cpp
with a dispatcher that runs each in turn, rewires tests/Makefile.am, and
git mv's the sources to tests/<subdir>/.

It refuses when two files of the group define the same thing with external
linkage: merged, one definition stands for all of them, and the other files
then read their own type through a layout that is not its own. That is not
hypothetical -- four Ogg tests each had their own MockIOHandler, and merging
them without fixing it corrupted the heap. Give each file's copy internal
linkage (an anonymous namespace, or static for a free function) and try again.
MERGE_ALLOW names symbols a shared header defines inline, which are the same
definition everywhere and so are safe.
"""
import pathlib, re, subprocess, sys

suite, subdir, title, tags, ldadd, *group = sys.argv[1:]
ldadd = ldadd or "$(COMMON_TEST_LIBS) $(AM_LDFLAGS)"
root = pathlib.Path(subprocess.run(["git", "rev-parse", "--show-toplevel"],
                                   capture_output=True, text=True, check=True).stdout.strip())
tests = root / "tests"

# 1. Two files of the group must not define the same thing with external
#    linkage: merged, one definition wins and the other file's code reads its
#    own type through a layout that is not its own. Anonymous-namespace
#    helpers are internal and safe; so is anything from a shared header.
import collections
defined = collections.defaultdict(set)
unbuilt = []
for name in group:
    # A program with per-program flags gets automake's prefixed object name.
    obj = next((o for o in (tests / f"{name}.o", tests / f"{name}-{name}.o") if o.exists()), None)
    if obj is None:
        unbuilt.append(name)
        continue
    out = subprocess.run(["nm", "-C", "--defined-only", str(obj)], capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]* ([A-Za-z]) (.+)$", line)
        if not m:
            continue
        kind, sym = m.group(1), m.group(2)
        if kind in "tbdrsn" or "(anonymous namespace)" in sym:
            continue
        # nm prints a return type for some symbols ("bool std::operator==<...>"),
        # so the qualified name is what the filter below has to see.
        head = sym[:sym.index("(")] if "(" in sym else sym
        depth, cut = 0, 0
        for i, ch in enumerate(head):
            if ch == "<":
                depth += 1
            elif ch == ">":
                depth -= 1
            elif ch == " " and depth == 0:
                cut = i + 1
        # Both spellings are tested below: the cut is a guess, and on a symbol
        # it guesses wrong about it must not hide what the symbol really is.
        forms = (sym, sym[cut:])
        # Types a shared test header defines inline are the same definition in
        # every file that includes it; MERGE_ALLOW names them.
        import os
        if any(f.startswith(a) for f in forms for a in os.environ.get("MERGE_ALLOW", "").split()):
            continue
        if any(f.startswith((".L", "std::", "__gnu", "TestFramework::", "PsyMP3::", "TagLib::",
                             "Debug::", "operator new", "operator delete", "typeinfo", "vtable",
                             "VTT", "guard variable", "DW.ref", "non-virtual thunk",
                             "virtual thunk", "rc::", "SheenBidi", "hb_", "FT_",
                             # production types that are not in a namespace
                             "Stream::", "AudioFrame", "IOHandler::", "Player::",
                             "AudioCodec::", "Demuxer::")) for f in forms):
            continue
        defined[sym].add(name)
clashes = {s_: f for s_, f in defined.items() if len(f) > 1 and s_ != "main"}
if unbuilt:
    print(f"REFUSING: build these first so their symbols can be checked: {', '.join(unbuilt)}")
    sys.exit(1)
if clashes:
    print("REFUSING: these are defined by more than one file in the group, and merging")
    print("would let one definition stand for all of them:")
    owners = collections.defaultdict(set)
    for sym, files in clashes.items():
        owners[frozenset(files)].add(sym.split("::")[0])
    for files, names in owners.items():
        print(f"   {', '.join(sorted(names))}  in  {', '.join(sorted(files))}")
    print("Give each file's copy internal linkage (wrapclass.py) and try again.")
    sys.exit(1)

# 3. The dispatcher.
width = max(len(n) for n in group) + 2
decls = "\n".join(f"int {n}_main();" for n in group)
table = "\n".join(f'    {{"{n}",{" " * (width - len(n))}{n}_main}},' for n in group)
(tests / f"{suite}.cpp").write_text(f'''/*
 * {suite}.cpp - {title.lower()}, in one program
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Each of these tests keeps its own file and its own suite; what they share
 * is this program's link, which without them numbered one per test and cost
 * more than every one of them takes to run. Every entry returns its failure
 * count, as its main did.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: {title}
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: {tags}
 * @TEST_METADATA_END
 */

#include <cstdio>

{decls}

namespace {{

struct Entry {{
    const char* name;
    int (*run)();
}};

const Entry kEntries[] = {{
{table}
}};

}} // namespace

int main()
{{
    int failures = 0;
    int failed_files = 0;
    int skipped_files = 0;
    for (const Entry& entry : kEntries) {{
        // The name goes out before the tests run, so that a crash says which
        // file was in the middle of it.
        std::printf("\\n=== %s ===\\n", entry.name);
        std::fflush(stdout);
        const int failed = entry.run();
        if (failed > 0) {{
            ++failed_files;
            failures += failed;
        }}
    }}

    std::printf("\\n=== {title}: %zu files, %d failed test(s) in %d file(s), %d skipped ===\\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}}
''')

# 4. Makefile.am: one program in place of the group.
p = tests / "Makefile.am"
s = p.read_text()
def drop_from_programs(text, name, replacement=None):
    """Remove one name from a check_PROGRAMS statement, whatever its layout."""
    for m in re.finditer(r"^[ \t]*check_PROGRAMS\s*\+?=.*?(?<!\\)\n", text, re.M | re.S):
        stmt = m.group(0)
        if not re.search(r"(?<![\w.-])" + re.escape(name) + r"(?![\w.-])", stmt):
            continue
        fixed = re.sub(r"(?<![\w.-])" + re.escape(name) + r"(?![\w.-])[ \t]*",
                       (replacement + " ") if replacement else "", stmt, count=1)
        # A continuation line emptied by the removal goes with it.
        fixed = re.sub(r"\n[ \t]*\\\n", "\n", fixed)
        fixed = re.sub(r"[ \t]+\\\n$", "\n", fixed)
        return text[:m.start()] + fixed + text[m.end():], True
    return text, False

s, ok = drop_from_programs(s, group[0], suite)
if not ok:
    print(f"REFUSING: {group[0]} is not listed in check_PROGRAMS as expected")
    sys.exit(1)
for name in group[1:]:
    s, ok = drop_from_programs(s, name)
    if not ok:
        print(f"REFUSING: {name} is not listed in check_PROGRAMS as expected")
        sys.exit(1)

def remove_block(text, var):
    """Drop `var = ...` and its backslash continuation lines. Returns (text, at)."""
    start = text.find(f"\n{var} = ")
    if start < 0:
        return text, None
    start += 1
    at = start
    while True:
        eol = text.index("\n", at)
        if not text[at:eol].rstrip().endswith("\\"):
            break
        at = eol + 1
    end = text.index("\n", at) + 1
    if text[end:end + 1] == "\n":
        end += 1
    return text[:start] + text[end:], start

where = None
for n in group:
    found = False
    for var in (f"{n}_SOURCES", f"{n}_LDADD", f"{n}_CPPFLAGS", f"{n}_CXXFLAGS", f"{n}_LDFLAGS"):
        s, at = remove_block(s, var)
        if at is not None:
            found = True
            if where is None or at < where:
                where = at
    if not found:
        print(f"REFUSING: no Makefile block found for {n}")
        sys.exit(1)
s = s[:where] + "@@MERGED@@" + s[where:]
blocks = "@@MERGED@@"
merged = (f"# {title}: one program, as the AC-3 and Matroska tests are. Each test keeps\n"
          f"# its file and its suite; what they no longer have is a link apiece.\n"
          f"{suite}_SOURCES = {suite}.cpp \\\n\t"
          + " \\\n\t".join(f"{subdir}/{n}.cpp" for n in group)
          + f"\n{suite}_LDADD = {ldadd}\n\n")
s = s.replace(blocks, merged, 1)
p.write_text(s)

# 2. Rename each entry.
for name in group:
    p = tests / (name + ".cpp")
    s = p.read_text()
    new, count = re.subn(r"^int main\(\)", f"int {name}_main()", s, flags=re.M)
    argv_form = re.search(r"^int main\(int \w+, ?char\s*\*\s*\*?\s*\w+(\[\])?\)", new, re.M)
    if argv_form:
        # Nothing passes a merged program arguments of its own, so the entry
        # is called with just a program name.
        new, n2 = re.subn(r"^int main\(", f"int {name}_main_argv(", new, flags=re.M)
        new += (f"\n/// {name} took arguments when it was a program of its own.\n"
                f"int {name}_main()\n{{\n"
                f"    char program[] = \"{name}\";\n"
                f"    char* argv[] = {{program, nullptr}};\n"
                f"    return {name}_main_argv(1, argv);\n}}\n")
        count += n2
    if count == 0:
        print(f"REFUSING: {name}.cpp has no 'int main()' this can rename")
        sys.exit(1)
    if count > 1:
        print(f"note: {name}.cpp defines main {count} times (conditional build); renamed each")
    p.write_text(new)

# 5. Move the sources.
(tests / subdir).mkdir(parents=True, exist_ok=True)
for name in group:
    subprocess.run(["git", "mv", f"tests/{name}.cpp", f"tests/{subdir}/{name}.cpp"], cwd=root, check=True)
print(f"merged {len(group)} programs into {suite}, sources in tests/{subdir}/")
