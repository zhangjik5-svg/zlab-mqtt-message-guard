#include "MessageGuard.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

zlab::GuardConfig strictConfig() {
  zlab::GuardConfig config;
  config.duplicateWindowMs = 100;
  config.tokenRefillMs = 1000;
  config.burstTokens = 2;
  config.strikesToQuarantine = 2;
  config.quarantineMs = 5000;
  return config;
}

void testDuplicateSuppression() {
  zlab::MessageGuard guard(strictConfig());
  const uint32_t payload = zlab::MessageGuard::fingerprint("{temp:21}");
  expect(guard.evaluate("sensor-a", payload, 10) ==
             zlab::GuardDecision::Allow,
         "first message is allowed");
  expect(guard.evaluate("sensor-a", payload, 50) ==
             zlab::GuardDecision::Duplicate,
         "same payload inside window is suppressed");
  expect(guard.evaluate("sensor-a", payload, 200) ==
             zlab::GuardDecision::Allow,
         "same payload after window is allowed");
  expect(guard.stats().duplicates == 1, "duplicate counter increments");
}

void testRateLimitAndQuarantine() {
  zlab::MessageGuard guard(strictConfig());
  expect(guard.evaluate("noisy", 1, 0) == zlab::GuardDecision::Allow,
         "first burst token is available");
  expect(guard.evaluate("noisy", 2, 1) == zlab::GuardDecision::Allow,
         "second burst token is available");
  expect(guard.evaluate("noisy", 3, 2) ==
             zlab::GuardDecision::RateLimited,
         "first excess message is rate limited");
  expect(guard.evaluate("noisy", 4, 3) ==
             zlab::GuardDecision::Quarantined,
         "repeated excess traffic triggers quarantine");
  expect(guard.evaluate("noisy", 5, 1000) ==
             zlab::GuardDecision::Quarantined,
         "traffic remains blocked during quarantine");
  expect(guard.evaluate("noisy", 6, 5004) == zlab::GuardDecision::Allow,
         "device recovers after quarantine deadline");
}

void testTokenRefill() {
  zlab::MessageGuard guard(strictConfig());
  guard.evaluate("paced", 1, 100);
  guard.evaluate("paced", 2, 101);
  expect(guard.evaluate("paced", 3, 1100) == zlab::GuardDecision::Allow,
         "one token refills after the configured interval");
}

void testBoundedDeviceTable() {
  zlab::MessageGuard guard;
  char key[24];
  for (size_t i = 0; i < zlab::MessageGuard::kMaxDevices + 3; ++i) {
    std::snprintf(key, sizeof(key), "device-%u", static_cast<unsigned>(i));
    expect(guard.evaluate(key, static_cast<uint32_t>(i + 1),
                          static_cast<uint32_t>(i)) ==
               zlab::GuardDecision::Allow,
           "new device can use the guard");
  }
  expect(guard.trackedDevices() == zlab::MessageGuard::kMaxDevices,
         "device table remains bounded");
  expect(guard.stats().evictions == 3, "oldest entries are evicted");
}

void testMillisWraparound() {
  zlab::MessageGuard guard(strictConfig());
  const uint32_t nearWrap = 0xfffffff0u;
  guard.evaluate("wrap", 1, nearWrap);
  guard.evaluate("wrap", 2, nearWrap + 1u);
  expect(guard.evaluate("wrap", 3, 1000u) == zlab::GuardDecision::Allow,
         "unsigned elapsed time survives millis wraparound");
}

void testInvalidInput() {
  zlab::MessageGuard guard;
  expect(guard.evaluate(NULL, 1, 0) == zlab::GuardDecision::Invalid,
         "null device key is rejected");
  expect(guard.evaluate("sensor", 0, 0) == zlab::GuardDecision::Invalid,
         "zero fingerprint is rejected");
  expect(guard.stats().invalid == 2, "invalid counter increments");
}

}  // namespace

int main() {
  testDuplicateSuppression();
  testRateLimitAndQuarantine();
  testTokenRefill();
  testBoundedDeviceTable();
  testMillisWraparound();
  testInvalidInput();
  if (failures != 0) return EXIT_FAILURE;
  std::cout << "All MessageGuard tests passed\n";
  return EXIT_SUCCESS;
}
