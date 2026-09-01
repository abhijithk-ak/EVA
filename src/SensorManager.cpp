#include "SensorManager.h"
#include "Logger.h"

SensorManager::SensorManager()
    : sensorOk(false),
      lastPollTime(0),
      lastDistanceMm(0),
      currentEvent(SpatialEvent::CLEAR),
      obstacleStreak(0),
      edgeStreak(0) {}

bool SensorManager::begin() {
    sensorOk = tof.begin();
    if (!sensorOk) {
        EVA_LOGLN("[SensorManager] VL53L0X init FAILED - spatial awareness disabled");
    } else {
        EVA_LOGLN("[SensorManager] VL53L0X ready");
    }
    return sensorOk;
}

bool SensorManager::isHealthy() const {
    return sensorOk;
}

SpatialEvent SensorManager::classify(uint16_t mm, bool valid) {
    // An out-of-range reading while pointed down usually means the
    // beam missed the desk entirely -> treat as an edge candidate.
    bool obstacleSample = valid && (mm < TOF_OBSTACLE_MM);
    bool edgeSample      = (!valid) || (valid && mm > TOF_EDGE_MM);

    obstacleStreak = obstacleSample ? (obstacleStreak + 1) : 0;
    edgeStreak      = edgeSample     ? (edgeStreak + 1)     : 0;

    if (obstacleStreak >= TOF_OBSTACLE_CONFIRM_SAMPLES) {
        return SpatialEvent::OBSTACLE;
    }
    if (edgeStreak >= TOF_EDGE_CONFIRM_SAMPLES) {
        return SpatialEvent::EDGE;
    }
    if (!obstacleSample && !edgeSample) {
        return SpatialEvent::CLEAR;
    }
    // Streak not yet confirmed - keep previous stable event.
    return currentEvent;
}

void SensorManager::update() {
    if (!sensorOk) {
        currentEvent = SpatialEvent::CLEAR;
        return;
    }

    unsigned long now = millis();

    if (now - lastPollTime < TOF_POLL_INTERVAL_MS) return;
    lastPollTime = now;

    VL53L0X_RangingMeasurementData_t measure;
    tof.rangingTest(&measure, false);

    bool valid = (measure.RangeStatus != 4); // 4 == out of range / invalid
    if (valid) {
        lastDistanceMm = measure.RangeMilliMeter;
    }

    currentEvent = classify(lastDistanceMm, valid);
}

SpatialEvent SensorManager::getEvent() const {
    return currentEvent;
}

uint16_t SensorManager::getLastDistanceMm() const {
    return lastDistanceMm;
}
