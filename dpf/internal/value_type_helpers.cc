
#include "dpf/internal/value_type_helpers.h"

namespace distributed_point_functions {
namespace dpf_internal {

absl::StatusOr<int> ValidateValueTypeAndGetBitSize(
    const ValueType& value_type) {
  if (value_type.type_case() == ValueType::kInteger) {
    int bitsize = value_type.integer().bitsize();
    if (bitsize < 1) {
      return absl::InvalidArgumentError("`bitsize` must be positive");
    }
    if (bitsize > 128) {
      return absl::InvalidArgumentError(
          "`bitsize` must be less than or equal to 128");
    }
    if ((bitsize & (bitsize - 1)) != 0) {
      return absl::InvalidArgumentError("`bitsize` must be a power of 2");
    }
    return bitsize;
  } else if (value_type.type_case() == ValueType::kTuple) {
    int bitsize = 0;
    for (const ValueType& el : value_type.tuple().elements()) {
      absl::StatusOr<int> el_bitsize = ValidateValueTypeAndGetBitSize(el);
      if (!el_bitsize.ok()) {
        return el_bitsize.status();
      }
      bitsize += *el_bitsize;
    }
    return bitsize;
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported value_type:\n", value_type.DebugString()));
  }
}

bool ValueTypesAreEqual(const ValueType& lhs, const ValueType& rhs) {
  if (lhs.type_case() == ValueType::kInteger &&
      rhs.type_case() == ValueType::kInteger) {
    return lhs.integer().bitsize() == rhs.integer().bitsize();
  } else if (lhs.type_case() == ValueType::kTuple &&
             rhs.type_case() == ValueType::kTuple &&
             lhs.tuple().elements_size() == rhs.tuple().elements_size()) {
    bool result = true;
    for (int i = 0; i < static_cast<int>(lhs.tuple().elements_size()); ++i) {
      result &=
          ValueTypesAreEqual(lhs.tuple().elements(i), rhs.tuple().elements(i));
    }
    return result;
  }
  return false;
}

absl::StatusOr<absl::uint128> ConvertValueToImpl(const Value& value,
                                                 type_helper<absl::uint128>) {
  if (value.value_case() != Value::kInteger) {
    return absl::InvalidArgumentError("The given Value is not an integer");
  }
  if (value.integer().value_case() != Value::Integer::kValueUint128) {
    return absl::InvalidArgumentError(
        "The given Value does not have value_uint128 set");
  }
  const Block& block = value.integer().value_uint128();
  return absl::MakeUint128(block.high(), block.low());
}

Value ToValue(absl::uint128 input) {
  Value result;
  Block& block = *(result.mutable_integer()->mutable_value_uint128());
  block.set_high(absl::Uint128High64(input));
  block.set_low(absl::Uint128Low64(input));
  return result;
}

}  // namespace dpf_internal

}  // namespace distributed_point_functions