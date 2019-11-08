//
// Created by 28974 on 2019/10/31.
//

void noise_suppression(char *in_file, char *out_file) {
    //音频采样率
    uint32_t sampleRate = 0;
    //总音频采样数
    uint64_t inSampleCount = 0;
    int16_t *inBuffer = wavRead_int16(in_file, &sampleRate, &inSampleCount);

    //如果加载成功
    if (inBuffer != nullptr) {
        nsProcess(inBuffer, sampleRate, inSampleCount, kVeryHigh);
        wavWrite_int16(out_file, inBuffer, sampleRate, inSampleCount);

        free(inBuffer);
    }
}
int nsProcess(int16_t *buffer, size_t sampleRate, int samplesCount, enum nsLevel level) {
    if (buffer == nullptr) return -1;
    if (samplesCount == 0) return -1;
    size_t samples = WEBRTC_SPL_MIN(160, sampleRate / 100);
    if (samples == 0) return -1;
    const int maxSamples = 320;
    int num_bands = 1;
    int16_t *input = buffer;
    size_t nTotal = (samplesCount / samples);

    NsHandle *nsHandle = WebRtcNs_Create();

    int status = WebRtcNs_Init(nsHandle, sampleRate);
    if (status != 0) {
        printf("WebRtcNs_Init fail\n");
        return -1;
    }
    status = WebRtcNs_set_policy(nsHandle, level);
    if (status != 0) {
        printf("WebRtcNs_set_policy fail\n");
        return -1;
    }
    for (int i = 0; i < nTotal; i++) {
        float inf_buffer[maxSamples];
        float outf_buffer[maxSamples];
        S16ToFloat(input, samples, inf_buffer);
        float *nsIn[1] = {inf_buffer};   //ns input[band][data]
        float *nsOut[1] = {outf_buffer};  //ns output[band][data]
        WebRtcNs_Analyze(nsHandle, nsIn[0]);
        WebRtcNs_Process(nsHandle, (const float *const *) nsIn, num_bands, nsOut);
        FloatToS16(outf_buffer, samples, input);
        input += samples;
    }
    WebRtcNs_Free(nsHandle);

    return 1;
}
int16_t FloatToS16_C(float v) {
    static const float kMaxRound = (float) INT16_MAX - 0.5f;
    static const float kMinRound = (float) INT16_MIN + 0.5f;
    if (v > 0) {
        v *= kMaxRound;
        return v >= kMaxRound ? INT16_MAX : (int16_t) (v + 0.5f);
    }

    v *= -kMinRound;
    return v <= kMinRound ? INT16_MIN : (int16_t) (v - 0.5f);
}
void FloatToS16(const float *src, size_t size, int16_t *dest) {
    size_t i;
    for (i = 0; i < size; ++i)
        dest[i] = FloatToS16_C(src[i]);
}
void S16ToFloat(const int16_t *src, size_t size, float *dest) {
    size_t i;
    for (i = 0; i < size; ++i)
        dest[i] = S16ToFloat_C(src[i]);
}
float S16ToFloat_C(int16_t v) {
    if (v > 0) {
        return ((float) v) / (float) INT16_MAX;
    }

    return (((float) v) / ((float) -INT16_MIN));
}