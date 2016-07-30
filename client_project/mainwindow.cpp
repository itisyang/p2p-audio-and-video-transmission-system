#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AVAPIs.h"
#include "IOTCAPIs.h"
#include "P2PTunnelAPIs.h"
#include "RDTAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"
#include <sys/time.h>
#include <unistd.h>
#include <QLabel>
#include <QMediaPlayer>
#include <QAudioFormat>
#include <QAudioOutput>
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

void *label;
int Width = 640,Height = 480;

char avID[]="admin";
char avPass[]="888888";

int SID = 0;
int avIndex = 0;
int PLAY_FLAG = 0;

typedef struct _AV_Server
{
    char UID[21];
    int SID;
    int speakerCh;
}AV_Server;

AV_Server *ServerInfo;

#define VIDEO_BUF_SIZE 128000
#define AUDIO_BUF_SIZE	1024
int gSleepMs = 10;


AVCodec *pAVCodec;
AVCodecContext *pAVCodecContext;
AVPacket pAVPacket;
AVFrame *pAVFrame;

AVCodec *pAVCodec_a;
AVCodecContext *pAVCodecContext_a;
AVPacket pAVPacket_a;
AVFrame *pAVFrame_a;

int first = 0;

SwsContext *pSwsContext;
AVPicture  pAVPicture;

void ffmpeg_init()
{
//Initialize libavformat and register all the muxers, demuxers and protocols
    av_register_all();
//Register Device
    avdevice_register_all();
//initialization of network components
    avformat_network_init();
}


void *thread_ReceiveVideo(void *arg)
{
    printf("[thread_ReceiveVideo] Starting....\n");

    int avIndex = *(int *)arg;
    char buf[VIDEO_BUF_SIZE]={0};
    int ret;

    FRAMEINFO_t frameInfo;
    unsigned int frmNo;
    struct timeval tv, tv2;
    printf("Start IPCAM video stream OK!\n");
    int /*flag = 0,*/ cnt = 0, fpsCnt = 0, round = 0, lostCnt = 0;
    int outBufSize = 0;
    int outFrmSize = 0;
    int outFrmInfoSize = 0;
    //int bCheckBufWrong;
    int bps = 0;
    gettimeofday(&tv, NULL);

    //初始化图片空间
    avpicture_alloc(&pAVPicture,AV_PIX_FMT_RGB24,Width,Height);
    //初始化缩放规则
    pSwsContext = sws_getContext(Width, Height, AV_PIX_FMT_YUV420P,
                                 Width,Height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);

    while(1)
    {
        //ret = avRecvFrameData(avIndex, buf, VIDEO_BUF_SIZE, (char *)&frameInfo, sizeof(FRAMEINFO_t), &frmNo);
        ret = avRecvFrameData2(avIndex, buf, VIDEO_BUF_SIZE, &outBufSize, &outFrmSize, (char *)&frameInfo, sizeof(FRAMEINFO_t), &outFrmInfoSize, &frmNo);

        // show Frame Info at 1st frame
        if(frmNo == 0)
        {
            //printf("%u  %x   %d\n",frameInfo.codec_id,frameInfo.codec_id,frameInfo.codec_id);
//            const char *format[] = {"MPEG4","H263","H264","MJPEG","UNKNOWN"};
//            int idx = 0;
//            if(frameInfo.codec_id == MEDIA_CODEC_VIDEO_MPEG4)
//                idx = 0;
//            else if(frameInfo.codec_id == MEDIA_CODEC_VIDEO_H263)
//                idx = 1;
//            else if(frameInfo.codec_id == MEDIA_CODEC_VIDEO_H264)
//                idx = 2;
//            else if(frameInfo.codec_id == MEDIA_CODEC_VIDEO_MJPEG)
//                idx = 3;
//            else
//                idx = 4;
//            printf("--- Video Formate: %s ---\n", format[idx]);
//            if(idx != 2)
//                continue;
        }
//        qDebug("frmNo = %d",frmNo);

        if(ret == AV_ER_DATA_NOREADY)
        {
//            printf("AV_ER_DATA_NOREADY[%d]\n", avIndex);
            usleep(gSleepMs * 1000);
            continue;
        }
        else if(ret == AV_ER_LOSED_THIS_FRAME)
        {
            printf("Lost video frame NO[%d]\n", frmNo);
            lostCnt++;
            continue;
        }
        else if(ret == AV_ER_INCOMPLETE_FRAME)
        {
            if(outFrmInfoSize > 0)
            printf("Incomplete video frame NO[%d] ReadSize[%d] FrmSize[%d] FrmInfoSize[%u] Codec[%d] Flag[%d]\n", frmNo, outBufSize, outFrmSize, outFrmInfoSize, frameInfo.codec_id, frameInfo.flags);
            else
            printf("Incomplete video frame NO[%d] ReadSize[%d] FrmSize[%d] FrmInfoSize[%u]\n", frmNo, outBufSize, outFrmSize, outFrmInfoSize);
            lostCnt++;
        }
        else if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE)
        {
            printf("[thread_ReceiveVideo] AV_ER_SESSION_CLOSE_BY_REMOTE\n");
            break;
        }
        else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT)
        {
            printf("[thread_ReceiveVideo] AV_ER_REMOTE_TIMEOUT_DISCONNECT\n");
            break;
        }
        else if(ret == IOTC_ER_INVALID_SID)
        {
            printf("[thread_ReceiveVideo] Session cant be used anymore\n");
            break;
        }
        else
        {
            bps += outBufSize;
        }

