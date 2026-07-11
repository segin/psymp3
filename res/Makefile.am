datadir=$(prefix)/share/psymp3/data
# vera.ttf: embedded UI font. psymp3.rgba: raw RGBA8888 (128x128) of the app
# icon, loaded at runtime for the SDL window icon on Unix (no PNG decoder needed).
data_DATA = vera.ttf psymp3.rgba

# Windows resource files
if HAVE_WINDRES
noinst_DATA = psymp3.res
psymp3.res: psymp3.rc psymp3.ico music.ico xpmanifest.xml
	$(WINDRES) $(srcdir)/psymp3.rc -O coff -o psymp3.res
endif

# --- Unix desktop integration (installed/removed by make install/uninstall) ---
# Guard on !HAVE_WINDRES so the Windows cross-build (which gets its icon from
# psymp3.rc) skips this. Uses $(datarootdir) (= $(prefix)/share), NOT the
# psymp3-specific datadir override above, so files land in the standard XDG dirs.
if !HAVE_WINDRES
desktopdir = $(datarootdir)/applications
dist_desktop_DATA = psymp3.desktop

# Install the icon into the hicolor theme at each shipped size, renamed to
# psymp3.png so the .desktop's Icon=psymp3 resolves through the theme.
icon_sizes = 16 32 48 64 96 128
install-data-local:
	@for s in $(icon_sizes); do \
	  d="$(DESTDIR)$(datarootdir)/icons/hicolor/$${s}x$${s}/apps"; \
	  echo " $(MKDIR_P) $$d"; $(MKDIR_P) "$$d" || exit 1; \
	  echo " $(INSTALL_DATA) psymp3_icon3_nobg_$${s}x$${s}.png $$d/psymp3.png"; \
	  $(INSTALL_DATA) "$(srcdir)/psymp3_icon3_nobg_$${s}x$${s}.png" "$$d/psymp3.png" || exit 1; \
	done

uninstall-local:
	@for s in $(icon_sizes); do \
	  rm -f "$(DESTDIR)$(datarootdir)/icons/hicolor/$${s}x$${s}/apps/psymp3.png"; \
	done
endif

EXTRA_DIST = psymp3.rc psymp3.ico music.ico xpmanifest.xml psymp3.rgba \
             psymp3.icns music.icns psymp3.png psymp3.sh psymp3_icon.xpm \
             psymp3_icon1_nobg_128x128.png psymp3_icon1_nobg_16x16.png \
             psymp3_icon1_nobg_32x32.png psymp3_icon1_nobg_48x48.png \
             psymp3_icon1_nobg_64x64.png psymp3_icon1_nobg_96x96.png \
             psymp3_icon3.png psymp3_icon3_nobg_128x128.png \
             psymp3_icon3_nobg_16x16.png psymp3_icon3_nobg_32x32.png \
             psymp3_icon3_nobg_48x48.png psymp3_icon3_nobg_64x64.png \
             psymp3_icon3_nobg_96x96.png

# Clean up build artifacts
if HAVE_WINDRES
CLEANFILES = psymp3.res
endif
