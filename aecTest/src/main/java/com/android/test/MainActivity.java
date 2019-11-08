package com.android.test;

import android.Manifest;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.StrictMode;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;

import com.android.aec.api.AudioFrame;
import com.android.aec.api.AudioPlayer;
import com.android.aec.api.Frequency;
import com.android.aec.api.OnTSUpdateListener;
import com.android.aec.api.RecodeAudioDataListener;
import com.android.aec.util.AudioUtils;
import com.android.aec.util.BytesTransUtil;
import com.android.aec.util.WebRtcUtil;
import com.tbruyelle.rxpermissions2.RxPermissions;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends FragmentActivity implements RecodeAudioDataListener, OnTSUpdateListener {
  AudioPlayer audioPlayer;
  File mFilterFile,mFilterFile2, mRecodeFile,agcFile,nsFile;
  BufferedOutputStream mRecodeStream;
  Thread mPlayThread;
  Thread mSendThread;
  volatile boolean isPlay;
  Frequency hz = Frequency.PCM_8K;
  int frameSize = 800;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
      StrictMode.VmPolicy.Builder builder = new StrictMode.VmPolicy.Builder();
      StrictMode.setVmPolicy(builder.build());
    }
    setContentView(R.layout.activity_main);
    mFilterFile = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/test_filter.pcm");
    mFilterFile2 = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/test_filter2.pcm");
    mRecodeFile = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/test_recode.pcm");
    agcFile = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/agcFile.pcm");
    nsFile = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/nsFile.pcm");
    new RxPermissions(this).request(
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO).subscribe();
  }

  public void onClick(View v) {
    if (isPlay) {
      return;
    }
    v.setEnabled(false);
    isPlay = true;
    mRecodeStream = openStreamByFile(mRecodeFile);

    audioPlayer = AudioPlayer.getInstance().disableSystemAEC().init(this, this, hz, frameSize);
    audioPlayer.setPtsUpdateListener(this);
    audioPlayer.start(true, true);
    mPlayThread = new Thread("PlayThread") {
      public void run() {
        BufferedInputStream in_far = null;
        try {
          in_far = new BufferedInputStream(getResources().getAssets().open(Frequency.PCM_8K == hz ? "far_8k.pcm" : "far_16k.pcm"));
          byte[] data = new byte[frameSize];
          int length = 0;
          AudioFrame frame = new AudioFrame();
          while (isPlay) {
            length = in_far.read(data, 0, data.length);
            if (length == data.length) {
//							Thread.sleep(70);
              frame.setAudioData(data, length);
              frame.setTimeStamp(System.currentTimeMillis());
              Log.i("TAG", "time:" + System.currentTimeMillis());
              while (!audioPlayer.putPlayData(frame)) {
                Log.i("TAG", "putPlayData sleep");
                try {
                  Thread.sleep(10);
                } catch (Exception e) {
                  e.printStackTrace();
                }
              }
              length = 0;
            } else {
              Log.e("TAG", "in_far.close");
              in_far.close();
              Thread.sleep(5000);
              in_far = new BufferedInputStream(getResources().getAssets().open(Frequency.PCM_8K == hz ? "far.pcm" : "far2.pcm"));
            }
          }
        } catch (Throwable e) {
          e.printStackTrace();
        } finally {
          if (in_far != null) {
            try {
              in_far.close();
            } catch (IOException e) {
              e.printStackTrace();
            }
          }
        }
      }
    };
    mPlayThread.start();
    mSendThread = new Thread("SendThread") {
      public void run() {
        BufferedOutputStream out = null;
        byte[] filterData = null;
        try {
          out = openStreamByFile(mFilterFile);
          while (isPlay) {
            filterData = audioPlayer.getFilterData();
            if (filterData != null && out != null) {
              Log.i("TAG", "filter Data size =" + filterData.length);
              out.write(filterData);
            } else {
              Log.i("TAG", "filter Data size = null");
              Thread.sleep(10);
            }

          }

        } catch (Throwable e) {
          e.printStackTrace();
        } finally {
          try {
            if (out != null) {
              out.flush();
              out.close();
            }
          } catch (IOException e) {
            e.printStackTrace();
          }
        }
      }

      ;
    };
    mSendThread.start();
  }

  private void agc() throws IOException {
    long res = WebRtcUtil.AgcInit(0, 255, hz.value());
    BytesTransUtil bytesTransUtil = BytesTransUtil.getInstance();
    // rData 为读取的缓冲区 分配160字节
    byte[] rData = new byte[160];
    ByteBuffer outBuffer = ByteBuffer.allocateDirect(160);
    FileInputStream is=new FileInputStream(mFilterFile);
    BufferedOutputStream fos = openStreamByFile(agcFile);
    int nbread = 0;
    //--------------------开始读取---------------------------
    while((nbread=is.read(rData))>-1){
      short[] shorts = bytesTransUtil.Bytes2Shorts(rData);
      res = WebRtcUtil.AgcFun(outBuffer, shorts,80);
      for (int i = 0 ;i< 80 ;i++){

        shorts[i] = (short) (outBuffer.get(2*i) + ( outBuffer.get(2*i+1) << 8));
      }
      fos.write(bytesTransUtil.Shorts2Bytes(shorts),0,nbread);
    }
  }

  /**
   * 停止回声消除
   *
   * @param v
   */
  public void stopAEC(View v) {
    isPlay = false;
    try {
      if (mPlayThread != null) {
        mPlayThread.interrupt();
        mPlayThread = null;
      }
//			if (mSendThread != null) {
//				mSendThread.interrupt();
//				mSendThread = null;
//			}
      if (mRecodeStream != null) {
        mRecodeStream.flush();
        mRecodeStream.close();
        mRecodeStream = null;
      }
      audioPlayer.stop();
      audioPlayer.release();
    } catch (Exception e) {
      e.printStackTrace();
    }
    findViewById(R.id.btn_strat).setEnabled(true);
  }
  public void processFilterSound(View v){
    new Thread(new Runnable() {

      @Override
      public void run() {
        try {
          String outPath=Environment.getExternalStorageDirectory().getAbsolutePath()+ "/";
          String far=outPath + "speaker.pcm";
          String near=outPath + "micin.pcm";
          Log.e("sdfsdfsdfsdf",new File(far).exists()+"===far");
          Log.e("sdfsdfsdfsdf",new File(near).exists()+"===near");
          if(mFilterFile2.exists()){
            mFilterFile2.delete();
          }
          WebRtcUtil.aec(near,far,mFilterFile2.getPath());
          if(mFilterFile2.exists()){
            Log.e("sdfsdfsdf","234");
          }
          AudioTrack audioTrack = AudioUtils.createTracker(new int[1], hz.value(), hz.value() / 10, false);
          if (mFilterFile2.exists() && mFilterFile2.length() > 0) {
            BufferedInputStream in = new BufferedInputStream(new FileInputStream(mFilterFile2));
            byte[] data = new byte[hz.value() / 10];
            int len = 0;
            while ((len = in.read(data)) > 0) {
              audioTrack.play();
              audioTrack.write(data, 0, len);
            }
            audioTrack.release();
          }

        } catch (Throwable t) {
          t.printStackTrace();
        }
      }
    }).start();
  }
  /**
   * 播放回声消除后的音频文件
   *
   * @param v
   */
  public void playFilterSound(View v) {
    new Thread(new Runnable() {

      @Override
      public void run() {
        try {
//          agc();
          AudioTrack audioTrack = AudioUtils.createTracker(new int[1], hz.value(), hz.value() / 10, false);
          if (mFilterFile.exists() && mFilterFile.length() > 0) {
            BufferedInputStream in = new BufferedInputStream(new FileInputStream(mFilterFile));
            byte[] data = new byte[hz.value() / 10];
            int len = 0;
            while ((len = in.read(data)) > 0) {
              audioTrack.play();
              audioTrack.write(data, 0, len);
            }
            audioTrack.release();
          }
        } catch (Throwable t) {
          t.printStackTrace();
        }

      }
    }).start();
  }
  /**
   * 播放增益音频文件
   *
   * @param v
   */
  public void playFilterSound2(View v) {
    new Thread(new Runnable() {

      @Override
      public void run() {
        try {
//          agc();
          WebRtcUtil.AgcTransform(mFilterFile2.getPath(),agcFile.getPath(),1);
          AudioTrack audioTrack = AudioUtils.createTracker(new int[1], hz.value(), hz.value() / 10, false);
          if (agcFile.exists() && agcFile.length() > 0) {
            BufferedInputStream in = new BufferedInputStream(new FileInputStream(agcFile));
            byte[] data = new byte[hz.value() / 10];
            int len = 0;
            while ((len = in.read(data)) > 0) {
              audioTrack.play();
              audioTrack.write(data, 0, len);
            }
            audioTrack.release();
          }
        } catch (Throwable t) {
          t.printStackTrace();
        }

      }
    }).start();
  }
  /**
   * 播放降噪的音频文件
   *
   * @param v
   */
  public void playFilterSound3(View v) {
    new Thread(new Runnable() {

      @Override
      public void run() {
        try {
          WebRtcUtil.AgcTransform(mFilterFile.getPath(),agcFile.getPath(),1);
          WebRtcUtil.noiseSuppression(agcFile.getPath(),nsFile.getPath());
          AudioTrack audioTrack = AudioUtils.createTracker(new int[1], hz.value(), hz.value() / 10, false);
          if (nsFile.exists() && nsFile.length() > 0) {
            BufferedInputStream in = new BufferedInputStream(new FileInputStream(nsFile));
            byte[] data = new byte[hz.value() / 10];
            int len = 0;
            while ((len = in.read(data)) > 0) {
              audioTrack.play();
              audioTrack.write(data, 0, len);
            }
            audioTrack.release();
          }
        } catch (Throwable t) {
          t.printStackTrace();
        }

      }
    }).start();
  }

  /**
   * 播放未做回声消除的音频文件
   *
   * @param v
   */
  public void playRecodeSound(View v) {
    new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          AudioTrack audioTrack = AudioUtils.createTracker(new int[1], hz.value(), hz.value() / 10, false);
          if (mRecodeFile.exists() && mRecodeFile.length() > 0) {
            BufferedInputStream in = new BufferedInputStream(new FileInputStream(mRecodeFile));
            byte[] data = new byte[hz.value() / 10];
            int len = 0;
            while ((len = in.read(data)) > 0) {
              audioTrack.play();
              audioTrack.write(data, 0, len);
            }
            audioTrack.release();
          }
        } catch (Throwable t) {
          t.printStackTrace();
        }

      }
    }).start();
  }

  /**
   * 打开/关闭声音
   *
   * @param v
   */
  public void switchSound(View v) {
    if ("打开声音".equals(((Button) v).getText())) {
      audioPlayer.soundOn();
      ((Button) v).setText("关闭声音");
    } else {
      audioPlayer.soundOff();
      ((Button) v).setText("打开声音");
    }
  }

  /**
   * 打开/关闭录音
   *
   * @param v
   */
  public void switchSpeak(View v) {
    if ("关闭录音".equals(((Button) v).getText())) {
      audioPlayer.pauseAudioRecord();
      ((Button) v).setText("打开录音");
    } else {
      audioPlayer.resumeAudioRecord();
      ((Button) v).setText("关闭录音");
    }
  }

  @Override
  protected void onDestroy() {
    if (audioPlayer != null) {
      audioPlayer.stop();
    }
    super.onDestroy();
  }

  public static BufferedOutputStream openStreamByFile(File file) {
    BufferedOutputStream out = null;
    try {
      if (file.exists()) {
        file.delete();
      } else {
        file.createNewFile();
      }
      out = new BufferedOutputStream(new FileOutputStream(file, true));
    } catch (Exception e) {
      e.printStackTrace();
      return null;
    }
    return out;
  }

  // 录制的原声音频数据回调接口 runOnBackThread
  @Override
  public int onRecodeAudioData(byte[] data, int length, byte[] imfo) {
    if (mRecodeStream != null) {
      try {
        mRecodeStream.write(data, 0, length);
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
    return 0;
  }

  @Override
  public void onUpdate(long pts) {
    Log.i("TAG", "pts:" + pts);
  }

}
