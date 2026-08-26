#include "MessageGuard.h"

#include <limits.h>
#include <string.h>

namespace zlab {

GuardConfig::GuardConfig()
    : duplicateWindowMs(3000),
      tokenRefillMs(1000),
      burstTokens(5),
      strikesToQuarantine(3),
      quarantineMs(30000) {}

GuardStats::GuardStats()
    : received(0),
      allowed(0),
      duplicates(0),
      rateLimited(0),
      quarantined(0),
      invalid(0),
      evictions(0) {}

MessageGuard::MessageGuard(const GuardConfig& config) : config_(config) {
  if (config_.tokenRefillMs == 0) config_.tokenRefillMs = 1;
  if (config_.burstTokens == 0) config_.burstTokens = 1;
  if (config_.strikesToQuarantine == 0) config_.strikesToQuarantine = 1;
  if (config_.quarantineMs == 0) config_.quarantineMs = 1;
  reset();
}

void MessageGuard::reset() {
  memset(devices_, 0, sizeof(devices_));
  stats_ = GuardStats();
}

const GuardStats& MessageGuard::stats() const { return stats_; }

size_t MessageGuard::trackedDevices() const {
  size_t count = 0;
  for (size_t i = 0; i < kMaxDevices; ++i) {
    if (devices_[i].used) ++count;
  }
  return count;
}

uint32_t MessageGuard::fingerprint(const char* text) {
  if (text == NULL) return 0;
  uint32_t hash = 2166136261u;
  while (*text != '\0') {
    hash ^= static_cast<uint8_t>(*text++);
    hash *= 16777619u;
  }
  return hash;
}

const char* MessageGuard::decisionName(GuardDecision decision) {
  switch (decision) {
    case GuardDecision::Allow:
      return "allow";
    case GuardDecision::Duplicate:
      return "duplicate";
    case GuardDecision::RateLimited:
      return "rate_limited";
    case GuardDecision::Quarantined:
      return "quarantined";
    case GuardDecision::Invalid:
      return "invalid";
  }
  return "unknown";
}

MessageGuard::DeviceState& MessageGuard::stateFor(uint32_t keyHash,
                                                   uint32_t nowMs) {
  size_t freeIndex = kMaxDevices;
  size_t oldestIndex = 0;
  uint32_t oldestAge = 0;

  for (size_t i = 0; i < kMaxDevices; ++i) {
    DeviceState& state = devices_[i];
    if (state.used && state.keyHash == keyHash) return state;
    if (!state.used && freeIndex == kMaxDevices) freeIndex = i;
    if (state.used) {
      const uint32_t age = nowMs - state.lastSeenMs;
      if (age >= oldestAge) {
        oldestAge = age;
        oldestIndex = i;
      }
    }
  }

  const size_t index = freeIndex != kMaxDevices ? freeIndex : oldestIndex;
  if (freeIndex == kMaxDevices) ++stats_.evictions;
  DeviceState& state = devices_[index];
  memset(&state, 0, sizeof(state));
  state.used = true;
  state.keyHash = keyHash;
  state.tokens = config_.burstTokens;
  state.lastSeenMs = nowMs;
  state.lastRefillMs = nowMs;
  return state;
}

bool MessageGuard::isQuarantined(const DeviceState& state,
                                 uint32_t nowMs) const {
  return state.quarantineUntilMs != 0 &&
         static_cast<int32_t>(state.quarantineUntilMs - nowMs) > 0;
}

void MessageGuard::refill(DeviceState& state, uint32_t nowMs) {
  const uint32_t elapsed = nowMs - state.lastRefillMs;
  const uint32_t gained = elapsed / config_.tokenRefillMs;
  if (gained == 0) return;

  const uint32_t replenished = static_cast<uint32_t>(state.tokens) + gained;
  state.tokens = static_cast<uint8_t>(
      replenished > config_.burstTokens ? config_.burstTokens : replenished);
  state.lastRefillMs += gained * config_.tokenRefillMs;
}

GuardDecision MessageGuard::evaluate(const char* deviceKey,
                                     uint32_t payloadFingerprint,
                                     uint32_t nowMs) {
  ++stats_.received;
  if (deviceKey == NULL || deviceKey[0] == '\0' || payloadFingerprint == 0) {
    ++stats_.invalid;
    return GuardDecision::Invalid;
  }

  DeviceState& state = stateFor(fingerprint(deviceKey), nowMs);
  state.lastSeenMs = nowMs;

  if (isQuarantined(state, nowMs)) {
    ++stats_.quarantined;
    return GuardDecision::Quarantined;
  }
  if (state.quarantineUntilMs != 0) {
    state.quarantineUntilMs = 0;
    state.strikes = 0;
  }

  refill(state, nowMs);
  if (state.hasAccepted && state.lastFingerprint == payloadFingerprint &&
      nowMs - state.lastAcceptedMs <= config_.duplicateWindowMs) {
    ++stats_.duplicates;
    return GuardDecision::Duplicate;
  }

  if (state.tokens == 0) {
    ++stats_.rateLimited;
    if (state.strikes < UCHAR_MAX) ++state.strikes;
    if (state.strikes >= config_.strikesToQuarantine) {
      state.quarantineUntilMs = nowMs + config_.quarantineMs;
      ++stats_.quarantined;
      return GuardDecision::Quarantined;
    }
    return GuardDecision::RateLimited;
  }

  --state.tokens;
  state.strikes = 0;
  state.hasAccepted = true;
  state.lastFingerprint = payloadFingerprint;
  state.lastAcceptedMs = nowMs;
  ++stats_.allowed;
  return GuardDecision::Allow;
}

}  // namespace zlab
