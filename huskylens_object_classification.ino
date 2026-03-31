#include <Wire.h>
#include "HUSKYLENS.h"

HUSKYLENS huskylens;

static const int SDA_PIN = 8;
static const int SCL_PIN = 9;

unsigned long t0 = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);

  huskylens.begin(Wire);

  t0 = millis();
  Serial.println("time_ms,det_count,class_id,x,y,w,h");
}

void loop() {
  unsigned long time_ms = millis() - t0;

  if (!huskylens.request()) {
    Serial.print(time_ms);
    Serial.println(",0,-1,-1,-1,-1,-1");
    delay(20);
    return;
  }

  int n = huskylens.count();

  if (n <= 0) {
    Serial.print(time_ms);
    Serial.println(",0,-1,-1,-1,-1,-1");
    delay(20);
    return;
  }

  for (int i = 0; i < n; i++) {
    HUSKYLENSResult r = huskylens.get(i);

    Serial.print(time_ms);        Serial.print(",");
    Serial.print(n);              Serial.print(",");
    Serial.print((int)r.ID);      Serial.print(",");
    Serial.print((int)r.xCenter); Serial.print(",");
    Serial.print((int)r.yCenter); Serial.print(",");
    Serial.print((int)r.width);   Serial.print(",");
    Serial.println((int)r.height);
  }

  delay(20);
}