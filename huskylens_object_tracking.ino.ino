#include "HUSKYLENS.h"
#include <Wire.h>

HUSKYLENS huskylens;

#define SDA_PIN 8
#define SCL_PIN 9

// 1 = "Time:..., ID:..., X:..." style; 0 = CSV: time_ms,id,x,y,w,h
#define OUTPUT_LABELED 1

static void pickPrimary(const HUSKYLENSResult &r,
                        int &id, int &x, int &y, int &w, int &h,
                        uint32_t &bestArea) {
  uint32_t area = (uint32_t)r.width * (uint32_t)r.height;
  if (area >= bestArea) {
    bestArea = area;
    id = (int)r.ID;
    x  = (int)r.xCenter;
    y  = (int)r.yCenter;
    w  = (int)r.width;
    h  = (int)r.height;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  huskylens.begin(Wire);

#if OUTPUT_LABELED == 0
  Serial.println("time_ms,id,x,y,w,h");
#endif
}

void loop() {
  int id = -1, x = -1, y = -1, w = -1, h = -1;

  if (huskylens.request() && huskylens.available()) {
    uint32_t bestArea = 0;
    while (huskylens.available()) {
      HUSKYLENSResult r = huskylens.read();
      pickPrimary(r, id, x, y, w, h, bestArea);
    }
  }

#if OUTPUT_LABELED == 1
    Serial.print("Time:");
    Serial.print(millis());
    Serial.print(", ID:");
    Serial.print(id);
    Serial.print(", X:");
    Serial.print(x);
    Serial.print(", Y:");
    Serial.print(y);
    Serial.print(", W:");
    Serial.print(w);
    Serial.print(", H:");
    Serial.println(h);
#else
    Serial.print(millis());
    Serial.print(",");
    Serial.print(id);
    Serial.print(",");
    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.print(",");
    Serial.print(w);
    Serial.print(",");
    Serial.println(h);
#endif

  delay(50);
}