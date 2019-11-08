#include "com_android_aec_util_WebRtcUtil.h"
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <android/log.h>
#include <string.h>
#include <audio_aec.h>
#include <noise_suppression.h>

#include  <stdio.h>
#include  <jni.h>

#include <aec_core.h>
#include <echo_cancellation.h>
#include <noise_suppression_x.h>
#include <analog_agc.h>
#include <gain_control.h>
#include <fcntl.h>


#define TAG "WEBRTC"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)

#define NS_ERROR  -1
#define NS_SUCCESS  7
#define AGC_SUCCESS  7

void* handle = NULL;
static pthread_mutex_t pthread_mutex;
static NsHandle  *pNS_inst = NULL;
static LegacyAgc *agcHandle = NULL;
unsigned  char* agc_buffer = NULL;
unsigned  char* ns_buffer = NULL;
JNIEXPORT jint JNICALL Java_com_android_aec_util_WebRtcUtil_init(JNIEnv *env,
		jclass obj, jlong rate) {
	static int lockinit = 0;
	if (lockinit == 0) {
		pthread_mutex_init(&pthread_mutex, NULL);
		lockinit = 1;
	}
	handle = audio_process_aec_create(rate);
	if (handle) {
		return 0;
	}
	return -1;
}

JNIEXPORT void JNICALL Java_com_android_aec_util_WebRtcUtil_free(JNIEnv * env,
		jclass obj) {
	pthread_mutex_lock(&pthread_mutex);
	if (handle != NULL) {
		audio_process_aec_free(handle);
		handle = NULL;
	}
	pthread_mutex_unlock(&pthread_mutex);
}

JNIEXPORT jint JNICALL Java_com_android_aec_util_WebRtcUtil_bufferFarendAndProcess(
		JNIEnv *env, jclass obj, jbyteArray far, jbyteArray near,
		jbyteArray out, jint length, jint delay, jint skew, jfloat near_v,
		jfloat far_v) {
	pthread_mutex_lock(&pthread_mutex);
	char *farbuff = (char *) (*env)->GetByteArrayElements(env, far, NULL);
	char *nearbuff = (char *) (*env)->GetByteArrayElements(env, near, NULL);
	char *outbuff = (char *) (*env)->GetByteArrayElements(env, out, NULL);
	struct aec_process_param param;
	param.far = farbuff;
	param.near = nearbuff;
	param.filter = outbuff;
	param.size = length;
	param.delay_time = delay;
	if (handle != NULL) {
		int ret = audio_process_aec_process(handle, &param, near_v, far_v);
	}
	(*env)->ReleaseByteArrayElements(env, far, (jbyte *) farbuff, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, near, (jbyte *) nearbuff, JNI_ABORT);
	(*env)->ReleaseByteArrayElements(env, out, (jbyte *) outbuff, 0);
	pthread_mutex_unlock(&pthread_mutex);

	return 0;
}
JNIEXPORT jint JNICALL Java_com_android_aec_util_WebRtcUtil_test1
  (JNIEnv *env, jclass clazz){

  return 0;
  }

