#pragma once

#include <stddef.h>
#include <stdint.h>

namespace zlab {

enum class GuardDecision : uint8_t {
  Allow,
  Duplicate,
  RateLimited,
  Quarantined,
  Invalid,
};

struct GuardConfig {
  uint32_t duplicateWindowMs;
  uint32_t tokenRefillMs;
  uint8_t burstTokens;
  uint8_t strikesToQuarantine;
  uint32_t quarantineMs;

  GuardConfig();
};

struct GuardStats {
  uint32_t received;
  uint32_t allowed;
  uint32_t duplicates;
  uint32_t rateLimited;
  uint32_t quarantined;
  uint32_t invalid;
  uint32_t evictions;

  GuardStats();
};

class MessageGuard {
 public:
  static const size_t kMaxDevices = 16;

  explicit MessageGuard(const GuardConfig& config = GuardConfig());

  GuardDecision evaluate(const char* deviceKey, uint32_t payloadFingerprint,
                         uint32_t nowMs);
  void reset();

  const GuardStats& stats() const;
  size_t trackedDevices() const;

  static uint32_t fingerprint(const char* text);
  static const char* decisionName(GuardDecision decision);

 private:
  struct DeviceState {
    bool used;
    bool hasAccepted;
    uint32_t keyHash;
    uint32_t lastFingerprint;
    uint32_t lastAcceptedMs;
    uint32_t lastSeenMs;
    uint32_t lastRefillMs;
    uint32_t quarantineUntilMs;
    uint8_t tokens;
    uint8_t strikes;
  };

  DeviceState& stateFor(uint32_t keyHash, uint32_t nowMs);
  void refill(DeviceState& state, uint32_t nowMs);
  bool isQuarantined(const DeviceState& state, uint32_t nowMs) const;

  GuardConfig config_;
  GuardStats stats_;
  DeviceState devices_[kMaxDevices];
};

}  // namespace zlab