//        printf("-----------------\n");

        //解码h264,显示
        pAVPacket.data = (unsigned char*)buf;
        pAVPacket.size = outBufSize;
        int got_frame;

        avcodec_decode_video2(pAVCodecContext,pAVFrame,&got_frame,&pAVPacket);
        if(got_frame){
                //printf("success decode  h264\n");
            if(PLAY_FLAG == 1)
            {
                sws_scale(pSwsContext,(const uint8_t* const *)pAVFrame->data,
                          pAVFrame->linesize, 0, Height,pAVPicture.data, pAVPicture.linesize);
                //第一个参数为sws_getContext函数返回的值
                //第四个参数为从输入图像数据的第多少列开始逐行扫描，通常设为0
                //第五个参数为需要扫描多少行，通常为输入图像数据的高度
                QImage image(pAVPicture.data[0], Width,Height, QImage::Format_RGB888);
                QPixmap pix = QPixmap::fromImage(image);
                QLabel *label_temp = (QLabel *)label;
                label_temp->setPixmap(pix);
            }
        }

//        fpsCnt++;
//        gettimeofday(&tv2, NULL);
//        long sec = tv2.tv_sec-tv.tv_sec, usec = tv2.tv_usec-tv.tv_usec;
//        if(usec < 0)
//        {
//            sec--;
//            usec += 1000000;
//        }
//        usec += (sec*1000000);
//        int FPS = 1000000/usec;

//        if(usec > 1000000)
//        {
//            round++;
//                    avIndex, fpsCnt, lostCnt, cnt, outFrmSize, frameInfo.codec_id, frameInfo.flags, (bps/1024)*8);
//            gettimeofday(&tv, NULL);
//            fpsCnt = 0;
//            bps = 0;
//        }


    }



    //close_videoX(fd);
    printf("[thread_ReceiveVideo] thread exit\n");

    return 0;
}


