# package/appimage/Dockerfile - the image build-appimage.sh runs in.
#
# An AppImage runs on any distribution whose glibc is at least as new as the
# one it was built against, so it is built on the oldest base worth supporting
# rather than on the developer's machine. Ubuntu 22.04 (glibc 2.35) is that
# base: it is the oldest Ubuntu LTS with Qt 6 and a GCC new enough for the
# tree, and Linux Mint 21 is built on it.
#
#   docker build -t psymp3-appimage package/appimage
#
# build-appimage.sh --docker does that and then runs itself inside the image.
FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# The first group builds PsyMP3; qmake6 and the portal platform theme are for
# finding and bundling Qt (the file dialog); the last group is what SDL3 is
# built against.
# SDL loads the audio and display libraries at run time from the machine it is
# on, so those are needed here as headers only and are not bundled.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential git ca-certificates curl file \
        autoconf automake libtool autoconf-archive pkgconf \
        libtag1-dev libfreetype-dev libharfbuzz-dev libssl-dev \
        libcurl4-openssl-dev libogg-dev libopus-dev libfdk-aac-dev \
        libspeex-dev libdbus-1-dev zlib1g-dev \
        qt6-base-dev qmake6 qt6-xdgdesktopportal-platformtheme \
        cmake ninja-build libasound2-dev libpulse-dev libpipewire-0.3-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
        libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev \
        libdecor-0-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev \
        libgles2-mesa-dev libgbm-dev libdrm-dev libudev-dev libibus-1.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 22.04 has no SDL3 package.
ARG SDL_TAG=release-3.4.18
RUN git clone --branch "$SDL_TAG" --depth 1 https://github.com/libsdl-org/SDL.git /tmp/SDL \
    && cmake -S /tmp/SDL -B /tmp/SDL/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_INSTALL_PREFIX=/usr/local -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/SDL/build \
    && cmake --install /tmp/SDL/build \
    && ldconfig \
    && rm -rf /tmp/SDL

# linuxdeploy gathers the libraries the binary needs into the bundle and packs
# it; its Qt plugin adds Qt's own plugins. Both are AppImages themselves, and
# there is no FUSE in a container, so they are told to unpack and run.
ENV APPIMAGE_EXTRACT_AND_RUN=1
RUN curl -fsSL -o /usr/local/bin/linuxdeploy \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && curl -fsSL -o /usr/local/bin/linuxdeploy-plugin-qt \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
    && chmod +x /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-qt

# The tree is bind-mounted from the host and owned by the host's user.
RUN git config --system --add safe.directory '*'
