#include <windows.h>
#include <sys/time.h>
#include <unistd.h>
#include <QDebug>

#include "AVAPIs.h"
#include "IOTCAPIs.h"
#include "P2PTunnelAPIs.h"
#include "RDTAPIs.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "public_header.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

#define MAX_CLIENT_NUMBER   6
#define MAX_SIZE_IOCTRL_BUF 1024
#define SERVTYPE_STREAM_SERVER 1

#define USE_AUDIO 0
static int gOnlineNum = 0;
char gUID[21];
static AV_Client gClientInfo[MAX_CLIENT_NUMBER];
AVRational time_base_q = { 1, AV_TIME_BASE };
AVFrame *pFrame;
AVFrame *pFrameYUV;
SwsContext *pSwsContext;
SwsContext *img_convert_ctx;
SwrContext *aud_convert_ctx;
AVPicture  pAVPicture;
AVFormatContext *fmt_ctx_v_i;
AVFormatContext *fmt_ctx_a_i;
AVFormatContext *fmt_ctx_v_o;
AVFormatContext *fmt_ctx_a_o;
AVCodecContext  *pCodecCtx_v_i;
AVCodecContext  *pCodecCtx_a_i;
AVCodecContext  *pCodecCtx_v_o;
AVCodecContext  *pCodecCtx_a_o;
AVStream* video_st;
AVStream* audio_st;
AVCodec *pCodec_v;
AVCodec* pCodec_a;
AVPacket pAVPacket_v,enc_pkt_v;
AVPacket pAVPacket_a,enc_pkt_a;

AVAudioFifo *fifo;
uint8_t **converted_input_samples;

int framecnt = 0;
int vid_next_pts = 0;
int nb_samples = 0;
int videoindex;
int audioindex;
int Width = 0;
int Height = 0;
static const char* out_path = "12345.h264";
static const char* out_path_a = "123456.h264";
int aud_next_pts = 0;

void ffmpeg_init()
{
//Register Device
    av_register_all();
//Initialize libavformat and register all the muxers, demuxers and protocols
    avdevice_register_all();
//initialization of network components
    avformat_network_init();
}

//Show Device
static void show_dshow_device(){
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("dshow");
    printf("Device Info=============\n");
    avformat_open_input(&pFormatCtx,"video=dummy",iformat,&options);
    printf("========================\n");
}

int device_init()
{
    AVInputFormat *fmt_temp = av_find_input_format("dshow");
    // Set device params
    AVDictionary *param = 0;
    //实时缓冲区，用来存放实时数据帧
    av_dict_set(&param, "rtbufsize", "1000M", 0);
    fmt_ctx_v_i = avformat_alloc_context();
    //HP Webcam-50   //video=USB2.0 PC CAMERA  //Integrated Webcam
    if(avformat_open_input(&fmt_ctx_v_i,"video=USB2.0 PC CAMERA",fmt_temp,&param)!=0){
        printf("Couldn't open input stream.\n");
        return -1;
    }

    AVDictionary *param_a = 0;
    av_dict_set(&param_a, "rtbufsize", "1000M", 0);
    fmt_ctx_a_i = avformat_alloc_context();
    //麦克风 (2- USB2.0 MIC)  //麦克风 Realtek High Definition Audio
    if (avformat_open_input(&fmt_ctx_a_i, "audio=麦克风 (2- USB2.0 MIC)", fmt_temp, &param_a) != 0){
        printf("Couldn't open input audio stream.\n");
        return -1;
    }

    return 0;
}

int decoder_init()
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

#if 1

    if (avformat_find_stream_info(fmt_ctx_a_i, NULL) < 0){
        printf("Couldn't find audio stream information.\n");
        return -1;
    }
    audioindex = -1;
    for (unsigned int i = 0; i < fmt_ctx_a_i->nb_streams; i++){
        if (fmt_ctx_a_i->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            audioindex = i;
            break;
        }
    }
    if (audioindex == -1){
        printf("Couldn't find a audio stream.");
        return -1;
    }
    pCodecCtx_a_i = fmt_ctx_a_i->streams[audioindex]->codec;
    pCodec_a = avcodec_find_decoder(pCodecCtx_a_i->codec_id);
    if (avcodec_open2(pCodecCtx_a_i, pCodec_a, NULL) < 0){
        printf("Could not open audio codec.");
        return -1;
    }

