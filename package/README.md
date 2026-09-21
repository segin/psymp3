# Distribution packaging

Native packages for PsyMP3. Everything here is driven by CI
(`.github/workflows/packages.yml`), but each builder also runs standalone on a
matching machine or container.

| Directory | Format | Targets tested in CI |
|---|---|---|
| `dpkg/` | `.deb` | Debian 13 (trixie), Ubuntu 26.04 LTS |
| `rpm/`  | `.rpm` | Fedora (latest), openSUSE Tumbleweed |
| `arch/` | `.pkg.tar.zst` | not in CI; built with `makepkg` on Arch |

## Building by hand

```sh
./package/dpkg/build-deb.sh      # -> package/dpkg/out/*.deb
./package/rpm/build-rpm.sh       # -> package/rpm/out/*.rpm
(cd package/arch && makepkg)     # -> package/arch/*.pkg.tar.zst
```

The two scripts need the distribution's build dependencies installed first. The
dependency lists are single-sourced from the packaging metadata itself, so
install them from there rather than maintaining a third copy:

```sh
# Debian/Ubuntu
mk-build-deps -ir package/dpkg/debian/control

# Fedora
dnf builddep package/rpm/psymp3.spec

# openSUSE
zypper install $(rpmspec -q --buildrequires --define "psymp3_version 0" \
    package/rpm/psymp3.spec)
```

`makepkg -s` installs the Arch ones itself, from the PKGBUILD.

## Versioning

PsyMP3's own version label — `2-CURRENT` on master, `2.0-RC2` on a release
tag — is not a legal package version for either format. `package/version.sh`
maps it:

| Tree version | Package version | Notes |
|---|---|---|
| `2.0-RC2` | `2.0~rc2` | sorts *before* `2.0` |
| `2.0-BETA4` | `2.0~beta4` | sorts before `2.0~rc2` |
| `1.99.17-RELEASE` | `1.99.17` | |
| `2-CURRENT` | `2.0~snapshot<build>` | `<build>` is the monotonic counter in `res/psymp3.rc` |

`~` sorts lower than everything in both dpkg and rpm, which is what makes a
pre-release upgrade cleanly to the final release. Verified ordering:

```
2.0~beta4  <  2.0~rc2  <  2.0~snapshot1400  <  2.0
```

A master snapshot outranking the RC it followed is intentional: master is
ahead of the tag, and the eventual `2.0` still supersedes every `2.0~*`.

pacman is the exception. Its `vercmp` gives `~` no special meaning, so
`2.0~rc2` sorts *above* `2.0` there — backwards. Dropping the tilde restores
the order, because a version ending in letters sorts below the bare release
under pacman's rules, so `version.sh arch` prints that spelling instead:

```
2.0beta4  <  2.0rc2  <  2.0snapshot1400  <  2.0
```

## Notes on the Debian packaging

`dpkg-buildpackage` insists the packaging live in `./debian` at the root of the
tree being built. Rather than write into your working tree, `build-deb.sh`
exports HEAD to a temporary directory, drops `package/dpkg/debian` in beside
it, generates `debian/changelog` with the derived version, and builds there.
The source format is `3.0 (native)` — upstream and packaging are the same
tree, so there is no separate orig tarball to manage.

Because it builds from an export, **`build-deb.sh` packages what is committed,
not what is in your working tree**; commit first. `--worktree` packages the
tree as-is, but then the tree must be `distclean`: a configured tree carries
generated Makefiles holding absolute paths, and `dh_auto_clean`'s
`make distclean` fails on them the moment the tree is built from anywhere
other than where it was configured.

`debian/rules` skips `dh_auto_test`: the test harness wants fixture media and a
session D-Bus for the MPRIS tests, neither of which exists in a package build.

## Notes on the RPM packaging

One spec covers both distributions. Build dependencies are written as
`pkgconfig(...)` rather than package names because Fedora and openSUSE spell
nearly every `-devel` package differently (`taglib-devel` vs `libtag-devel`,
`opus-devel` vs `libopus-devel`, `dbus-devel` vs `dbus-1-devel`). OpenSSL is
the one exception, named explicitly per distribution: `pkgconfig(openssl)` is
also provided by `aws-lc-devel` on openSUSE, which would otherwise be pulled in
ahead of the real thing.

`build-rpm.sh` packs the tarball with `git archive`, so untracked build output
in a working tree never leaks into the package.

## Notes on the Arch packaging

The PKGBUILD takes its source from the repository it sits in, over
`git+file://`, so like the other two it packages what is committed. `pkgver()`
calls `version.sh arch` rather than carrying its own copy of the mapping;
makepkg will rewrite the `pkgver=` line in the file to match, which is
expected and need not be committed back.

Arch has the real `libfdk-aac`, so AAC is built here — unlike the Fedora
package, which has only `fdk-aac-free` to link against.
