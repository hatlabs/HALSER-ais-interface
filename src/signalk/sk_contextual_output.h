#ifndef SK_CONTEXTUAL_OUTPUT_H_
#define SK_CONTEXTUAL_OUTPUT_H_

#include <utility>

#include "sensesp/signalk/signalk_emitter.h"
#include "sensesp/system/valueconsumer.h"

namespace sensesp {

/**
 * @brief An SKEmitter that receives a (context, value) pair and emits
 * a Signal K delta with a dynamic context.
 *
 * This is a sibling of SKOutput<T>. Where SKOutput always emits to the
 * implicit "self" context, SKContextualOutput includes an explicit context
 * in its JSON output, allowing data to be attributed to other vessels
 * or entities (e.g. AIS targets).
 *
 * The input type is std::pair<String, T> where:
 *   - first: the Signal K context (e.g. "vessels.urn:mrn:imo:mmsi:477553000")
 *   - second: the value
 *
 * The path is static (set at construction), following the same pattern as
 * SKOutput.
 */
template <typename T>
class SKContextualOutput : public SKEmitter,
                           public ValueConsumer<std::pair<String, T>> {
 public:
  SKContextualOutput(String sk_path) : SKEmitter(sk_path) {}

  void set(const std::pair<String, T>& input) override {
    context_ = input.first;
    value_ = input.second;
    this->notify();
  }

  void as_signalk_json(JsonDocument& doc) override {
    doc["context"] = context_;
    doc["path"] = this->get_sk_path();
    doc["value"] = value_;
  }

 private:
  String context_;
  T value_;
};

// Specialization for JsonDocument values (complex objects like position).
// Uses serialized() to embed pre-built JSON.
class SKContextualOutputRawJson : public SKEmitter,
                                  public ValueConsumer<std::pair<String, String>> {
 public:
  SKContextualOutputRawJson(String sk_path) : SKEmitter(sk_path) {}

  void set(const std::pair<String, String>& input) override {
    context_ = input.first;
    value_ = input.second;
    this->notify();
  }

  void as_signalk_json(JsonDocument& doc) override {
    doc["context"] = context_;
    doc["path"] = this->get_sk_path();
    doc["value"] = serialized(value_);
  }

 private:
  String context_;
  String value_;
};

}  // namespace sensesp

#endif  // SK_CONTEXTUAL_OUTPUT_H_