#endif

    return 0;
}

int encoder_init()
{
#if USE_AUDIO
    avformat_alloc_output_context2(&fmt_ctx_v_o, NULL, "flv", out_path);
    if (!fmt_ctx_v_o){
            qDebug( "Could not create output context");
            return -1;
    }
#else
    avformat_alloc_output_context2(&fmt_ctx_v_o, NULL, "h264", out_path);
    if (!fmt_ctx_v_o){
            qDebug( "Could not create output context");
            return -1;
    }
    avformat_alloc_output_context2(&fmt_ctx_a_o, NULL, "h264", out_path_a);
    if (!fmt_ctx_a_o){
            qDebug( "Could not create output context");
            return -1;
    }
#endif

    //视频，编码成h264
    pCodec_v = avcodec_find_encoder(AV_CODEC_ID_H264);
    pCodecCtx_v_o = avcodec_alloc_context3(pCodec_v);
    pCodecCtx_v_o->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx_v_o->bit_rate = 300000;
    pCodecCtx_v_o->width = pCodecCtx_v_i->width;
    pCodecCtx_v_o->height = pCodecCtx_v_i->height;
    /* frames per second */
    pCodecCtx_v_o->time_base = {1,25};
    pCodecCtx_v_o->gop_size = 25;//250//30
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
    av_dict_set(&param, "preset", "veryfast", 0);
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

    //camera data may has a pix fmt of RGB or sth else,convert it to YUV420, for send data
    img_convert_ctx = sws_getContext(Width, Height, pCodecCtx_v_i->pix_fmt,
                                     Width, Height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    //Initialize the buffer to store YUV frames to be encoded.
    pFrameYUV = av_frame_alloc();
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, Width, Height));
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, Width, Height);



#if 1//USE_AUDIO

    //音频，编码成ACC
    pCodec_a = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!pCodec_a){
        printf("Can not find output audio encoder! (没有找到合适的编码器！)\n");
        return -1;
    }
    pCodecCtx_a_o = avcodec_alloc_context3(pCodec_a);
    pCodecCtx_a_o->channels = 2;
    pCodecCtx_a_o->channel_layout = av_get_default_channel_layout(2);
    pCodecCtx_a_o->sample_rate = pCodecCtx_a_i->sample_rate;
    pCodecCtx_a_o->sample_fmt = pCodec_a->sample_fmts[0];
    pCodecCtx_a_o->bit_rate = 32000;
    pCodecCtx_a_o->time_base.num = 1;
    pCodecCtx_a_o->time_base.den = pCodecCtx_a_o->sample_rate;
    pCodecCtx_a_o->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    if (fmt_ctx_a_o->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCtx_a_o->flags |= CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(pCodecCtx_a_o, pCodec_a, NULL) < 0){
        printf("Failed to open ouput audio encoder! (编码器打开失败！)\n");
        return -1;
    }

    // Initialize the resampler to be able to convert audio sample formats
    aud_convert_ctx = swr_alloc_set_opts(NULL,
        av_get_default_channel_layout(pCodecCtx_a_o->channels),
        pCodecCtx_a_o->sample_fmt,
        pCodecCtx_a_o->sample_rate,
        av_get_default_channel_layout(pCodecCtx_a_i->channels),
        pCodecCtx_a_i->sample_fmt,
        pCodecCtx_a_i->sample_rate,
        0, NULL);
    swr_init(aud_convert_ctx);

    //Initialize the FIFO buffer to store audio samples to be encoded.
    fifo = NULL;
    fifo = av_audio_fifo_alloc(pCodecCtx_a_o->sample_fmt, pCodecCtx_a_o->channels, 1);

    //Initialize the buffer to store converted samples to be encoded.
    converted_input_samples = NULL;
    if (!(converted_input_samples = (uint8_t**)calloc(pCodecCtx_a_o->channels,
            sizeof(**converted_input_samples)))) {
                printf("Could not allocate converted input sample pointers\n");
                return AVERROR(ENOMEM);
            }

    //Add a new stream to output,should be called by the user before avformat_write_header() for muxing
    audio_st = avformat_new_stream(fmt_ctx_a_o, pCodec_a);
    if (audio_st == NULL){
        return -1;
    }
    audio_st->time_base.num = 1;
    audio_st->time_base.den = pCodecCtx_a_o->sample_rate;
    audio_st->codec = pCodecCtx_a_o;