void* thread_ReceiveAudio(void *arg)
{
    printf("[thread_ReceiveAudio] Starting....\n");

    int avIndex = *(int *)arg;
    char buf[AUDIO_BUF_SIZE];

    FRAMEINFO_t frameInfo;
    unsigned int frmNo;
    int recordCnt = 0;
    int ret;

    //设置采样格式
    QAudioFormat audioFormat;
    //设置采样率
    audioFormat.setSampleRate(44100);
    //设置通道数
    audioFormat.setChannelCount(2);
    //设置采样大小，一般为8位或16位
    audioFormat.setSampleSize(16);
    //设置编码方式
    audioFormat.setCodec("audio/pcm");
    //设置字节序
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    //设置样本数据类型
    audioFormat.setSampleType(QAudioFormat::SignedInt);

    QAudioOutput *audio = new QAudioOutput(audioFormat, 0);

    QIODevice *out = audio->start();

    while(1){
//        printf("ReceiveAudio\n");
        ret = avCheckAudioBuf(avIndex);
        if(ret < 0) break;
        if(ret < 30) // determined by audio frame rate
        {
            usleep(100000);//0.1s
            continue;
        }
        ret = avRecvAudioData(avIndex, buf, AUDIO_BUF_SIZE, (char *)&frameInfo, sizeof(FRAMEINFO_t), &frmNo);

        if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE)
        {
            printf("[thread_ReceiveAudio] AV_ER_SESSION_CLOSE_BY_REMOTE\n");
            break;
        }
        else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT)
        {
            printf("[thread_ReceiveAudio] AV_ER_REMOTE_TIMEOUT_DISCONNECT\n");
            break;
        }
        else if(ret == IOTC_ER_INVALID_SID)
        {
            printf("[thread_ReceiveAudio] Session cant be used anymore\n");
            break;
        }
        else if(ret == AV_ER_LOSED_THIS_FRAME)
        {
            printf("AV_ER_LOSED_THIS_FRAME[%d]\n", frmNo);
            continue;
        }
        else if(ret < 0)
        {
            printf("Other error[%d]!!!\n", ret);
            continue;
        }

        //音频解码

    }
}