JNIEXPORT jlong JNICALL Java_com_android_aec_util_WebRtcUtil_AgcInit(JNIEnv *env, jclass cls, jlong minLevel , jlong maxLevel , jlong fs){
        minLevel = 0;
		maxLevel = 255;
		int agcMode  = kAgcModeFixedDigital;
		agcHandle=WebRtcAgc_Create();
        if ( (   agcHandle ) != 0) { //allocate dynamic memory on native heap for NS instance pointed by hNS.
             LOGE("Noise_Suppression WebRtcNs_Create err! \n");
             return  NS_ERROR;  //error occurs

        }
        if (0 !=  WebRtcAgc_Init(agcHandle, minLevel, maxLevel, agcMode, fs) )
	    {
             LOGE("WebRtcAgc_Init WebRtcNs_Init err! \n");
             return  NS_ERROR;  //error occurs
	    }
        WebRtcAgcConfig agcConfig;
		agcConfig.compressionGaindB = 20; //在Fixed模式下，越大声音越大
		agcConfig.limiterEnable     = 1;
		agcConfig.targetLevelDbfs   = 3;  //dbfs表示相对于full scale的下降值，0表示full scale，越小声音越大
		WebRtcAgc_set_config(agcHandle, agcConfig);
		return NS_SUCCESS;
}
JNIEXPORT jint Java_com_android_aec_util_WebRtcUtil_AgcFun(JNIEnv *env, jclass type, jobject jdirectBuff,jshortArray sArr_, jint frameSize) {
    if(agc_buffer == NULL){
        LOGE("gc_buffer == NULL! \n");
        void* buffer = (*env)->GetDirectBufferAddress(env,jdirectBuff);
        agc_buffer = buffer;
    }
    uint8_t saturationWarning;
    int outMicLevel = 0;
    int micLevelOut = 0;
    int i =0 ;
    int inMicLevel  = micLevelOut;
    const short *pData    = NULL;
    short *pOutData    = NULL;
    pOutData = (short*)malloc(frameSize*sizeof(short));
    pData  =(*env)->GetShortArrayElements(env,sArr_,NULL);
    if(agcHandle == NULL){
        LOGE("agcHandle is null! \n");
        return  -3;
    }
    if(frameSize <= 0){
        return  -2;
    }
    int  agcProcessResult =  WebRtcAgc_Process(agcHandle,
                                               &pData,
                                               1,
                                               frameSize,
                                               &pOutData,
                                               inMicLevel,
                                               &outMicLevel,
                                               0,
                                               &saturationWarning);
    if (0 !=  agcProcessResult )
    {
        LOGE("failed in WebRtcAgc_Process!  agcProcessResult = %d \n" ,agcProcessResult);
        return  NS_ERROR ;  //error occurs
    }
    //memset(agc_buffer, 0,  160);
    shortTobyte(80,pOutData,agc_buffer);
//    (*env)->ReleaseShortArrayElements(env, sArr_, pData, 0);
    return  AGC_SUCCESS;
}
JNIEXPORT void JNICALL Java_com_android_aec_util_WebRtcUtil_AgcFree(JNIEnv *env , jclass  cls){
    WebRtcAgc_Free(agcHandle);
}
JNIEXPORT jint JNICALL  Java_com_android_aec_util_WebRtcUtil_processNS32K(JNIEnv *env, jclass type, jobject buffer1,
                                                jshortArray outframe_, jint sf) {
    jshort *outframe = (*env)->GetShortArrayElements(env, outframe_, NULL);
    (*env)->ReleaseShortArrayElements(env, outframe_, outframe, 0);
    return 0;
}
JNIEXPORT jint JNICALL  Java_com_android_aec_util_WebRtcUtil_AgcTransform(JNIEnv *env, jclass type, jstring file,
                                                jstring outfile, jint mode){
    char *filename=(char*)(*env)->GetStringUTFChars(env,file,NULL);
    char *outfilename=(char*)(*env)->GetStringUTFChars(env,outfile,NULL);
    void *agcInst = WebRtcAgc_Create();
    int minLevel = 0;
    int maxLevel = 255;
    int agcMode  = kAgcModeFixedDigital;
    int fs       = 8000;
    int status   = 0;
    status = WebRtcAgc_Init(agcInst, minLevel, maxLevel, agcMode, fs);
    if(status != 0)
    {
        printf("failed in WebRtcAgc_Init\n");
        return -1;
    }
    WebRtcAgcConfig agcConfig;
    agcConfig.compressionGaindB = 100;//在Fixed模式下，越大声音越大
    agcConfig.limiterEnable = 1;
    agcConfig.targetLevelDbfs = 0;   //dbfs表示相对于full scale的下降值，0表示full scale，越小声音越大
    status = WebRtcAgc_set_config(agcInst, agcConfig);
    if(status != 0)
    {
        printf("failed in WebRtcAgc_set_config\n");
        return -1;
    }


    //alloc
    FILE *infp=fopen(filename,"r");
    int nBands = 1;
    int frameSize = 80;//10ms对应于160个short
    short **pData = (short**)malloc(nBands*sizeof(short*));
    pData[0] = (short*)malloc(frameSize*sizeof(short));
    short **pOutData = (short**)malloc(nBands*sizeof(short*));
    pOutData[0] = (short*)malloc(frameSize*sizeof(short));

    //process
    FILE *outfp = fopen(outfilename,"w");
    int len = frameSize;
    int micLevelIn = 0;
    int micLevelOut = 0;
    while(len > 0)
    {
        memset(pData[0], 0, frameSize*sizeof(short));
        len = fread(pData[0], sizeof(short), frameSize, infp);

        int inMicLevel  = micLevelOut;
        int outMicLevel = 0;
        uint8_t saturationWarning;
        status = WebRtcAgc_Process(agcInst, pData, nBands, frameSize, pOutData, inMicLevel, &outMicLevel, 0, &saturationWarning);
        if (status != 0)
        {
            printf("failed in WebRtcAgc_Process\n");
            return -1;
        }
        if (saturationWarning == 1)
        {
            printf("saturationWarning\n");
        }
        micLevelIn = outMicLevel;
        //write file
        len = fwrite(pOutData[0], sizeof(short), len, outfp);
    }
    fclose(infp);
    fclose(outfp);

    WebRtcAgc_Free(agcInst);
    free(pData[0]);
    free(pData);
    free(pOutData[0]);
    free(pOutData);
}
JNIEXPORT jint JNICALL  Java_com_android_aec_util_WebRtcUtil_noiseSuppression(JNIEnv *env, jclass object, jstring file,
                                                jstring outfile){
        char *filename=(char*)(*env)->GetStringUTFChars(env,file,NULL);
        char *outfilename=(char*)(*env)->GetStringUTFChars(env,outfile,NULL);
        int fs       = 8000;
        int mode  = 4;

        NsxHandle  *pNS_inst=WebRtcNsx_Create();
        WebRtcNsx_Init(pNS_inst,fs);
        WebRtcNsx_set_policy(pNS_inst,mode);

        FILE *infp=fopen(filename,"r");
        int nBands = 1;
        int frameSize = 80;//10ms对应于160个short
        short **pData = (short**)malloc(nBands*sizeof(short*));
        pData[0] = (short*)malloc(frameSize*sizeof(short));
        short **pOutData = (short**)malloc(nBands*sizeof(short*));
        pOutData[0] = (short*)malloc(frameSize*sizeof(short));

         FILE *outfp = fopen(outfilename,"w");
        int len = frameSize;

        while(len > 0)
        {
            memset(pData[0], 0, frameSize*sizeof(short));
            len = fread(pData[0], sizeof(short), frameSize, infp);

            WebRtcNsx_Process(pNS_inst,pData,nBands,pOutData);
            //write file
            len = fwrite(pOutData[0], sizeof(short), len, outfp);
        }
        fclose(infp);
        fclose(outfp);

        WebRtcNsx_Free(pNS_inst);
        free(pData[0]);
        free(pData);
        free(pOutData[0]);
        free(pOutData);
}