#endif

    //Write  Header
    if(avformat_write_header(fmt_ctx_v_o,NULL) != 0){
        qDebug("Failed to write header!");
        return -1;
    }

    return 0;
}


int p2p_init()
{
    int ret = 0;
    int SID = 0;
    strcpy(gUID,"ZSCYLVKMKPYERAA9111A");
    //H2WB5KJU4UMERAPY111A
    //ZSCYLVKMKPYERAA9111A

    InitAVInfo();
    create_streamout_thread();

    IOTC_Set_Max_Session_Number(MAX_CLIENT_NUMBER);

    // use which Master base on location, port 0 means to get a random port
    ret = IOTC_Initialize2(0);
    if(ret != IOTC_ER_NoERROR){
        qDebug("IOTC_Initialize2(), ret=[%d]\n", ret);
        DeInitAVInfo();
        return -1;
    }

    //using callback for the login status of that device is updated from IOTC servers
    IOTC_Get_Login_Info_ByCallBackFn(LoginInfoCB);
    // alloc MAX_CLIENT_NUMBER*3 for every session av data/speaker/play back
    avInitialize(MAX_CLIENT_NUMBER*3);
    // create thread to login because without WAN still can work on LAN
    pthread_t ThreadLogin_ID;
    if((ret = pthread_create(&ThreadLogin_ID, NULL, &thread_Login, (void *)gUID))){
        printf("Login Thread create fail, ret=[%d]\n", ret);
        return -1;
    }
    pthread_detach(ThreadLogin_ID);

    pthread_t ThreadListen_ID;
    ret = pthread_create(&ThreadListen_ID, NULL, &thread_listen_client, (void *)SID);
    if(ret){
        printf("Listen thread create fail, ret=[%d]\n",ret);
        return -1;
    }
    pthread_detach(ThreadListen_ID);

    return 0;
}

void InitAVInfo()
{
    int i;
    for(i=0;i<MAX_CLIENT_NUMBER;i++)
    {
        memset(&gClientInfo[i], 0, sizeof(AV_Client));
        gClientInfo[i].avIndex = -1;
        gClientInfo[i].playBackCh = -1;
        pthread_rwlock_init(&(gClientInfo[i].sLock), NULL);
    }
}

void DeInitAVInfo()
{
    int i;
    for(i=0;i<MAX_CLIENT_NUMBER;i++)
    {
        memset(&gClientInfo[i], 0, sizeof(AV_Client));
        gClientInfo[i].avIndex = -1;
        pthread_rwlock_destroy(&gClientInfo[i].sLock);
    }
}

void CALLBACK LoginInfoCB(unsigned int nLoginInfo)
{
    if((nLoginInfo & 0x04))
        printf("I can be connected via Internet\n");
    else if((nLoginInfo & 0x08))
        printf("I am be banned by IOTC Server because UID multi-login\n");
}

void *thread_Login(void *arg)
{
    int ret;

    while(1){
        ret = IOTC_Device_Login((char *)arg, NULL, NULL);
        if(ret == IOTC_ER_NoERROR){
            printf("IOTC_Device_Login success! uid = %s\n", (char *)arg);
            break;
        }
        else{
            printf("IOTC_Device_Login() ret = %d\n", ret);
            Sleep(10000);
        }
    }

    pthread_exit(0);
    return NULL;
}

int CALLBACK AuthCallBackFn(char *viewAcc,char *viewPwd)
{
    //if(strcmp(viewAcc, avID) == 0 && strcmp(viewPwd, avPass) == 0)
        return 1;

    return 0;
}


void regedit_client_to_video(int SID, int avIndex)
{
    AV_Client *p = &gClientInfo[SID];
    p->avIndex = avIndex;
    p->bEnableVideo = 1;
}

void unregedit_client_from_video(int SID)
{
    AV_Client *p = &gClientInfo[SID];
    p->bEnableVideo = 0;
}

void regedit_client_to_audio(int SID, int avIndex)
{
    AV_Client *p = &gClientInfo[SID];
    p->bEnableAudio = 1;
}

