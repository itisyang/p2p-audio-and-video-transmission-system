#ifndef READCAMERA_H
#define READCAMERA_H
#include <QThread>
#include <QImage>
#include <QMutex>

//必须加以下内容,否则编译不能通过,为了兼容C和C99标准
#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif

//引入ffmpeg头文件
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
}


class readcamera : public QThread
{
    Q_OBJECT
public:
    readcamera();
    void run();
    void ffmpeg_init();
    int device_init();
    int decoder_init();
    int encoder_init();
    int send_data();

signals:
    void emit_image(QImage);

private:
    AVFrame *pFrame;
    AVFrame *pFrameYUV;
    SwsContext *pSwsContext;
    SwsContext *img_convert_ctx;
    AVPicture  pAVPicture;
    AVFormatContext *fmt_ctx_v_i;
    AVFormatContext *fmt_ctx_v_o;
    AVCodecContext  *pCodecCtx_v_i;
    AVCodecContext  *pCodecCtx_v_o;
    AVStream* video_st;
    AVCodec *pCodec_v;
    AVPacket pAVPacket_v;
    AVPacket enc_pkt_v;

    int framecnt = 0;
    int vid_next_pts = 0;

    int videoindex;
    int Width;
    int Height;
    QMutex mutex;
};

#endif // READCAMERA_H
