#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// Cấu hình WiFi
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Cấu hình Firebase
#define DATABASE_URL "YOUR_FIREBASE_DATABASE_URL"
#define DATABASE_SECRET "YOUR_FIREBASE_DATABASE_SECRET"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Cấu hình các chân cảm biến
#define SOIL_SENSOR_PIN A0
#define SOIL_POWER_PIN D2
#define RAIN_SENSOR_PIN D5
#define TILT_SENSOR_PIN D6

// Chân điều khiển thiết bị
#define LED_PIN D7
#define BUZZER_PIN D1

volatile bool daRung = false;
bool canhBaoSatLo = false;
unsigned long thoiGianCanhBao = 0;

// Hàm ngắt khi có rung
void IRAM_ATTR xuLyRung() {
  daRung = true;
}

// Hàm đọc độ ẩm đất
float docDoAmDat() {
  digitalWrite(SOIL_POWER_PIN, HIGH);
  delay(500);
  int tong = 0;
  for (int i = 0; i < 10; i++) {
    tong += analogRead(SOIL_SENSOR_PIN);
    delay(50);
  }
  digitalWrite(SOIL_POWER_PIN, LOW);
  int trungbinh = tong / 10;
  float percent = map(trungbinh, 1023, 0, 0, 100);
  return percent;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Đang kết nối WiFi...");
    delay(1000);
  }
  Serial.println("Đã kết nối WiFi");

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(SOIL_POWER_PIN, OUTPUT);
  digitalWrite(SOIL_POWER_PIN, LOW);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(TILT_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(TILT_SENSOR_PIN), xuLyRung, RISING);
}

void loop() {
  // Nếu đang trong thời gian cảnh báo sạt lở thì dừng đo và kiểm tra thời gian
  if (canhBaoSatLo) {
    // Nếu đã qua 5 giây, tắt còi và tiếp tục hoạt động bình thường
    if (millis() - thoiGianCanhBao >= 3000) {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      Firebase.setInt(fbdo, "/canh_bao", 0);  
      Firebase.setInt(fbdo, "/thiet_bi/loa", 0);
      Firebase.setInt(fbdo, "/thiet_bi/den", 0);
      canhBaoSatLo = false;
      daRung = false;  // Reset rung
    } else {
      return; // Đang cảnh báo -> bỏ qua đo lường, Firebase
    }
  }

  // Đo dữ liệu
  float doam_percent = docDoAmDat();
  int mua = digitalRead(RAIN_SENSOR_PIN);  // LOW: Có mưa
  int rung = daRung ? HIGH : LOW;

  Serial.print("Do am dat: "); Serial.print(doam_percent); Serial.println("%");
  Serial.print("Mua: "); Serial.println(mua == LOW ? "Co mua" : "Khong mua");
  Serial.print("Rung: "); Serial.println(rung == HIGH ? "Co rung" : "Binh thuong");
  Serial.println();

  // Gửi dữ liệu lên Firebase
  Firebase.setFloat(fbdo, "/sensor_data/do_am_dat", doam_percent);
  Firebase.setInt(fbdo, "/sensor_data/mua", (mua == LOW ? 1 : 0));
  Firebase.setInt(fbdo, "/sensor_data/rung", rung);

  // Phát hiện sạt lở: Độ ẩm cao + có mưa + có rung
  if (doam_percent > 70 && mua == LOW && rung == HIGH) {
    Serial.println("CANH BAO: NGUY CO SAT LO!!!");

    // Bật còi và đèn
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    // Cập nhật Firebase
    Firebase.setInt(fbdo, "/canh_bao", 1);
    Firebase.setInt(fbdo, "/thiet_bi/loa", 1);
    Firebase.setInt(fbdo, "/thiet_bi/den", 1);

    // Bắt đầu thời gian cảnh báo
    canhBaoSatLo = true;
    thoiGianCanhBao = millis();
  }

  if (fbdo.httpCode() == 200) {
    Serial.println("Cập nhật Firebase thành công");
  } else {
    Serial.print("Firebase lỗi: ");
    Serial.println(fbdo.errorReason());
  }

  daRung = false;
  delay(1000);
}