void unregedit_client_from_audio(int SID)
{
    AV_Client *p = &gClientInfo[SID];
    p->bEnableAudio = 0;
}


/****
Thread - Start AV server and recv IOCtrl cmd for every new av idx
*/
void *thread_ForAVServerStart(void *arg)
{
    int SID = *(int *)arg;
    free(arg);
    int ret;
    unsigned int ioType;
    char ioCtrlBuf[MAX_SIZE_IOCTRL_BUF];
    struct st_SInfo Sinfo;

    printf("SID[%d], thread_ForAVServerStart, OK\n", SID);

    //设置验证信息及超时时间（设置为0直到client连接上server才返回）
    int nResend=-1;
    int avIndex = avServStart3(SID, AuthCallBackFn, 0, SERVTYPE_STREAM_SERVER, 0, &nResend);
    if(avIndex < 0){
        printf("avServStart3 failed!! SID[%d] code[%d]!!!\n", SID, avIndex);
        IOTC_Session_Close(SID);
        gOnlineNum--;
        pthread_exit(0);
    }
    if(IOTC_Session_Check(SID, &Sinfo) == IOTC_ER_NoERROR){
        const char *mode[3] = {"P2P", "RLY", "LAN"};
        if( isdigit( Sinfo.RemoteIP[0] ))
            printf("Client is from[IP:%s, Port:%d] Mode[%s]\n", Sinfo.RemoteIP, Sinfo.RemotePort, mode[(int)Sinfo.Mode]);
    }
    printf("avServStart3 OK, avIndex[%d], Resend[%d]\n\n", avIndex, nResend);

    avServSetResendSize(avIndex, 512);

    while(1){
        //printf("wait cmd\n");
        ret = avRecvIOCtrl(avIndex, &ioType, (char *)&ioCtrlBuf, MAX_SIZE_IOCTRL_BUF, 2000);
        if(ret >= 0){
            printf("received cmd\n");
            Handle_IOCTRL_Cmd(SID, avIndex, ioCtrlBuf, ioType);
        }
        else if(ret != AV_ER_TIMEOUT){
            printf("avIndex[%d], avRecvIOCtrl error, code[%d]\n",avIndex, ret);
            break;
        }
    }

    if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE)
        qDebug() << "对方关闭了会话";
    qDebug() << "退出接收消息模块";

    unregedit_client_from_video(SID);
    unregedit_client_from_audio(SID);
    avServStop(avIndex);
    printf("SID[%d], avIndex[%d], thread_ForAVServerStart exit!!\n", SID, avIndex);

    IOTC_Session_Close(SID);
    gOnlineNum--;

    pthread_exit(0);
    return NULL;
}


void *thread_listen_client(void *arg)
{
    int SID;
//    SID = (int)arg;
    while(1){
        int *sid = NULL;
        // Accept connection only when IOTC_Listen() calling
        SID = IOTC_Listen(0);
        qDebug() << "Listen";
        if(SID < 0){
            if(SID == IOTC_ER_EXCEED_MAX_SESSION){
                printf("exceed max session!\n");
                Sleep(3000);
            }
            continue;
        }

        sid = (int *)malloc(sizeof(int));
        *sid = SID;
        pthread_t Thread_ID;
        int ret = pthread_create(&Thread_ID, NULL, &thread_ForAVServerStart, (void *)sid);
        if(ret < 0)
            printf("pthread_create failed ret[%d]\n", ret);
        else{
            pthread_detach(Thread_ID);
            gOnlineNum++;
        }
    }

    DeInitAVInfo();
    avDeInitialize();
    IOTC_DeInitialize();
}


