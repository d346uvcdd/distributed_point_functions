// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/internal/value_type_helpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "dpf/internal/status_matchers.h"

namespace distributed_point_functions {
namespace dpf_internal {
namespace {

template <typename T>
class ValueTypeIntegerTest : public testing::Test {};
using IntegerTypes =
    ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, absl::uint128>;
TYPED_TEST_SUITE(ValueTypeIntegerTest, IntegerTypes);

TYPED_TEST(ValueTypeIntegerTest, GetValueTypeProtoForIntegers) {
  ValueType value_type = GetValueTypeProtoFor<TypeParam>();

  EXPECT_TRUE(value_type.has_integer());
  EXPECT_EQ(value_type.integer().bitsize(), sizeof(TypeParam) * 8);
}

TYPED_TEST(ValueTypeIntegerTest, TestValueTypesAreEqual) {
  ValueType value_type_1 = GetValueTypeProtoFor<TypeParam>(), value_type_2;
  value_type_2.mutable_integer()->set_bitsize(sizeof(TypeParam) * 8);

  EXPECT_TRUE(ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_TRUE(ValueTypesAreEqual(value_type_2, value_type_1));
}

TYPED_TEST(ValueTypeIntegerTest, TestValueTypesAreNotEqual) {
  ValueType value_type_1 = GetValueTypeProtoFor<TypeParam>(), value_type_2;
  value_type_2.mutable_integer()->set_bitsize(sizeof(TypeParam) * 8 * 2);

  EXPECT_FALSE(ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(ValueTypesAreEqual(value_type_2, value_type_1));
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfNotInteger) {
  Value value;
  value.mutable_tuple();

  EXPECT_THAT(ConvertValueTo<TypeParam>(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "The given Value is not an integer"));
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfInvalidIntegerCase) {
  Value value;
  value.mutable_integer();

  EXPECT_THAT(ConvertValueTo<TypeParam>(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Unknown value case for the given integer Value"));
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfValueOutOfRange) {
  Value value;
  auto value_64 = uint64_t{1} << 32;
  value.mutable_integer()->set_value_uint64(value_64);

  if constexpr (sizeof(TypeParam) >= sizeof(uint64_t)) {
    DPF_EXPECT_OK(ConvertValueTo<TypeParam>(value));
  } else {
    EXPECT_THAT(ConvertValueTo<TypeParam>(value),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         absl::StrCat("Value (= ", value_64,
                                      ") too large for the given type T (size ",
                                      sizeof(TypeParam), ")")));
  }
}

template <typename T>
class ValueTypeTupleTest : public testing::Test {};
using TupleTypes = ::testing::Types<Tuple<uint64_t>, Tuple<uint64_t, uint64_t>,
                                    Tuple<uint32_t, absl::uint128, uint8_t>,
                                    Tuple<uint8_t, uint8_t, uint8_t, uint8_t>>;
TYPED_TEST_SUITE(ValueTypeTupleTest, TupleTypes);

TYPED_TEST(ValueTypeTupleTest, GetValueTypeProtoForTuples) {
  ValueType value_type = GetValueTypeProtoFor<TypeParam>();

  EXPECT_TRUE(value_type.has_tuple());
  EXPECT_EQ(value_type.tuple().elements_size(), std::tuple_size<TypeParam>());
  // Fold expression to iterate over tuple elements. See
  // https://stackoverflow.com/a/54053084.
  auto it = value_type.tuple().elements().begin();
  std::apply(
      [&it](auto&&... args) {
        ((
             // We need an extra lambda because we need multiple statements per
             // tuple element.
             [&it] {
               EXPECT_TRUE(it->has_integer());
               EXPECT_EQ(it->integer().bitsize(), (sizeof(args) * 8));
               ++it;
             }()),
         ...);
      },
      TypeParam());
}

TYPED_TEST(ValueTypeTupleTest, TestValueTypesSizeIsCorrect) {
  ValueType value_type = GetValueTypeProtoFor<TypeParam>();

  DPF_ASSERT_OK_AND_ASSIGN(int size,
                           ValidateValueTypeAndGetBitSize(value_type));

  EXPECT_EQ(size, 8 * GetTotalSize<TypeParam>());
}

TYPED_TEST(ValueTypeTupleTest, ValueConversionFailsIfValueIsNotATuple) {
  Value value;
  value.mutable_integer();

  EXPECT_THAT(ConvertValueTo<Tuple<uint32_t>>(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "The given Value is not a tuple"));
}

TEST(ValueTypeTupleTest, ValueConversionFailsIfValueSizeDoesntMatchTupleSize) {
  Value value;
  value.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(
      1234);

  using TupleType = Tuple<uint32_t, uint32_t>;
  EXPECT_THAT(
      ConvertValueTo<TupleType>(value),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "The tuple in the given Value has the wrong number of elements"));
}

TEST(ValueTypeTupleTest, TestValueTypesAreEqual) {
  using T1 = Tuple<uint32_t, absl::uint128, uint8_t>;
  using T2 = Tuple<uint32_t, absl::uint128, uint8_t>;

  ValueType value_type_1 = GetValueTypeProtoFor<T1>();
  ValueType value_type_2 = GetValueTypeProtoFor<T2>();

  EXPECT_TRUE(ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_TRUE(ValueTypesAreEqual(value_type_2, value_type_1));
}

TEST(ValueTypeTupleTest, TestValueTypesAreNotEqual) {
  using T1 = Tuple<uint32_t, absl::uint128, uint8_t>;
  using T2 = Tuple<uint32_t, absl::uint128, uint16_t>;

  ValueType value_type_1 = GetValueTypeProtoFor<T1>();
  ValueType value_type_2 = GetValueTypeProtoFor<T2>();

  EXPECT_FALSE(ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(ValueTypesAreEqual(value_type_2, value_type_1));
}

}  // namespace

}  // namespace dpf_internal
}  // namespace distributed_point_functions
