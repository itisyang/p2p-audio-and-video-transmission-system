#ifndef PUBLIC_HEADER_H
#define PUBLIC_HEADER_H


int p2p_init();
void *thread_listen_client(void *arg);
void Handle_IOCTRL_Cmd(int SID, int avIndex, char *buf, int type);
void *thread_ForAVServerStart(void *arg);

void regedit_client_to_video(int SID, int avIndex);
void regedit_client_to_audio(int SID, int avIndex);
void unregedit_client_from_video(int SID);
void unregedit_client_from_audio(int SID);

void CALLBACK LoginInfoCB(unsigned int nLoginInfo);
int CALLBACK AuthCallBackFn(char *viewAcc,char *viewPwd);
void *thread_Login(void *arg);

void DeInitAVInfo();
void InitAVInfo();

void create_streamout_thread();
void *thread_VideoFrameData(void *arg);
void *thread_AudioFrameData(void *arg);



void ffmpeg_init();
int device_init();
int decoder_init();
int encoder_init();


typedef struct _AV_Client
{
    int avIndex;
    unsigned char bEnableAudio;
    unsigned char bEnableVideo;
    unsigned char bEnableSpeaker;
    unsigned char bStopPlayBack;
    unsigned char bPausePlayBack;
    int speakerCh;
    int playBackCh;
    //SMsgAVIoctrlPlayRecord playRecord;
    pthread_rwlock_t sLock;
}AV_Client;


#endif // PUBLIC_HEADER_H