void Handle_IOCTRL_Cmd(int SID, int avIndex, char *buf, int type)
{
    printf("Handle CMD: 0x%x\n", type);

    switch(type)
    {
        case IOTYPE_USER_IPCAM_START:{
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("IOTYPE_USER_IPCAM_START, ch:%d, avIndex:%d\n", p->channel, avIndex);
            //get writer lock
            int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
            regedit_client_to_video(SID, avIndex);
            //release lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);

            printf("regedit_client_to_video OK\n\n");
        }
            break;
        case IOTYPE_USER_IPCAM_STOP:{
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("IOTYPE_USER_IPCAM_STOP, ch:%d, avIndex:%d\n", p->channel, avIndex);
            //get writer lock
            int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
            unregedit_client_from_video(SID);
            //release lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);
            printf("unregedit_client_from_video OK\n\n");
        }
            break;
        case IOTYPE_USER_IPCAM_AUDIOSTART:{
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("IOTYPE_USER_IPCAM_AUDIOSTART, ch:%d, avIndex:%d\n", p->channel, avIndex);
            //get writer lock
            int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
            regedit_client_to_audio(SID, avIndex);
            //release lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Release SID %d rwlock error, ret = %d\n\n", SID, lock_ret);
            printf("regedit_client_to_audio OK\n\n");
        }
            break;
        case IOTYPE_USER_IPCAM_AUDIOSTOP:{
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("IOTYPE_USER_IPCAM_AUDIOSTOP, ch:%d, avIndex:%d\n", p->channel, avIndex);
            //get writer lock
            int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
            unregedit_client_from_audio(SID);
            //release lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
            if(lock_ret)
                printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);
            printf("unregedit_client_from_audio OK\n\n");
        }
            break;
        default:{
                printf("avIndex %d: non-handle type[%X]\n\n", avIndex, type);
        }
            break;
    }
}

unsigned int getTimeStamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

void create_streamout_thread()
{
    int ret;
    pthread_t ThreadVideoFrameData_ID;
    pthread_t ThreadAudioFrameData_ID;

    if((ret = pthread_create(&ThreadVideoFrameData_ID, NULL, &thread_VideoFrameData, NULL))){
        printf("pthread_create ret=%d\n", ret);
        exit(-1);
    }
    pthread_detach(ThreadVideoFrameData_ID);

    if((ret = pthread_create(&ThreadAudioFrameData_ID, NULL, &thread_AudioFrameData, NULL))){
        printf("pthread_create ret=%d\n", ret);
        exit(-1);
    }
    pthread_detach(ThreadAudioFrameData_ID);
}

