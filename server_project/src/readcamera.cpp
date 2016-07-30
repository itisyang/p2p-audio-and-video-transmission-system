#include "readcamera.h"
#include <QDebug>

#define USE_DSHOW 1
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

//USB2.0 PC CAMERA   麦克风(2- USB2.0 MIC)
//HP Webcam-50     内置麦克风(IDT High D efinition AudioCODEC)
//Integrated Webcam   麦克风(Realtek High Definition Audio)

int thread_exit = 0;
unsigned char *video_buf;
static const char* out_path = "12345.h264";


int flush_encoder(AVFormatContext *fmt_ctx_v_i, AVFormatContext *fmt_ctx_v_o,
                  unsigned int stream_index, int framecnt)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt_v;
    if (!(fmt_ctx_v_o->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
        return 0;

    while (1)
    {
        enc_pkt_v.data = NULL;
        enc_pkt_v.size = 0;
        av_init_packet(&enc_pkt_v);
        ret = avcodec_encode_video2(fmt_ctx_v_o->streams[stream_index]->codec,
                                    &enc_pkt_v,NULL, &got_frame);
        //av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame){
            ret = 0;
            break;
        }
        qDebug("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d ", enc_pkt_v.size);
        framecnt++;
        //Write PTS
        AVRational time_base = fmt_ctx_v_o->streams[stream_index]->time_base;//{ 1, 1000 };
        AVRational r_framerate1 = fmt_ctx_v_i->streams[0]->r_frame_rate;// { 50, 2 };
        AVRational time_base_q = { 1, AV_TIME_BASE };
        //Duration between 2 frames (us)
        int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
        //Parameters
        enc_pkt_v.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
        enc_pkt_v.dts = enc_pkt_v.pts;
        enc_pkt_v.duration = av_rescale_q(calc_duration, time_base_q, time_base);

        /* copy packet*/
        //转换PTS/DTS（Convert PTS/DTS）
        enc_pkt_v.pos = -1;

        //fmt_ctx_v_o->duration = enc_pkt_v.duration * framecnt;

        /* mux encoded frame */
        ret = av_interleaved_write_frame(fmt_ctx_v_o, &enc_pkt_v);
        if (ret < 0)
            break;
    }
    return ret;
}


void show_dshow_device()
{
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("dshow");
    qDebug("\nDevice Info:\n");
    avformat_open_input(&pFormatCtx,"video=dummy",iformat,&options);
}


readcamera::readcamera()
{
    //ffmpeg_init();
}

/******************ffmpeg资源初始化***********************/
void readcamera::ffmpeg_init()
{
//Register Device
    av_register_all();
//Initialize libavformat and register all the muxers, demuxers and protocols
    avdevice_register_all();
//initialization of network components
    avformat_network_init();
}

/******************摄像头初始化***********************/
int readcamera::device_init()
{
    show_dshow_device();
#if USE_DSHOW
    AVInputFormat *ifmt = av_find_input_format("dshow");
    // Set device params
    AVDictionary *device_param = 0;
    /*if not setting rtbufsize, error messages will be shown in cmd,
      but you can still watch or record the stream correctly in most time
      setting rtbufsize will erase those error messages,
      however, larger rtbufsize will bring latency*/
    av_dict_set(&device_param, "rtbufsize", "1000M", 0);
    this->fmt_ctx_v_i = avformat_alloc_context();
    //Set own video device's name  //HP Webcam-50   //video=USB2.0 PC CAMERA
    if(avformat_open_input(&fmt_ctx_v_i,"video=Integrated Webcam",ifmt,&device_param)!=0){
        qDebug("Couldn't open input stream. ");
        return -1;
    }
#else
    AVInputFormat *ifmt = av_find_input_format("vfwcap");
    if (ifmt == NULL){
        qDebug("not support input device vfwcap. ");
        return -1;
    }
    if(avformat_open_input(&fmt_ctx_v_i,"1",ifmt,NULL) != 0){
        qDebug("Couldn't open input stream. ");
        return -1 ;
    }
#endif
    return 0;
}

