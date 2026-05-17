// Imu.h
//
// QMI8658C 6-axis accelerometer driver, just the bits we actually use:
// initialisation and a shake detector.
//
// Shake detection counts direction reversals on the dominant horizontal
// axis within a short window, so a single pickup or jolt never triggers
// a reset, SHAKE_REVERSALS_REQUIRED (default 4) crosses are needed
// inside SHAKE_WINDOW_MS, with SHAKE_COOLDOWN_MS between successive
// resets.

#pragma once

#include "Config.h"

class IMU {
public:
  // Probes the I2C bus for a QMI8658C, configures the accelerometer
  // (±8 g, 500 Hz), and seeds the previous-sample state. Returns false
  // if the chip isn't found or doesn't ack, the rest of the firmware
  // continues running without shake detection in that case.
  bool begin();

  // Polls the accelerometer (call no faster than IMU_POLL_MS). Returns
  // true exactly once per qualifying shake event. Returns false when no
  // sample was available, when we're still in the cool-down window, or
  // when the current motion hasn't accumulated enough reversals yet.
  bool update();

  bool isOk() const {
    return _ok;
  }

private:
  bool _ok = false;
  float _prevAx = 0.0f;
  float _prevAy = 0.0f;
  uint32_t _lastShakeAt = 0;
  int8_t _lastDirection = 0;
  uint8_t _reversalCount = 0;
  uint32_t _lastReversalAt = 0;

  void writeReg(uint8_t reg, uint8_t val);
  bool readAccel(float& x, float& y, float& z);
};