/********
Thread - Send Video frames to all AV-idx
*/
void *thread_VideoFrameData(void *arg)
{
#if 1
    int i = 0, ret = 0;

    // *** set Video Frame info here ***
    FRAMEINFO_t frameInfo;
    memset(&frameInfo, 0, sizeof(FRAMEINFO_t));
    frameInfo.codec_id = MEDIA_CODEC_VIDEO_H264;
    //frameInfo.flags = IPC_FRAME_FLAG_IFRAME;
    printf("thread_VideoFrameData start OK\n");

    int frameFinished;
    int enc_got_frame;
    //Initialize the buffer to store converted samples to be encoded.
    while(1){
        frameInfo.timestamp = getTimeStamp();
        frameInfo.onlineNum = gOnlineNum;
        int64_t start_time=av_gettime();
        if (av_read_frame(fmt_ctx_v_i, &pAVPacket_v) >= 0){
            if(pAVPacket_v.stream_index == videoindex){
                pFrame = av_frame_alloc();
                avcodec_decode_video2(pCodecCtx_v_i, pFrame, &frameFinished, &pAVPacket_v);
                av_free_packet(&pAVPacket_v);
                if (frameFinished){
                    //qDebug() << "解码一帧视频";
                    sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data,
                              pFrame->linesize, 0, Height, pFrameYUV->data, pFrameYUV->linesize);
                    //说明转换好的YUV帧的规格,用于H264编码
                    pFrameYUV->width = pFrame->width;
                    pFrameYUV->height = pFrame->height;
                    pFrameYUV->format = AV_PIX_FMT_YUV420P;

                    //Initialize h264数据
                    enc_pkt_v.data = NULL;
                    enc_pkt_v.size = 0;
                    av_init_packet(&enc_pkt_v);
                    //encode to h264
                    avcodec_encode_video2(pCodecCtx_v_o, &enc_pkt_v, pFrameYUV, &enc_got_frame);
                    if (enc_got_frame == 1){
                        framecnt++;
                        AVRational time_base = fmt_ctx_v_o->streams[videoindex]->time_base;
                        AVRational r_framerate1 = fmt_ctx_v_i->streams[videoindex]->r_frame_rate;
                        AVRational time_base_q = { 1, AV_TIME_BASE };
                        int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));

                        enc_pkt_v.pts = av_rescale_q(framecnt*calc_duration, time_base_q, time_base);
                        enc_pkt_v.dts = enc_pkt_v.pts;
                        enc_pkt_v.duration = av_rescale_q(calc_duration, time_base_q, time_base);

                        enc_pkt_v.pos = -1;
                        int64_t pts_time = av_rescale_q(enc_pkt_v.dts, time_base, time_base_q);
                        int64_t now_time = av_gettime() - start_time;
                        if (pts_time > now_time)
                            av_usleep(pts_time - now_time);

                        //qDebug() << "one frame success encode to h264";
                        for(i = 0 ; i < MAX_CLIENT_NUMBER; i++){
                            //get reader lock
                            int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
                            if(lock_ret)
                                printf("Acquire SID %d rdlock error, ret = %d\n", i, lock_ret);
                            if(gClientInfo[i].avIndex < 0 || gClientInfo[i].bEnableVideo == 0){
                                //this is not alive or asking for video
                                //release reader lock
                                lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                                if(lock_ret)
                                    printf("Acquire SID %d rdlock error, ret = %d\n", i, lock_ret);
                                continue;
                            }
                            //send frame data to a client
                            ret = avSendFrameData(gClientInfo[i].avIndex, (char*)enc_pkt_v.data, enc_pkt_v.size,
                                                  &frameInfo, sizeof(FRAMEINFO_t));
                            if(ret == AV_ER_EXCEED_MAX_SIZE){
                                // means data not write to queue, send too slow, I want to skip it
                                usleep(10000);
                                continue;
                            }
                            else if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE){
                                printf("thread_VideoFrameData AV_ER_SESSION_CLOSE_BY_REMOTE SID[%d]\n", i);
                                unregedit_client_from_video(i);
                                continue;
                            }
                            else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
                                printf("thread_VideoFrameData AV_ER_REMOTE_TIMEOUT_DISCONNECT SID[%d]\n", i);
                                unregedit_client_from_video(i);
                                continue;
                            }
                            else if(ret == IOTC_ER_INVALID_SID){
                                printf("session can't be used anymore\n");
                                unregedit_client_from_video(i);
                                continue;
                            }
                            else if(ret < 0)
                                printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
                        }
                        av_free_packet(&enc_pkt_v);
                        av_frame_free(&pFrame);
                        //av_frame_free(&pFrameYUV);
                    }
                }
            }
        }
    }
    pthread_exit(0);

#endif
}

