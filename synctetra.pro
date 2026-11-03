TEMPLATE = subdirs

RC_ICONS = app/bin/resource/pic/icon.ico

# For desktop
!android {
SUBDIRS = app mimetex opus ogg monocypher

app.file = app/app.pro
app.depends = mimetex opus ogg monocypher

mimetex.file = thirdParty/mimetex/mimetex.pro
opus.file = thirdParty/opus/opus.pro
ogg.file = thirdParty/ogg/ogg.pro
monocypher.file = thirdParty/monocypher/monocypher.pro
}

# For Android
android {
SUBDIRS = app opus ogg monocypher

app.file = app/app.pro
app.depends = opus ogg monocypher

opus.file = thirdParty/opus/opus.pro
ogg.file = thirdParty/ogg/ogg.pro
monocypher.file = thirdParty/monocypher/monocypher.pro
}

