TEMPLATE = subdirs

RC_ICONS = app/bin/resource/pic/icon.ico

# For desktop
!android {
SUBDIRS = app mimetex opus ogg

app.file = app/app.pro
app.depends = mimetex opus ogg

mimetex.file = thirdParty/mimetex/mimetex.pro
opus.file = thirdParty/opus/opus.pro
ogg.file = thirdParty/ogg/ogg.pro
}

# For Android
android {
SUBDIRS = app opus ogg

app.file = app/app.pro
app.depends = opus ogg

opus.file = thirdParty/opus/opus.pro
ogg.file = thirdParty/ogg/ogg.pro
}

