#-------------------------------------------------
#
# Project created by QtCreator 2016-04-08T20:55:01
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = server_project
TEMPLATE = app
CONFIG += console

INCLUDEPATH += include/server
INCLUDEPATH += include/ffmpeg
INCLUDEPATH += include/p2p
FORMS += ui/mainwindow.ui

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/readcamera.cpp

HEADERS += \
    Include/server/mainwindow.h \
    Include/server/readcamera.h \
    Include/server/public_header.h \
    Include/ffmpeg/libavcodec/avcodec.h \
    Include/ffmpeg/libavcodec/avdct.h \
    Include/ffmpeg/libavcodec/avfft.h \
    Include/ffmpeg/libavcodec/d3d11va.h \
    Include/ffmpeg/libavcodec/dirac.h \
    Include/ffmpeg/libavcodec/dv_profile.h \
    Include/ffmpeg/libavcodec/dxva2.h \
    Include/ffmpeg/libavcodec/qsv.h \
    Include/ffmpeg/libavcodec/vaapi.h \
    Include/ffmpeg/libavcodec/vda.h \
    Include/ffmpeg/libavcodec/vdpau.h \
    Include/ffmpeg/libavcodec/version.h \
    Include/ffmpeg/libavcodec/videotoolbox.h \
    Include/ffmpeg/libavcodec/vorbis_parser.h \
    Include/ffmpeg/libavcodec/xvmc.h \
    Include/ffmpeg/libavdevice/avdevice.h \
    Include/ffmpeg/libavdevice/version.h \
    Include/ffmpeg/libavfilter/avfilter.h \
    Include/ffmpeg/libavfilter/avfiltergraph.h \
    Include/ffmpeg/libavfilter/buffersink.h \
    Include/ffmpeg/libavfilter/buffersrc.h \
    Include/ffmpeg/libavfilter/version.h \
    Include/ffmpeg/libavformat/avformat.h \
    Include/ffmpeg/libavformat/avio.h \
    Include/ffmpeg/libavformat/version.h \
    Include/ffmpeg/libavutil/adler32.h \
    Include/ffmpeg/libavutil/aes.h \
    Include/ffmpeg/libavutil/aes_ctr.h \
    Include/ffmpeg/libavutil/attributes.h \
    Include/ffmpeg/libavutil/audio_fifo.h \
    Include/ffmpeg/libavutil/avassert.h \
    Include/ffmpeg/libavutil/avconfig.h \
    Include/ffmpeg/libavutil/avstring.h \
    Include/ffmpeg/libavutil/avutil.h \
    Include/ffmpeg/libavutil/base64.h \
    Include/ffmpeg/libavutil/blowfish.h \
    Include/ffmpeg/libavutil/bprint.h \
    Include/ffmpeg/libavutil/bswap.h \
    Include/ffmpeg/libavutil/buffer.h \
    Include/ffmpeg/libavutil/camellia.h \
    Include/ffmpeg/libavutil/cast5.h \
    Include/ffmpeg/libavutil/channel_layout.h \
    Include/ffmpeg/libavutil/common.h \
    Include/ffmpeg/libavutil/cpu.h \
    Include/ffmpeg/libavutil/crc.h \
    Include/ffmpeg/libavutil/des.h \
    Include/ffmpeg/libavutil/dict.h \
    Include/ffmpeg/libavutil/display.h \
    Include/ffmpeg/libavutil/downmix_info.h \
    Include/ffmpeg/libavutil/error.h \
    Include/ffmpeg/libavutil/eval.h \
    Include/ffmpeg/libavutil/ffversion.h \
    Include/ffmpeg/libavutil/fifo.h \
    Include/ffmpeg/libavutil/file.h \
    Include/ffmpeg/libavutil/frame.h \
    Include/ffmpeg/libavutil/hash.h \
    Include/ffmpeg/libavutil/hmac.h \
    Include/ffmpeg/libavutil/hwcontext.h \
    Include/ffmpeg/libavutil/hwcontext_vdpau.h \
    Include/ffmpeg/libavutil/imgutils.h \
    Include/ffmpeg/libavutil/intfloat.h \
    Include/ffmpeg/libavutil/intreadwrite.h \
    Include/ffmpeg/libavutil/lfg.h \
    Include/ffmpeg/libavutil/log.h \
    Include/ffmpeg/libavutil/lzo.h \
    Include/ffmpeg/libavutil/macros.h \
    Include/ffmpeg/libavutil/mastering_display_metadata.h \
    Include/ffmpeg/libavutil/mathematics.h \
    Include/ffmpeg/libavutil/md5.h \
    Include/ffmpeg/libavutil/mem.h \
    Include/ffmpeg/libavutil/motion_vector.h \
    Include/ffmpeg/libavutil/murmur3.h \
    Include/ffmpeg/libavutil/opt.h \
    Include/ffmpeg/libavutil/parseutils.h \
    Include/ffmpeg/libavutil/pixdesc.h \
    Include/ffmpeg/libavutil/pixelutils.h \
    Include/ffmpeg/libavutil/pixfmt.h \
    Include/ffmpeg/libavutil/random_seed.h \
    Include/ffmpeg/libavutil/rational.h \
    Include/ffmpeg/libavutil/rc4.h \
    Include/ffmpeg/libavutil/replaygain.h \
    Include/ffmpeg/libavutil/ripemd.h \
    Include/ffmpeg/libavutil/samplefmt.h \
    Include/ffmpeg/libavutil/sha.h \
    Include/ffmpeg/libavutil/sha512.h \
    Include/ffmpeg/libavutil/stereo3d.h \
    Include/ffmpeg/libavutil/tea.h \
    Include/ffmpeg/libavutil/threadmessage.h \
    Include/ffmpeg/libavutil/time.h \
    Include/ffmpeg/libavutil/timecode.h \
    Include/ffmpeg/libavutil/timestamp.h \
    Include/ffmpeg/libavutil/tree.h \
    Include/ffmpeg/libavutil/twofish.h \
    Include/ffmpeg/libavutil/version.h \
    Include/ffmpeg/libavutil/xtea.h \
    Include/ffmpeg/libpostproc/postprocess.h \
    Include/ffmpeg/libpostproc/version.h \
    Include/ffmpeg/libswresample/swresample.h \
    Include/ffmpeg/libswresample/version.h \
    Include/ffmpeg/libswscale/swscale.h \
    Include/ffmpeg/libswscale/version.h \
    Include/p2p/AVAPIs.h \
    Include/p2p/AVFRAMEINFO.h \
    Include/p2p/AVIOCTRLDEFs.h \
    Include/p2p/IOTCAPIs.h \
    Include/p2p/P2PTunnelAPIs.h \
    Include/p2p/RDTAPIs.h

LIBS += -L$$PWD/lib/ffmpeg/ -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale
LIBS += -L$$PWD/lib/p2p/ -lAVAPIs -lIOTCAPIs -lP2PTunnelAPIs -lRDTAPIs

QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_CXXFLAGS += -Wno-deprecated-declarations

PRECOMPILED_HEADER = include/server/stable.h