/*********
Send IOCtrl CMD to device
***/
int start_ipcam_stream(int avIndex_temp)
{
    int ret;
    unsigned short val = 0;



    if((ret = avSendIOCtrl(avIndex_temp, IOTYPE_INNER_SND_DATA_DELAY, (char *)&val, sizeof(unsigned short)) < 0)){
        printf("start_ipcam_stream failed[%d]\n", ret);
        return 0;
    }
    printf("send Cmd: IOTYPE_INNER_SND_DATA_DELAY, OK\n");

    SMsgAVIoctrlAVStream ioMsg;
    memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));

    ret = avSendIOCtrl(avIndex_temp, IOTYPE_USER_IPCAM_START,(char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
    if(ret < 0){
        printf("start_ipcam_stream failed[%d]\n", ret);
        return 0;
    }
    printf("send Cmd: IOTYPE_USER_IPCAM_START, OK\n");

    ret = avSendIOCtrl(avIndex_temp, IOTYPE_USER_IPCAM_AUDIOSTART, (char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
    if(ret < 0){
        printf("start_ipcam_stream failed[%d]\n", ret);
        return 0;
    }
    printf("send Cmd: IOTYPE_USER_IPCAM_AUDIOSTART, OK\n");




    return 1;
}

int stop_ipcam_stream(int avIndex_temp)
{
    int ret;
    unsigned short val = 0;

    if(PLAY_FLAG == 1){

        PLAY_FLAG = 0;

//        if((ret = avSendIOCtrl(avIndex_temp, IOTYPE_INNER_SND_DATA_DELAY, (char *)&val, sizeof(unsigned short)) < 0)){
//            printf("start_ipcam_stream failed[%d]\n", ret);
//            return 0;
//        }
//        printf("send Cmd: IOTYPE_INNER_SND_DATA_DELAY, OK\n");

//        SMsgAVIoctrlAVStream ioMsg;
//        memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));

//        ret = avSendIOCtrl(avIndex_temp, IOTYPE_USER_IPCAM_STOP,(char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
//        if(ret < 0){
//            printf("stop_ipcam_stream failed[%d]\n", ret);
//            return 0;
//        }
//        printf("send Cmd: IOTYPE_USER_IPCAM_STOP, OK\n");

//        ret = avSendIOCtrl(avIndex_temp, IOTYPE_USER_IPCAM_AUDIOSTOP, (char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
//        if(ret < 0){
//            printf("stop_ipcam_stream failed[%d]\n", ret);
//            return 0;
//        }
//        printf("send Cmd: IOTYPE_USER_IPCAM_AUDIOSTOP, OK\n");
    }
    else{
        PLAY_FLAG = 1;
    }

    return 1;
}



void *thread_ConnectCCR(void *arg)
{
    AV_Server *ServerInfo = (AV_Server *)arg;
    int tmpSID = IOTC_Get_SessionID();
    if(tmpSID < 0){
        printf("IOTC_Get_SessionID error code [%d]\n", tmpSID);
        return 0;
    }
    printf("IOTC_Get_SessionID, ret=[%d]\n", tmpSID);

    SID = IOTC_Connect_ByUID_Parallel(ServerInfo->UID, tmpSID);
    ServerInfo->SID = SID;
    printf("IOTC_Connect_ByUID_Parallel, ret=[%d]\n", SID);
    if(SID < 0){
        printf("IOTC_Connect_ByUID_Parallel failed[%d]\n", SID);
        return 0;
    }

    struct st_SInfo Sinfo;
    memset(&Sinfo, 0, sizeof(struct st_SInfo));

    const char *mode[] = {"P2P", "RLY", "LAN"};

    int nResend;
    unsigned int srvType;
    // The avClientStart2 will enable resend mechanism. It should work with avServerStart3 in device.
    //int avIndex = avClientStart(SID, avID, avPass, 20, &srvType, 0);
    avIndex = avClientStart2(SID, avID, avPass, 20, &srvType, 0, &nResend);
    if(nResend == 0){
        printf("Resend is not supported.");
    }
    if(avIndex < 0){
        printf("avClientStart2 failed[%d]\n", avIndex);
        return 0;
    }
    //确认连接状态并获得会话信息
    if(IOTC_Session_Check(SID, &Sinfo) == IOTC_ER_NoERROR){
        if( isdigit( Sinfo.RemoteIP[0] ))
            printf("Device is from %s:%d[%s] Mode=%s \n",
                   Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID, mode[(int)Sinfo.Mode]);
    }
    printf("avClientStart2 OK[%d], Resend[%d]\n", avIndex, nResend);

    avClientCleanBuf(avIndex);

    start_ipcam_stream(avIndex);

    pthread_t ThreadVideo_ID = 0, ThreadAudio_ID = 0;
    // Create Video Recv thread
    if ( (pthread_create(&ThreadVideo_ID, NULL, &thread_ReceiveVideo, (void *)&avIndex)) ){
        printf("Create Video Receive thread failed\n");
        exit(-1);
    }
    // Create Audio Recv thread
    if ( (pthread_create(&ThreadAudio_ID, NULL, &thread_ReceiveAudio, (void *)&avIndex)) ){
        printf("Create Audio Receive thread failed\n");
        exit(-1);
    }

    if( ThreadVideo_ID!=0){
        //pthread_join(ThreadVideo_ID, NULL);
        pthread_detach(ThreadVideo_ID);
    }
    if( ThreadAudio_ID!=0){
        //pthread_join(ThreadAudio_ID, NULL);
        pthread_detach(ThreadAudio_ID);
    }



    return NULL;
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //ui->play_label->setScaledContents(true);
    label = ui->play_label;
    ui->play_label->clear();

    //init P2P server
    IOTC_Initialize2(0);
    // alloc 3 sessions for video and two-way audio
    avInitialize(3);



    ffmpeg_init();

    pAVCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if(!pAVCodec){
        printf("avcodec_find_decoder fail");
        return ;
    }
    pAVCodecContext = avcodec_alloc_context3(pAVCodec);

    pAVCodecContext->time_base.num = 1;
    pAVCodecContext->frame_number = 1;  //每包一个视频帧
    pAVCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    pAVCodecContext->bit_rate = 0;
    pAVCodecContext->time_base.den = 25;//帧率
    if(!pAVCodecContext){
        qDebug("pAVCodecContext = NULL");
        return ;
    }

    if(pAVCodec->capabilities & CODEC_CAP_TRUNCATED){
        pAVCodecContext->flags |= CODEC_FLAG_TRUNCATED;
    }

    if(avcodec_open2(pAVCodecContext,pAVCodec,NULL) < 0){
        qDebug("avcodec_open2 fail");
        avcodec_free_context(&pAVCodecContext);
        return ;
    }

    pAVFrame = av_frame_alloc();
    if(!pAVFrame){
        qDebug("av_frame_alloc fail");
        avcodec_close(pAVCodecContext);
        avcodec_free_context(&pAVCodecContext);
        return ;
    }




    //音频解码器
    pAVCodec_a = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if(!pAVCodec_a){
        printf("avcodec_find_decoder fail");
        return ;
    }
    pAVCodecContext_a = avcodec_alloc_context3(pAVCodec_a);

    pAVCodecContext_a->time_base.num = 1;
    pAVCodecContext_a->frame_number = 1;  //每包一个视频帧
    pAVCodecContext_a->codec_type = AVMEDIA_TYPE_AUDIO;
    pAVCodecContext_a->bit_rate = 0;
    pAVCodecContext_a->time_base.den = 25;//帧率
    if(!pAVCodecContext_a){
        qDebug("pAVCodecContext = NULL");
        return ;
    }

    if(pAVCodec_a->capabilities & CODEC_CAP_TRUNCATED){
        pAVCodecContext_a->flags |= CODEC_FLAG_TRUNCATED;
    }

    if(avcodec_open2(pAVCodecContext_a,pAVCodec_a,NULL) < 0){
        qDebug("avcodec_open2 fail");
        avcodec_free_context(&pAVCodecContext_a);
        return ;
    }

    pAVFrame_a = av_frame_alloc();
    if(!pAVFrame_a){
        qDebug("av_frame_alloc fail");
        avcodec_close(pAVCodecContext_a);
        avcodec_free_context(&pAVCodecContext_a);
        return ;
    }



}

MainWindow::~MainWindow()
{
    avDeInitialize();
    IOTC_DeInitialize();

    if(avIndex != 0)
        avClientStop(avIndex);
    printf("avClientStop OK\n");
    if(SID != 0)
        IOTC_Session_Close(SID);
    printf("SID[%d] IOTC_Session_Close, OK\n", SID);
    if(ServerInfo != NULL)
        free(ServerInfo);


    printf("StreamClient exit...\n");

    delete ui;
}

void MainWindow::on_start_pushButton_clicked()
{
    avClientCleanBuf(avIndex);

    if(ui->start_pushButton->text() == "start"){
        PLAY_FLAG = 1;
        ui->start_pushButton->setText("stop");
    }
    else{
        PLAY_FLAG = 0;
        ui->start_pushButton->setText("start");
    }
}

void MainWindow::on_init_p2p_pushButton_clicked()
{
    int ret;
    const char *UID = "ZSCYLVKMKPYERAA9111A";
    //H2WB5KJU4UMERAPY111A
    //ZSCYLVKMKPYERAA9111A

    ServerInfo = (AV_Server *)malloc(sizeof(AV_Server));
    strcpy(ServerInfo->UID, UID);

    pthread_t ConnectThread_ID;
    ret = pthread_create(&ConnectThread_ID, NULL, &thread_ConnectCCR, (void *)ServerInfo);
    if(ret != 0){
        printf("pthread_create(ConnectThread_ID), ret=[%d]\n", ret);
        exit(-1);
    }
    pthread_detach(ConnectThread_ID);

    ui->init_p2p_pushButton->setEnabled(0);
}
