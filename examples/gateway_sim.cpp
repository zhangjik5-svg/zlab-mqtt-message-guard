#include "MessageGuard.h"

#include <iostream>

int main() {
  zlab::MessageGuard guard;
  const char* device = "ble-a4:c1:38:00:01";
  const char* payloads[] = {"temp=24.1", "temp=24.1", "temp=24.2"};
  const uint32_t times[] = {1000, 1200, 1400};

  for (size_t i = 0; i < 3; ++i) {
    const zlab::GuardDecision decision = guard.evaluate(
        device, zlab::MessageGuard::fingerprint(payloads[i]), times[i]);
    std::cout << times[i] << " ms | " << payloads[i] << " | "
              << zlab::MessageGuard::decisionName(decision) << '\n';
  }
}
