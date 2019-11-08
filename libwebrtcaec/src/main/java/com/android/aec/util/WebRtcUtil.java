package com.android.aec.util;

import com.android.aec.conf.Setting;

import java.nio.ByteBuffer;

public class WebRtcUtil {
  static {
    if (Setting.loadLibrary() != null) {
      Setting.loadLibrary().loadLibrary();
    } else {
      System.loadLibrary("webrtc_android");
      System.loadLibrary("webrtc_jni");
    }
  }

  public static native int init(long rate);

  public static native void free();

  /**
   * 回声消除
   * @param far 播放声
   * @param near 录制声
   * @param out 回声消除声音
   * @param processlength pcm频率
   * @param delay 延迟
   * @param skew
   * @param near_v
   * @param far_v
   * @return
   */
  public static native int bufferFarendAndProcess(byte[] far, byte[] near, byte[] out, int processlength, int delay, int skew, float near_v, float far_v);

  public static native int test1();

  public   native  static  void  AgcFree();
  public   native  static  int   AgcFun(ByteBuffer buffer1 , short[] sArr, int frameSize);
  public   native  static  long  AgcInit(long minLevel, long maxLevel,long fs);
  public   native  static  long  AgcTransform(String file, String  outFile,int mode);
  public   native  static  long  noiseSuppression(String file, String  outFile);
  public native static void aec(String near ,String far,String out);
}