/********
Thread - Send Audio frames to all AV-idx
*/
void *thread_AudioFrameData(void *arg)
{

#if 1//USE_AUDIO
    FRAMEINFO_t frameInfo;
    memset(&frameInfo, 0, sizeof(FRAMEINFO_t));
    frameInfo.codec_id = MEDIA_CODEC_AUDIO_G711U;

    int ret;
    int frameFinished;
    int enc_got_frame;
    const int output_frame_size = pCodecCtx_a_o->frame_size;

    while(1){
        int64_t start_time=av_gettime();
        while (av_audio_fifo_size(fifo) < output_frame_size){
            frameInfo.timestamp = getTimeStamp();
            frameInfo.onlineNum = gOnlineNum;

            /**
            * Decode one frame worth of audio samples, convert it to the
            * output sample format and put it into the FIFO buffer.
            */
            AVFrame *input_frame = av_frame_alloc();

            /** Decode one frame worth of audio samples. */
            /** Packet used for temporary storage. */
            AVPacket input_packet;
            av_init_packet(&input_packet);
            input_packet.data = NULL;
            input_packet.size = 0;

            if (av_read_frame(fmt_ctx_a_i, &input_packet) >= 0){

                avcodec_decode_audio4(pCodecCtx_a_i,input_frame,&frameFinished, &input_packet);

                av_packet_unref(&input_packet);

                /** If there is decoded data, convert and store it */
                if (frameFinished) {
                    //qDebug() << "解码一帧音频成功";
                    /**
                    * Allocate memory for the samples of all channels in one consecutive
                    * block for convenience.
                    */
                    if ((ret = av_samples_alloc(converted_input_samples,
                                                NULL,
                                                pCodecCtx_a_o->channels,
                                                input_frame->nb_samples,
                                                pCodecCtx_a_o->sample_fmt, 0)) < 0) {
                        printf("Could not allocate converted input samples\n");
                        av_freep(&(*converted_input_samples)[0]);
                        free(*converted_input_samples);
                        continue ;
                    }

                    /**
                    * Convert the input samples to the desired output sample format.
                    * This requires a temporary storage provided by converted_input_samples.
                    */
                    /** Convert the samples using the resampler. */
                    if ((ret = swr_convert(aud_convert_ctx,
                        converted_input_samples, input_frame->nb_samples,
                        (const uint8_t**)input_frame->extended_data, input_frame->nb_samples)) < 0) {
                        printf("Could not convert input samples\n");
                        continue;
                    }

                    /** Add the converted input samples to the FIFO buffer for later processing. */
                    /**
                    * Make the FIFO as large as it needs to be to hold both,
                    * the old and the new samples.
                    */
                    if ((ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + input_frame->nb_samples)) < 0) {
                        printf("Could not reallocate FIFO\n");
                        continue;
                    }


                    /** Store the new samples in the FIFO buffer. */
                    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                        input_frame->nb_samples) < input_frame->nb_samples) {
                        printf("Could not write data to FIFO\n");
                        continue;
                    }
                    //av_freep(&(*converted_input_samples)[0]);
                    //free(*converted_input_samples);
                    av_frame_free(&input_frame);

                }
            }
        }
        /**
               * If we have enough samples for the encoder, we encode them.
               * At the end of the file, we pass the remaining samples to
               * the encoder.
               */

            if (av_audio_fifo_size(fifo) >= output_frame_size){
                /** Temporary storage of the output samples of the frame written to the file. */
                AVFrame *output_frame=av_frame_alloc();
                if (!output_frame){
                    qDebug() << "output_frame alloc fail";
                    continue;
                }
                /**
                * Use the maximum number of possible samples per frame.
                * If there is less than the maximum possible frame size in the FIFO
                * buffer use this number. Otherwise, use the maximum possible frame size
                */
                const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                    pCodecCtx_a_o->frame_size);

                /** Initialize temporary storage for one output frame. */
                /**
                * Set the frame's parameters, especially its size and format.
                * av_frame_get_buffer needs this to allocate memory for the
                * audio samples of the frame.
                * Default channel layouts based on the number of channels
                * are assumed for simplicity.
                */
                output_frame->nb_samples = frame_size;
                output_frame->channel_layout = pCodecCtx_a_o->channel_layout;
                output_frame->format = pCodecCtx_a_o->sample_fmt;
                output_frame->sample_rate = pCodecCtx_a_o->sample_rate;

                /**
                * Allocate the samples of the created frame. This call will make
                * sure that the audio frame can hold as many samples as specified.
                */
                if ((ret = av_frame_get_buffer(output_frame, 0)) < 0) {
                    printf("Could not allocate output frame samples\n");
                    av_frame_free(&output_frame);
                    continue;
                }

                /**
                * Read as many samples from the FIFO buffer as required to fill the frame.
                * The samples are stored in the frame temporarily.
                */
                if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
                    printf("Could not read data from FIFO\n");
                    continue;
                }



                /** Encode one frame worth of audio samples. */
                /** Packet used for temporary storage. */
                AVPacket output_packet;
                av_init_packet(&output_packet);
                output_packet.data = NULL;
                output_packet.size = 0;

                /** Set a timestamp based on the sample rate for the container. */
                if (output_frame) {
                    nb_samples += output_frame->nb_samples;
                }

                /**
                * Encode the audio frame and store it in the temporary packet.
                * The output audio stream encoder is used to do this.
                */
                int enc_got_frame_a = 0;
                if ((ret = avcodec_encode_audio2(pCodecCtx_a_o, &output_packet,
                    output_frame, &enc_got_frame_a)) < 0) {
                    printf("Could not encode frame\n");
                    av_frame_free(&output_frame);
                    av_packet_unref(&output_packet);
                    continue;
                }
                /** Write one audio frame from the temporary packet to the output file. */
                if (enc_got_frame_a) {
                    //qDebug() << "编码一帧音频成功";
                    output_packet.stream_index = 1;
                    AVRational time_base = fmt_ctx_a_o->streams[audioindex]->time_base;
                    AVRational r_framerate1 = { fmt_ctx_a_i->streams[audioindex]->codec->sample_rate, 1 };// { 44100, 1};

                    int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));  //内部时间戳

                    output_packet.pts = av_rescale_q(nb_samples*calc_duration, time_base_q, time_base);
                    output_packet.dts = output_packet.pts;
                    output_packet.duration = output_frame->nb_samples;


                    aud_next_pts = nb_samples*calc_duration;

                    int64_t pts_time = av_rescale_q(output_packet.pts, time_base, time_base_q);
                    int64_t now_time = av_gettime() - start_time;
                    if ((pts_time > now_time) && ((aud_next_pts + pts_time - now_time)<vid_next_pts))
                        av_usleep(pts_time - now_time);

                    for(int i = 0 ; i < MAX_CLIENT_NUMBER; i++){
                        //get reader lock
                        int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
                        if(lock_ret) {
                            printf("Acquire SID %d rdlock error, ret = %d\n", i, lock_ret);
                            continue;
                        }

                        if(gClientInfo[i].avIndex < 0 || gClientInfo[i].bEnableAudio == 0) {
                            //release reader lock
                            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                            if(lock_ret)
                                printf("Acquire SID %d rdlock error, ret = %d\n", i, lock_ret);
                            continue;
                        }
                        // send audio data to av-idx
                        ret = avSendAudioData(gClientInfo[i].avIndex, (char*)output_packet.data, output_packet.size,
                                              &frameInfo, sizeof(FRAMEINFO_t));
                        //release reader lock
                        lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                        if(lock_ret) {
                            printf("Acquire SID %d rdlock error, ret = %d\n", i, lock_ret);
                        }

                        //printf("avIndex[%d] size[%d]\n", client_info[i].avIndex, size);
                        if(ret == AV_ER_NoERROR){
                            //qDebug() << "一帧音频发送成功";
                        }
                        else if (ret == AV_ER_SESSION_CLOSE_BY_REMOTE) {
                            printf("thread_AudioFrameData: AV_ER_SESSION_CLOSE_BY_REMOTE\n");
                            unregedit_client_from_audio(i);
                        } else if (ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT) {
                            printf("thread_AudioFrameData: AV_ER_REMOTE_TIMEOUT_DISCONNECT\n");
                            unregedit_client_from_audio(i);
                        } else if(ret == IOTC_ER_INVALID_SID) {
                            printf("Session cant be used anymore\n");
                            unregedit_client_from_audio(i);
                        } else if(ret < 0) {
                            printf("avSendAudioData error[%d]\n", ret);
                            unregedit_client_from_audio(i);
                        }
                    }
                    av_packet_unref(&output_packet);
                }
                av_frame_free(&output_frame);
            }

        }
    pthread_exit(0);
