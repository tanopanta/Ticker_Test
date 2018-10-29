// センサー保存テスト（タイマー割り込み） スケッチ例のM5Stack-core-esp32用->Ticker->argumentsを参考。
// （100Hzで計測、10秒ごとに保存(計測終了時付近はデータがぶれそうなのでバッファはそのまま破棄)
// 基本的にハンドラ内でブロッキングはNGなので重たい処理はフラグ等を利用しloop()内で行う。


// Ticker.hは内部でesp_timer.hを使用している。マイクロ秒レベルの指定をしたい場合はそちらを使ったほうがいいかも。
// そのesp_timerは
// xTaskCreatePinnedToCore(&timer_task, "esp_timer",ESP_TASK_TIMER_STACK, NULL, ESP_TASK_TIMER_PRIO, &s_timer_task, PRO_CPU_NUM);
// を利用していたのでFreeRTOSのマルチタスク機能を利用している。使用しているコアがPRO_CPU_NUM(0)なのでメインのloopで使われるAPP_CPU_NUM (1)とは別のコアで動く（多分）


#include <M5Stack.h>
#include <Ticker.h>


Ticker tickerSensor;
Ticker tickerWriteData;

struct sensorData {
  int heart;
  int gsr;
};
sensorData sdBuffA[1024];
sensorData sdBuffB[1024];
volatile bool bufAUse = true;

//バッファのインデックス
volatile int pointerA = 0;
volatile int pointerB = 0;
//フラグが立っているときに保存を開始
volatile bool buffSaveFlg = false;

//ハンドラ－１（センサーを読んでバッファリング)
void _readSensor() {
  sensorData s;
  s.heart = analogRead(36);
  s.gsr = analogRead(35);
  
  if(bufAUse) {
    sdBuffA[pointerA] = s;
    pointerA++;
  } else {
    sdBuffB[pointerB] = s;
    pointerB++;
  }
}
//ハンドラ－２（フラグの反転）
void _buffMng() {
  //フラグを反転
  bufAUse = !bufAUse;

  buffSaveFlg = true;
}


//バッファをSDに保存
void writeSD() {
  File file = SD.open("/test.csv",FILE_APPEND);

  if(!file) {
      Serial.println("SD not found?Plz Insert SD and reboot");
      tickerSensor.detach();
      tickerWriteData.detach();
      return;
  }
  

  if(bufAUse) {
    //bufA使用中ー＞bufB保存
    for(int i = 0; i < pointerB; i++) {
      char buf[16];//バッファをはみ出ると落ちるので余裕をもって確保
      sprintf(buf, "%d, %d", sdBuffB[i].heart, sdBuffB[i].gsr);
      file.println(buf);
    }
    file.close();
    pointerB = 0;
  } else {
    for(int i = 0; i < pointerA; i++) {
      char buf[16];
      sprintf(buf, "%d, %d", sdBuffA[i].heart, sdBuffA[i].gsr);
      file.println(buf);
    }
    file.close();
    pointerA = 0;
  }
}

void setup() {
    M5.begin();
    dacWrite(25, 0); // Speaker OFF
    Serial.begin(115200);
    M5.lcd.println("push B start, push A stop");
}

void loop() {
    if(M5.BtnA.wasPressed()) {
        //タイマー終了
        tickerSensor.detach();
        tickerWriteData.detach();

        //バッファ初期化
        pointerA = 0;
        pointerB = 0;
        buffSaveFlg = false;
        bufAUse = true;
        
        M5.lcd.print("stop read");
    }
    if(M5.BtnB.wasPressed()) {
        //タイマースタート
        
        // 10ミリ秒ごとにセンサーリード(100Hz)
        tickerSensor.attach_ms(10, _readSensor);
        // 10秒ごとに保存
        tickerWriteData.attach_ms(10000, _buffMng); 

        M5.lcd.print("start read");
    }
    //フラグのチェック
    if(buffSaveFlg) {
      buffSaveFlg = false;
      writeSD();
    }
    delay(100);
    M5.update();
}
