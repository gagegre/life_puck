// Imu.cpp -- QMI8658C 6-axis accelerometer driver.

#include "Imu.h"
#include "Clock.h"

#include <Wire.h>
#include <math.h>

bool IMU::begin() {
  // WHO_AM_I check.
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)QMI8658_ADDR, (uint8_t)1);
  if (!Wire.available() || Wire.read() != 0x05) return false;

  writeReg(0x02, 0x60);  // CTRL1
  writeReg(0x03, 0x23);  // CTRL2: ±8 g, 500 Hz
  writeReg(0x08, 0x01);  // CTRL7: accel on

  // Seed the previous-sample state. The Z component is read but
  // discarded -- only the horizontal axes drive shake detection.
  float scratchZ;
  readAccel(_prevAx, _prevAy, scratchZ);
  _ok = true;
  return true;
}

bool IMU::update() {
  if (!_ok) return false;
  if (!Clock::elapsed(_lastShakeAt, SHAKE_COOLDOWN_MS)) return false;

  float ax, ay, az;
  if (!readAccel(ax, ay, az)) return false;
  (void)az;  // vertical axis ignored: shake detection is horizontal-only.

  const float dx = ax - _prevAx;
  const float dy = ay - _prevAy;
  _prevAx = ax;
  _prevAy = ay;

  // Reset the reversal counter if too long has passed since the last cross.
  const uint32_t now = Clock::now();
  if (_reversalCount > 0 && (now - _lastReversalAt) > SHAKE_WINDOW_MS) {
    _reversalCount = 0;
    _lastDirection = 0;
  }

  // Pick the dominant horizontal axis as the shake axis.
  const float delta = (fabsf(dx) >= fabsf(dy)) ? dx : dy;
  if (fabsf(delta) < SHAKE_THRESHOLD) return false;

  const int8_t dir = (delta > 0) ? +1 : -1;
  if (dir != _lastDirection) {
    _lastDirection = dir;
    _lastReversalAt = now;
    _reversalCount++;
    if (_reversalCount >= SHAKE_REVERSALS_REQUIRED) {
      _lastShakeAt = now;
      _reversalCount = 0;
      _lastDirection = 0;
      return true;
    }
  }
  return false;
}

void IMU::writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool IMU::readAccel(float& x, float& y, float& z) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(0x35);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)QMI8658_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  const int16_t ax = (int16_t)(Wire.read() | (Wire.read() << 8));
  const int16_t ay = (int16_t)(Wire.read() | (Wire.read() << 8));
  const int16_t az = (int16_t)(Wire.read() | (Wire.read() << 8));
  constexpr float SCALE = 8.0f / 32768.0f;
  x = ax * SCALE;
  y = ay * SCALE;
  z = az * SCALE;
  return true;
}
