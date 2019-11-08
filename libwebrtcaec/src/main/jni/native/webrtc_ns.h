//
// Created by 28974 on 2019/10/31.
//

#ifndef ANDROID_WEBRTC_AEC_MASTER_WEBRTC_NS_H
#define ANDROID_WEBRTC_AEC_MASTER_WEBRTC_NS_H

void noise_suppression(char *in_file, char *out_file);

int nsProcess(int16_t *buffer, size_t sampleRate, int samplesCount, enum nsLevel level);
void FloatToS16(const float *src, size_t size, int16_t *dest);
void S16ToFloat(const int16_t *src, size_t size, float *dest);
int16_t FloatToS16_C(float v);
float S16ToFloat_C(int16_t v);
#endif //ANDROID_WEBRTC_AEC_MASTER_WEBRTC_NS_H