/******************解码器初始化***********************/
int readcamera::decoder_init()
{
    av_dump_format(fmt_ctx_v_i,0,"CAMERA",0);
    if(avformat_find_stream_info(fmt_ctx_v_i,NULL) < 0){
        qDebug("Couldn't find stream information. ");
        return -1;
    }
    videoindex = -1;
    for(unsigned int i = 0; i < fmt_ctx_v_i->nb_streams; i++){
        if(fmt_ctx_v_i->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            videoindex = i;
            break ;
        }
    }
    if(videoindex == -1){
        qDebug("Couldn't find a video stream. ");
        return -1;
    }
    pCodecCtx_v_i = fmt_ctx_v_i->streams[videoindex]->codec;
    Width = pCodecCtx_v_i->width;
    Height = pCodecCtx_v_i->height;
    qDebug() << "原始视频宽高：" << Width << Height;
    //获取视频流解码器
    pCodec_v = avcodec_find_decoder(pCodecCtx_v_i->codec_id);
    //初始化图片空间
    avpicture_alloc(&pAVPicture,AV_PIX_FMT_RGB24,Width,Height);
    //初始化缩放规则
    pSwsContext = sws_getContext(Width, Height, pCodecCtx_v_i->pix_fmt,
                                 Width,Height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);
    /*第一个宽高为原始图像数据的宽高，第二个宽高为输出图像数据的宽高
      第三、六个参数分别为输入和输出图片数据的类型
      第七个参数为scale缩放算法类型，后三个参数置NULL或0即可*/

    //打开解码器
    if(avcodec_open2(pCodecCtx_v_i, pCodec_v, NULL)<0){
        qDebug("Could not open codec. ");
        return -1;
    }

    return 0;
}

/*******************编码器初始化**********************/
int readcamera::encoder_init()
{
    //编码成h264
    avformat_alloc_output_context2(&fmt_ctx_v_o, NULL, "h264", out_path);
    if (!fmt_ctx_v_o){
            qDebug( "Could not create output context");
            return -1;
    }
    pCodec_v = avcodec_find_encoder(AV_CODEC_ID_H264);
    pCodecCtx_v_o = avcodec_alloc_context3(pCodec_v);
    pCodecCtx_v_o->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx_v_o->bit_rate = 300000;
    pCodecCtx_v_o->width = pCodecCtx_v_i->width;
    pCodecCtx_v_o->height = pCodecCtx_v_i->height;
    /* frames per second */
    pCodecCtx_v_o->time_base = {1,25};
    pCodecCtx_v_o->gop_size = 250;
    /* Some formats want stream headers to be separate. */
    if (fmt_ctx_v_o->oformat->flags & AVFMT_GLOBALHEADER)
            pCodecCtx_v_o->flags |= CODEC_FLAG_GLOBAL_HEADER;
    //最大最小量化值
    pCodecCtx_v_o->qmin = 10;
    pCodecCtx_v_o->qmax = 51;
    //Optional Param
    pCodecCtx_v_o->max_b_frames = 0;
//    av_opt_set(pCodecCtx_v_o->priv_data, "preset", "slow", 0);
    AVDictionary *param = 0;
    av_dict_set(&param, "preset", "fast", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    if (avcodec_open2(pCodecCtx_v_o, pCodec_v, &param) < 0){
        qDebug() << "Could not open codec";
        return -1;
    }

    video_st = avformat_new_stream(fmt_ctx_v_o, pCodec_v);
    if (video_st == NULL)
        return -1;
    video_st->time_base.num = 1;
    video_st->time_base.den = 25;
    video_st->codec = pCodecCtx_v_o;
    if (avio_open(&fmt_ctx_v_o->pb,out_path, AVIO_FLAG_READ_WRITE) < 0){
        qDebug("Failed to open output file!");
        return -1;
    }
    if(avformat_write_header(fmt_ctx_v_o,NULL) != 0){
        qDebug("Failed to write header!");
        return -1;
    }
    //camera data may has a pix fmt of RGB or sth else,convert it to YUV420, for send data
    img_convert_ctx = sws_getContext(Width, Height, pCodecCtx_v_i->pix_fmt,
                                     Width, Height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    //Initialize the buffer to store YUV frames to be encoded.
    pFrameYUV = av_frame_alloc();
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, Width, Height));
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, Width, Height);

    return 0;
}

