datadir=$(prefix)/share/psymp3/data
data_DATA = vera.ttf

# Windows resource files
if HAVE_WINDRES
    WIN_RES = psymp3.res
    psymp3.res: psymp3.rc psymp3.ico music.ico xpmanifest.xml
	$(WINDRES) $(srcdir)/psymp3.rc -O coff -o psymp3.res
else
    WIN_RES =
endif

EXTRA_DIST = psymp3.rc psymp3.ico music.ico xpmanifest.xml \
             psymp3.icns music.icns psymp3.png psymp3.sh psymp3_icon.xpm \
             psymp3_icon1_nobg_128x128.png psymp3_icon1_nobg_16x16.png \
             psymp3_icon1_nobg_32x32.png psymp3_icon1_nobg_48x48.png \
             psymp3_icon1_nobg_64x64.png psymp3_icon1_nobg_96x96.png \
             psymp3_icon3.png psymp3_icon3_nobg_128x128.png \
             psymp3_icon3_nobg_16x16.png psymp3_icon3_nobg_32x32.png \
             psymp3_icon3_nobg_48x48.png psymp3_icon3_nobg_64x64.png \
             psymp3_icon3_nobg_96x96.png

# Clean up build artifacts
CLEANFILES = $(WIN_RES)
