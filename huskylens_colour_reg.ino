#include "HUSKYLENS.h"
#include <Wire.h>

HUSKYLENS huskylens;

#define RUN_NUMBER  1
// Colour block detection mode
// HuskyLens must be set to "Color Recognition" mode manually on the device

static uint32_t t0 = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Connect to HuskyLens over I2C
  while (!huskylens.begin(Wire)) {
    Serial.println("HuskyLens not found, retrying...");
    delay(500);
  }

  // Set to colour recognition mode
  huskylens.writeAlgorithm(ALGORITHM_COLOR_RECOGNITION);
  delay(2000); // let it settle

  t0 = millis();
  Serial.println("time_ms,det_count,colour_id,x,y,w,h");
}

void loop() {
  uint32_t time_ms = millis() - t0;

  if (!huskylens.request()) {
    // No response from HuskyLens
    Serial.print(time_ms);
    Serial.println(",0,-1,-1,-1,-1,-1");
    delay(20);
    return;
  }

  if (!huskylens.isLearned()) {
    // No learned colours yet
    Serial.print(time_ms);
    Serial.println(",0,-1,-1,-1,-1,-1");
    delay(20);
    return;
  }

  if (!huskylens.available()) {
    // Nothing detected this frame
    Serial.print(time_ms);
    Serial.println(",0,-1,-1,-1,-1,-1");
    delay(20);
    return;
  }

  // Read all detected blocks
  int det_count = huskylens.countBlocks();

  while (huskylens.available()) {
    HUSKYLENSResult result = huskylens.read();

    if (result.command == COMMAND_RETURN_BLOCK) {
      Serial.print(time_ms);   Serial.print(",");
      Serial.print(det_count); Serial.print(",");
      Serial.print(result.ID); Serial.print(",");  // colour_id = learned colour number
      Serial.print(result.xCenter); Serial.print(",");
      Serial.print(result.yCenter); Serial.print(",");
      Serial.print(result.width);   Serial.print(",");
      Serial.println(result.height);
    }
  }

  delay(20);
}