void shortTobyte(int len , short *p , unsigned char *q){

    int i;
    for(i=0;i<len;i++)
    {
        q[2*i+1] = (unsigned char)(p[i]>>8);
        q[2*i] = p[i]&0x00ff;

    }
}
void shortToFloat(int len,short*p,float*q){
int i;
    for (i = 0; i < len; i++)
        {
            q[i] = *(p + i);

        }
}
void floatToShort(int len,float*p,short*q){
int i;
    for (i = 0; i < len; i++)
        {
             *(q + i)=(float)p[i];

        }
}
#define  NN 80
JNIEXPORT void JNICALL  Java_com_android_aec_util_WebRtcUtil_aec(JNIEnv *env, jclass obj, jstring near,
                                                jstring far,jstring out){
        char *c_near=(char*)(*env)->GetStringUTFChars(env,near,NULL);
        char *c_far=(char*)(*env)->GetStringUTFChars(env,far,NULL);
        char *c_out=(char*)(*env)->GetStringUTFChars(env,out,NULL);
        FILE *fp_far  = fopen(c_far, "rb");
        FILE *fp_near = fopen(c_near, "rb");
        FILE *fp_out  = fopen(c_out, "wb");

        int nBands = 1;
        int frameSize = 80;//10ms对应于160个short

        short *sFarPointer = (short*)malloc(frameSize*sizeof(short));
        short *sNearPointer = (short*)malloc(frameSize*sizeof(short));
        short *sOutPointer= (short*)malloc(frameSize*sizeof(short));

        float m_sOutNear_frame[NN];
	    float m_sFar_frame[NN];
	    float m_sNear_frame[NN];

	    memset(m_sNear_frame, 0, sizeof(float) * NN);
	    memset(m_sFar_frame, 0, sizeof(float) * NN);
        memset(m_sOutNear_frame, 0, sizeof(float) * NN);


        float* const p = m_sNear_frame;
        const float* const*  ptr = &p;

        float* const q = m_sOutNear_frame;
        float* const* ptr2 = &q;

         float* const farq = m_sFar_frame;

        void* aecmInst=WebRtcAec_Create();
        WebRtcAec_Init(aecmInst,8000,8000);
        AecConfig config;
        config.nlpMode = kAecNlpConservative;
        WebRtcAec_set_config(aecmInst, config);
        while(1){
//            memset(farData[0], 0, frameSize*sizeof(short));
            if (NN == fread(sFarPointer, sizeof(short), NN, fp_far)){
//                memset(sNearPointer, 0, frameSize*sizeof(short));
                shortToFloat(NN,sFarPointer,m_sFar_frame);
                fread(sNearPointer, sizeof(short), NN, fp_near);
                shortToFloat(NN,sNearPointer,m_sNear_frame);
                WebRtcAec_BufferFarend(aecmInst, farq, NN);//对参考声音(回声)的处理



                WebRtcAec_Process(aecmInst, ptr, 1, ptr2, NN, 109, 0);

                floatToShort(NN,m_sOutNear_frame,sOutPointer);
                fwrite(sOutPointer, sizeof(short), NN, fp_out);
            }else{
                break;
            }
        }

        fclose(fp_far);
        fclose(fp_near);
        fclose(fp_out);

        WebRtcAec_Free(aecmInst);
    //    free(pData[0]);
      //  free(pData);
        //free(pOutData[0]);
        //free(pOutData);

}
/*
 #ifdef __cplusplus
 }
 #endif
 */

