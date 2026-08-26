from __future__ import annotations

import shutil
import sys
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"Expected exactly one {label} insertion point, found {text.count(old)}")
    return text.replace(old, new, 1)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: integrate.py <OpenMQTTGateway checkout>")

    upstream = Path(sys.argv[1]).resolve()
    project = Path(__file__).resolve().parents[2]
    main_cpp = upstream / "main" / "main.cpp"
    text = main_cpp.read_text(encoding="utf-8")

    text = replace_once(
        text,
        '#include "LEDManager.h"\n#include "TheengsCommon.h"',
        '#include "LEDManager.h"\n#include "MessageGuard.h"\n#include "TheengsCommon.h"',
        "MessageGuard include",
    )
    text = replace_once(
        text,
        "std::queue<std::string> jsonQueue;\n",
        "std::queue<std::string> jsonQueue;\nzlab::MessageGuard zlabMessageGuard;\n",
        "guard instance",
    )

    helpers = r'''static const char* zlabMessageIdentity(
    const StaticJsonDocument<JSON_MSG_BUFFER>& jsonDoc) {
  static const char* keys[] = {"id", "mac", "address", "model", "origin"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    if (jsonDoc[keys[i]].is<const char*>()) {
      const char* value = jsonDoc[keys[i]].as<const char*>();
      if (value != nullptr && value[0] != '\0') return value;
    }
  }
  return "unknown";
}

static uint32_t zlabStableFingerprint(
    const StaticJsonDocument<JSON_MSG_BUFFER>& jsonDoc) {
  std::string stablePayload;
  for (JsonPairConst pair : jsonDoc.as<JsonObjectConst>()) {
    const char* key = pair.key().c_str();
    if (strcmp(key, "rssi") == 0 || strcmp(key, "time") == 0 ||
        strcmp(key, "UTCtime") == 0 || strcmp(key, "unixtime") == 0) {
      continue;
    }
    stablePayload += key;
    stablePayload += '=';
    serializeJson(pair.value(), stablePayload);
    stablePayload += ';';
  }
  return zlab::MessageGuard::fingerprint(stablePayload.c_str());
}

'''
    text = replace_once(
        text,
        "// Add a document to the queue\nbool enqueueJsonObject",
        helpers + "// Add a document to the queue\nbool enqueueJsonObject",
        "queue helper",
    )

    guard_check = r'''  std::string guardDeviceKey = jsonDoc["origin"] | "unknown";
  guardDeviceKey += ':';
  guardDeviceKey += zlabMessageIdentity(jsonDoc);
  const zlab::GuardDecision guardDecision = zlabMessageGuard.evaluate(
      guardDeviceKey.c_str(), zlabStableFingerprint(jsonDoc), millis());
  if (guardDecision != zlab::GuardDecision::Allow) {
    THEENGS_LOG_TRACE(F("ZLab guard filtered %s: %s" CR),
                      guardDeviceKey.c_str(),
                      zlab::MessageGuard::decisionName(guardDecision));
    return true;
  }
'''
    text = replace_once(
        text,
        "  serializeJson(jsonDoc, jsonString);\n#ifdef ESP32",
        "  serializeJson(jsonDoc, jsonString);\n" + guard_check + "#ifdef ESP32",
        "queue guard",
    )

    metrics = r'''  const zlab::GuardStats& guardStats = zlabMessageGuard.stats();
  SYSdata["guardok"] = guardStats.allowed;
  SYSdata["guarddup"] = guardStats.duplicates;
  SYSdata["guardlim"] = guardStats.rateLimited;
  SYSdata["guardiso"] = guardStats.quarantined;
  SYSdata["guarddev"] = zlabMessageGuard.trackedDevices();
'''
    text = replace_once(
        text,
        '  SYSdata["maxq"] = maxQueueLength;\n  SYSdata["cnt_index"] = cnt_index;',
        '  SYSdata["maxq"] = maxQueueLength;\n' + metrics + '  SYSdata["cnt_index"] = cnt_index;',
        "guard metrics",
    )

    main_cpp.write_text(text, encoding="utf-8", newline="\n")
    library = upstream / "lib" / "ZLabMessageGuard"
    library.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(project / "include" / "MessageGuard.h", library / "MessageGuard.h")
    shutil.copyfile(project / "src" / "MessageGuard.cpp", library / "MessageGuard.cpp")
    shutil.copyfile(project / "README.md", library / "README.md")


if __name__ == "__main__":
    main()