int readcamera::send_data()
{
    int frameFinished;
    int enc_got_frame;
    int64_t start_time = av_gettime();
    AVRational time_base_q = { 1, AV_TIME_BASE };
    if (av_read_frame(fmt_ctx_v_i, &pAVPacket_v) >= 0){
        if(pAVPacket_v.stream_index == videoindex){
            pFrame = av_frame_alloc();
            avcodec_decode_video2(pCodecCtx_v_i, pFrame, &frameFinished, &pAVPacket_v);
            if (frameFinished){
//                    //qDebug() << "发送一帧图像信号至界面";
#if 0
                mutex.lock();
                sws_scale(pSwsContext,(const uint8_t* const *)pFrame->data,
                          pFrame->linesize, 0, Height,pAVPicture.data, pAVPicture.linesize);
                //第一个参数为sws_getContext函数返回的值
                //第四个参数为从输入图像数据的第多少列开始逐行扫描，通常设为0
                //第五个参数为需要扫描多少行，通常为输入图像数据的高度
                QImage image(pAVPicture.data[0], Width,Height, QImage::Format_RGB888);
                emit emit_image(image);
                mutex.unlock();
#endif

#if 1
                //转换格式到YUV
                sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data,
                          pFrame->linesize, 0, Height, pFrameYUV->data, pFrameYUV->linesize);
                //qDebug() << "转换格式到YUV";
                //额外说明转换好的YUV帧的规格
                pFrameYUV->width = pFrame->width;
                pFrameYUV->height = pFrame->height;
                pFrameYUV->format = AV_PIX_FMT_YUV420P;
                //初始化数据包用来存储转换好的h264数据
                enc_pkt_v.data = NULL;
                enc_pkt_v.size = 0;
                av_init_packet(&enc_pkt_v);
                //编码成h264
                avcodec_encode_video2(pCodecCtx_v_o, &enc_pkt_v, pFrameYUV, &enc_got_frame);
                if (enc_got_frame == 1){
                    qDebug() << "成功编码一帧数据成h264";
                    framecnt++;
                    enc_pkt_v.stream_index = video_st->index;
                    //Write PTS
                    AVRational time_base = fmt_ctx_v_o->streams[0]->time_base;//{ 1, 1000 };
                    AVRational r_framerate1 = fmt_ctx_v_i->streams[videoindex]->r_frame_rate;//{ 50, 2 };
                    //Duration between 2 frames (us)
                    int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
                    //Parameters
                    enc_pkt_v.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
                    enc_pkt_v.dts = enc_pkt_v.pts;
                    enc_pkt_v.duration = av_rescale_q(calc_duration, time_base_q, time_base);
                    enc_pkt_v.pos = -1;
                    vid_next_pts=framecnt*calc_duration; //general timebase
                    //Delay
                    int64_t pts_time = av_rescale_q(enc_pkt_v.pts, time_base, time_base_q);
                    int64_t now_time = av_gettime() - start_time;
                   // if ((pts_time > now_time) && ((vid_next_pts + pts_time - now_time)<aud_next_pts))
                        av_usleep(pts_time - now_time);
                    av_interleaved_write_frame(fmt_ctx_v_o, &enc_pkt_v);
                    av_free_packet(&enc_pkt_v);
                }
                else{
                    qDebug() << "一帧编码成h264失败";
                }
#endif
//                if (flush_encoder(fmt_ctx_v_i, fmt_ctx_v_o, 0, framecnt) < 0){
//                    qDebug("Flushing encoder failed");
//                    return -1;
//                }
            }
            av_frame_free(&pFrame);
        }
    }
    else{
        qDebug() << "取不到数据帧";
    }
    //释放pAVPacket资源,否则内存会一直上升
    av_free_packet(&pAVPacket_v);

    return 0;
}

void readcamera::run()
{   
    if(device_init() < 0)
        return ;
    if(decoder_init() < 0)
        return ;
    if(encoder_init() < 0)
        return ;
    while(1){
        send_data();
    }
}

