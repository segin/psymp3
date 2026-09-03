#
# psymp3.spec - PsyMP3 RPM packaging (Fedora and openSUSE).
#
# The version is injected by package/rpm/build-rpm.sh:
#   rpmbuild --define "psymp3_version 2.0~snapshot1343" ...
# because PsyMP3's own version label ("2-CURRENT", "2.0-RC2") is not a legal
# RPM Version. See package/version.sh for the mapping.
#
# Dependencies are expressed as pkgconfig(...) rather than package names so
# the one spec resolves on both distributions, which spell nearly every
# -devel package differently (taglib-devel vs libtag-devel, opus-devel vs
# libopus-devel, dbus-devel vs dbus-1-devel, ...). OpenSSL is the exception:
# pkgconfig(openssl) is also provided by aws-lc on openSUSE, so it is named
# explicitly per distribution.
#

%global psymp3_ver %{?psymp3_version}%{!?psymp3_version:0}

Name:           psymp3
Version:        %{psymp3_ver}
Release:        1%{?dist}
Summary:        Audio player with FFT spectrum visualization

License:        ISC
URL:            https://github.com/segin/psymp3
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  autoconf-archive
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(sdl3)
BuildRequires:  pkgconfig(taglib)
BuildRequires:  pkgconfig(freetype2)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(ogg)
BuildRequires:  pkgconfig(opus)
BuildRequires:  pkgconfig(fdk-aac)
BuildRequires:  pkgconfig(spandsp)
BuildRequires:  pkgconfig(speex)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(Qt6Widgets)
%if 0%{?suse_version}
BuildRequires:  libopenssl-devel
BuildRequires:  update-desktop-files
%else
BuildRequires:  openssl-devel
BuildRequires:  desktop-file-utils
%endif

%description
PsyMP3 is a cross-platform audio player built around a real-time FFT
spectrum analyzer and a Windows 3.x-styled in-application window system.

It decodes MP3, MP2, FLAC, Ogg Vorbis, Opus, Speex, AAC, ALAC, WAV (PCM,
A-law, mu-law and G.722) and raw streams, using bundled decoders for
Vorbis (stb_vorbis), MP3 (minimp3), MP2 (kjmp2) and ALAC so the codec set
does not vary with what the system happens to provide.

Playback integrates with the desktop through MPRIS, scrobbles to Last.fm,
and can publish now-playing state to Discord.

%prep
%autosetup -n %{name}-%{version}

%build
# configure is generated, not shipped in the git tree.
./autogen.sh
%configure
%make_build

%install
%make_install

%if 0%{?suse_version}
%suse_update_desktop_file %{name}
%endif

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%files
%{_bindir}/%{name}
%{_datadir}/%{name}/
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Mon Aug 31 2026 Kirn Gill II <segin2005@gmail.com> - 0-1
- Automated build from the PsyMP3 source tree; see the project's GitHub
  releases page for per-release notes.