#endif

    return NULL;

}









MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui_init();

//    my_camera = new readcamera;
//    connect(my_camera,SIGNAL(emit_image(QImage)),this,SLOT(recv_image(QImage)));

    ffmpeg_init();
    if(device_init() != 0)
        return ;
    if(decoder_init() != 0)
        return ;
    if(encoder_init() != 0)
        return ;
    if(p2p_init() != 0)
        return ;

}

MainWindow::~MainWindow()
{
    delete my_camera;
    delete ui;
}

void MainWindow::ui_init()
{
    ui->m_label->setScaledContents(true);
    ui->password_lineEdit->setContextMenuPolicy(Qt::NoContextMenu);
    ui->password_lineEdit->setPlaceholderText(QString("password"));
    ui->password_lineEdit->setEchoMode(QLineEdit::Password);
}

void MainWindow::recv_image(QImage image)
{
    QPixmap pix;

    if (image.height()>0){
        pix = QPixmap::fromImage(image);
        ui->m_label->setPixmap(pix);
    }
    else{
        qDebug("没有获取到图像数据");
    }
}

void MainWindow::on_start_pushButton_clicked()
{
    if(ui->start_pushButton->text() == "start"){
        my_camera->start();
        ui->start_pushButton->setText("end");
    }
    else{
        my_camera->terminate();
        ui->start_pushButton->setText("start");
    }
}

