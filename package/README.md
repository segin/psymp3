# Distribution packaging

Native packages for PsyMP3. Everything here is driven by CI
(`.github/workflows/packages.yml`), but each builder also runs standalone on a
matching machine or container.

| Directory | Format | Targets tested in CI |
|---|---|---|
| `dpkg/` | `.deb` | Debian 13 (trixie), Ubuntu 26.04 LTS |
| `rpm/`  | `.rpm` | Fedora (latest), openSUSE Tumbleweed |
| `arch/` | `.pkg.tar.zst` | not in CI; built with `makepkg` on Arch |
| `appimage/` | `.AppImage` | built on Ubuntu 22.04, run on Ubuntu 24.04 |
| `mac/` | `PsyMP3.app` in a `.dmg` | built and run on GitHub's Apple Silicon macOS runner (`.github/workflows/macos.yml`) |
| `flatpak/` | `.flatpak` | a bundle on Flathub's KDE 6.11 runtime, built and run on Ubuntu |

## Building by hand

```sh
./package/dpkg/build-deb.sh      # -> package/dpkg/out/*.deb
./package/rpm/build-rpm.sh       # -> package/rpm/out/*.rpm
(cd package/arch && makepkg)     # -> package/arch/*.pkg.tar.zst
./package/appimage/build-appimage.sh --docker   # -> package/appimage/out/*.AppImage
./package/mac/build-app.sh --deps   # on macOS with Homebrew -> package/mac/out/PsyMP3.app and *.dmg
flatpak-builder --user --install-deps-from=flathub --force-clean \
    --repo=package/flatpak/repo package/flatpak/build \
    package/flatpak/io.github.segin.psymp3.yml   # then flatpak build-bundle, as CI does
```

The Flatpak builds from the working tree as it is, so run it from a clean
checkout (an in-tree build's objects would be copied in with the sources).

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

`makepkg -s` installs the Arch ones itself, from the PKGBUILD. The AppImage's
are in `appimage/Dockerfile`, which `--docker` builds for you.

## Versioning

PsyMP3's own version label — `2-CURRENT` on master, `2.0-RC2` on a release
tag — is not a legal package version for either format. `package/version.sh`
maps it:

| Tree version | Package version | Notes |
|---|---|---|
| `2.0-RC2` | `2.0~rc2` | sorts *before* `2.0` |
| `2.0-BETA4` | `2.0~beta4` | sorts before `2.0~rc2` |
| `1.99.17-RELEASE` | `1.99.17` | |
| `2-CURRENT` | `2.3~snapshot<build>` | the series is the next release, from `FILEVERSION 2,3,0,<build>` in `res/psymp3.rc`; `<build>` is its monotonic counter |

`~` sorts lower than everything in both dpkg and rpm, which is what makes a
pre-release upgrade cleanly to the final release. Verified ordering:

```
2.0~beta4  <  2.0~rc2  <  2.0~snapshot1400  <  2.0
2.2.2  <  2.3~snapshot1980  <  2.3
```

A master snapshot is named for the release master is heading towards, which
the release protocol sets in the `.rc`'s numeric version when it puts master
back to `2-CURRENT`: after 2.2.2-RELEASE that is `2,3,0`, so snapshots are
`2.3~snapshot<build>`. They outrank the release before and are superseded by
the next one. Snapshots from before this rule were named `2.0~snapshot<build>`;
those still upgrade to the new names.

pacman is the exception. Its `vercmp` gives `~` no special meaning, so
`2.0~rc2` sorts *above* `2.0` there — backwards. Dropping the tilde restores
the order, because a version ending in letters sorts below the bare release
under pacman's rules, so `version.sh arch` prints that spelling instead:

```
2.0beta4  <  2.0rc2  <  2.0snapshot1400  <  2.0
2.2.2  <  2.3snapshot1980  <  2.3
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

Fedora links FDK-AAC-free and may not support all AAC variants. openSUSE
builds against the full fdk-aac from Packman.

`build-rpm.sh` packs the tarball with `git archive`, so untracked build output
in a working tree never leaks into the package.

## Notes on the Arch packaging

The PKGBUILD takes its source from the repository it sits in, over
`git+file://`, so like the other two it packages what is committed. `pkgver()`
calls `version.sh arch` rather than carrying its own copy of the mapping;
makepkg will rewrite the `pkgver=` line in the file to match, which is
expected and need not be committed back.

## Notes on the AppImage

An AppImage is a single executable file carrying PsyMP3 and its libraries; it
needs no installation. It runs wherever glibc is at least as new as on the
machine that built it, so it is built on the oldest base worth supporting, not
on a developer's machine: `appimage/Dockerfile` is Ubuntu 22.04 (glibc 2.35),
the base of Linux Mint 21. That release has no SDL3 package, so the image
builds SDL3 from source, at the tag named by `SDL_TAG`.

`build-appimage.sh --docker` builds the image and runs itself inside it.
Without `--docker` it expects to be on such a machine already, with
`linuxdeploy` and `linuxdeploy-plugin-qt` on the `PATH`. Like the others it
packages what is committed.

What is inside, and what is not:

- **Bundled:** SDL3, TagLib, OpenSSL, curl, Ogg, Opus, FDK-AAC, Speex, D-Bus,
  and Qt 6 with its X11 platform plugin and desktop-portal theme, for the
  file dialog, plus what those libraries need in turn.
- **Taken from the machine it runs on:** glibc, libstdc++, FreeType,
  HarfBuzz, Fontconfig, libX11 and the OpenGL/EGL libraries, which
  linuxdeploy leaves out as things every desktop has; and the audio and
  display libraries (PipeWire, PulseAudio, ALSA, X11, Wayland), which SDL
  loads at run time. Qt's Wayland plugin is not bundled, so on a Wayland
  session the file dialog runs through XWayland.

The CI job runs the finished AppImage in an Ubuntu 24.04 container, on SDL's
dummy video and audio drivers, and checks that it starts and loads its bundled
font.

The data directory is compiled into the binary as `/usr/share/psymp3/data`,
which does not exist on the user's machine, since the bundle is mounted at a
new path on every run. The bundle's start-up script therefore sets
`PSYMP3_DATADIR` to its own copy, and the player reads that variable in
preference to the compiled-in path.
