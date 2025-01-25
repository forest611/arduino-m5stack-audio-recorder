#include <M5Unified.h>

static constexpr const size_t record_samplerate = 4096;                     // 録音サンプリングレート
static constexpr const size_t record_second = 12;                            // 録音時間(秒)
static constexpr const size_t max_samples = record_samplerate * record_second;  // 最大サンプル数
static int16_t *rec_data = nullptr;                                       // 録音データ
static size_t record_length = 0;                                          // 現在の録音長

// マイクのゲイン設定
static constexpr float TARGET_AMPLITUDE = 32767 * 0.8;  // ターゲットとする振幅（最大値の80%）

void setup(void) {
  auto cfg = M5.config();
  
  // HAT-SPK2の設定
  cfg.external_spk = true;
  cfg.external_speaker.hat_spk2 = true;
  cfg.output_power = true;
  
  M5.begin(cfg);

  // 画面上にログを表示する
  M5.setLogDisplayIndex(0);

  // 画面を横にする
  M5.Display.setRotation(1);

  // 録音用メモリ確保
  if (rec_data == nullptr) {
    rec_data = (int16_t*)heap_caps_malloc(max_samples * sizeof(int16_t), MALLOC_CAP_8BIT);
    if (rec_data != nullptr) {
      memset(rec_data, 0, max_samples * sizeof(int16_t));
      float memoryUsage = (float)(max_samples * sizeof(int16_t)) / ESP.getMaxAllocHeap() * 100.0;
      M5.Lcd.printf("Memory usage: %.2f%%\n", memoryUsage);      
      M5.Lcd.println("Memory allocated");
    } else {
      M5.Lcd.println("Memory allocation failed");
      return;
    }
  }

  // スピーカー設定
  M5.Speaker.setVolume(200);
  
  // スピーカーの状態を表示
  if (M5.Speaker.isEnabled()) {
    M5.Lcd.println("Speaker enabled");
  } else {
    M5.Lcd.println("Speaker not enabled");
  }
}

// 録音データを最大音量に合わせてノーマライズ
void normalizeAudioData() {
  // 最大振幅を検出
  int16_t max_amplitude = 0;
  for (int i = 0; i < record_length; i++) {
    int16_t abs_val = abs(rec_data[i]);
    if (abs_val > max_amplitude) {
      max_amplitude = abs_val;
    }
  }

  // ノーマライズ係数を計算（最大振幅の80%を目標に）
  float normalize_factor = max_amplitude > 0 ? TARGET_AMPLITUDE / max_amplitude : 1.0;
  
  M5.Lcd.printf("Max: %d, Factor: %.2f\n", max_amplitude, normalize_factor);

  // 音量を調整
  for (int i = 0; i < record_length; i++) {
    float normalized = rec_data[i] * normalize_factor;
    // クリッピング処理
    if (normalized > 32767) normalized = 32767;
    if (normalized < -32768) normalized = -32768;
    rec_data[i] = (int16_t)normalized;
  }
}

void loop(void) {
  M5.update();

  if (!M5.Mic.isEnabled()) 
    return;

  // Aボタンが押されている間は録音
  while (M5.BtnA.isPressed() && record_length + (record_samplerate/10) < max_samples) {
    if (!M5.Mic.isEnabled()) {
      M5.Lcd.println("recording...");
      M5.Speaker.end();
      M5.Mic.begin();
      delay(100);
    }

    // 0.1秒間録音
    if (M5.Mic.record(rec_data + record_length, record_samplerate/10, record_samplerate)){
      record_length += record_samplerate/10;
    }

    // 録音長が最大長に達したら終了
    if (record_length + (record_samplerate/10) >= max_samples){
      M5.Lcd.println("record over");
      break;
    }
  }

  // Bボタンが押されたら再生
  if (M5.BtnB.wasPressed()) {
    if (M5.Mic.isEnabled() || !M5.Speaker.isEnabled()) {
      M5.Mic.end();
      M5.Speaker.begin();
      delay(100);
    }

    if (record_length == 0) {
      M5.Lcd.println("no record");
      return;
    }

    if (!M5.Speaker.isEnabled()) {
      M5.Lcd.println("speaker not enabled");
      return;
    }

    // 録音データを正規化
    normalizeAudioData();
    
    M5.Lcd.println("play");
    
    M5.Speaker.playRaw(rec_data, record_length, record_samplerate);

    // 再生が終わるまで待ちたい場合はループで回す
    while (M5.Speaker.isPlaying()) {
      M5.update();
      delay(10);
    }

    // 録音データ解放 
    record_length = 0;

    // ログを削除
    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("ready to record");
  }
}