#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <ranges>
#include <limits>
#include <random>

extern "C" {
#include "circular_buffer.h"
}

constexpr size_t BUFFER_DATA_MAX_ELEMS = 0xFF;

struct Example {
    int         x, y;
    const char* str;

    auto operator<=>(const Example&) const = default;
};

template <typename T>
class CircularBufferTest : public ::testing::Test {
   protected:
    void SetUp() override {
        this->circular_buffer = reinterpret_cast<circular_buffer_t*>(std::malloc(circular_buffer_type_size));
    }

    void TearDown() override {
        free(this->circular_buffer);
    }

    circular_buffer_t*             circular_buffer;
    T                              circular_buffer_data[BUFFER_DATA_MAX_ELEMS];
    std::map<uint8_t, std::string> test_strings;

   public:
    constexpr static size_t  MAX_BUFFER_SIZE  = sizeof(circular_buffer_data);
    constexpr static uint8_t DEFAULT_ELEMENTS = 8;

    circular_buffer_t* default_cb(uint8_t elems = DEFAULT_ELEMENTS) {
        EXPECT_TRUE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, &this->circular_buffer_data,
                                        sizeof(T) * elems, elems, sizeof(T)));

        return circular_buffer;
    }

    T get_elem(uint64_t i) {
        if constexpr (std::is_same_v<T, uint8_t>) {
            return (uint8_t)i & 0xFF;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::bit_cast<int>((uint32_t)i & 0xFFFFFFFF);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<double>(i);
        } else if constexpr (std::is_same_v<T, Example>) {
            const char* str = "";
            if (test_strings.contains(i)) {
                str = test_strings[i].c_str();
            } else {
                str = test_strings.emplace(i, std::to_string(i)).first->second.c_str();
            }
            return (Example){.x   = std::bit_cast<int>((uint32_t)i & 0xFFFFFFFF),
                             .y   = std::bit_cast<int>((uint32_t)i & 0xFFFFFFFF),
                             .str = str};
        } else {
            ADD_FAILURE() << "Invalid type for get_elem: " << __FUNCTION__;
        }
    }

    bool cb_typed_push(circular_buffer_t* cb, const T& value) {
        return circular_buffer_push(cb, &value);
    }

    T* cb_typed_pop(circular_buffer_t* cb, T& value) {
        return reinterpret_cast<T*>(circular_buffer_pop(cb, &value));
    }

    bool cb_typed_unshift(circular_buffer_t* cb, const T& value) {
        return circular_buffer_unshift(cb, &value);
    }

    T* cb_typed_shift(circular_buffer_t* cb, T& value) {
        return reinterpret_cast<T*>(circular_buffer_shift(cb, &value));
    }

    T* cb_typed_at(circular_buffer_t* cb, uint8_t index) {
        return reinterpret_cast<T*>(circular_buffer_at(cb, index));
    }
};

using TestTypes    = ::testing::Types<uint8_t, int, double, Example>;
using TestTypesDev = ::testing::Types<int>;
TYPED_TEST_SUITE(CircularBufferTest, TestTypes);

#define CB_TEST(x) TYPED_TEST(CircularBufferTest, x)

CB_TEST(new_ok) {
    ASSERT_TRUE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, &this->circular_buffer_data,
                                    TestFixture::MAX_BUFFER_SIZE, BUFFER_DATA_MAX_ELEMS, sizeof(TypeParam)))
        << "Happy path creation of circular buffer failed instead of passing";
}

CB_TEST(new_nok_cb_location_null) {
    ASSERT_FALSE(circular_buffer_new(nullptr, circular_buffer_type_size, &this->circular_buffer_data,
                                     TestFixture::MAX_BUFFER_SIZE, BUFFER_DATA_MAX_ELEMS, sizeof(TypeParam)))
        << "Creating circular buffer with NULL location passed instead of failing";
}

CB_TEST(new_nok_cb_location_size_nok) {
    auto test_size = [this](size_t size) {
        EXPECT_FALSE(circular_buffer_new(this->circular_buffer, size, &this->circular_buffer_data,
                                         TestFixture::MAX_BUFFER_SIZE, BUFFER_DATA_MAX_ELEMS, sizeof(TypeParam)))
            << "Creating circular buffer of size " << size << " passed instead of failing";
    };

    test_size(0);
    test_size(circular_buffer_type_size - 1);
    test_size(circular_buffer_type_size + 1);
    test_size(std::numeric_limits<size_t>::max());
}

CB_TEST(new_nok_buffer_null) {
    ASSERT_FALSE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, nullptr,
                                     TestFixture::MAX_BUFFER_SIZE, BUFFER_DATA_MAX_ELEMS, sizeof(TypeParam)))
        << "Creating circular buffer with NULL buffer passed instead of failing";
}

CB_TEST(new_nok_buffer_size_nok) {
    auto test_size = [this](size_t size, uint8_t elems = BUFFER_DATA_MAX_ELEMS) {
        EXPECT_FALSE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, &this->circular_buffer_data,
                                         size, elems, sizeof(TypeParam)))
            << "Creating circular buffer buffer of size " << size << "(should be " << (uint16_t)elems << "*"
            << sizeof(TypeParam) << " = " << elems * sizeof(TypeParam) << ") passed instead of failing";
    };

    test_size(0);
    test_size(0, 1);
    test_size(0, 0);
    test_size(sizeof(TypeParam) - 1, 1);
    test_size(sizeof(TypeParam) + 1, 1);
    test_size(std::numeric_limits<size_t>::max(), 1);
}

CB_TEST(new_nok_elems_nok) {
    constexpr size_t PARAM_SIZE = sizeof(TypeParam);

    auto test_elems = [=, this](uint8_t elems, size_t size = TestFixture::MAX_BUFFER_SIZE) {
        EXPECT_FALSE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, &this->circular_buffer_data,
                                         size, elems, PARAM_SIZE))
            << "Creating circular buffer buffer with elements " << (uint16_t)elems << "(should be " << size << "/"
            << PARAM_SIZE << " = " << size / PARAM_SIZE << ") passed instead of failing";
    };

    test_elems(0);
    test_elems(1);
    test_elems(5 * PARAM_SIZE, 4 * PARAM_SIZE);
    test_elems(3 * PARAM_SIZE, 4 * PARAM_SIZE);
    test_elems(std::numeric_limits<uint8_t>::max(), 1 * PARAM_SIZE);
}

CB_TEST(new_nok_elem_size_nok) {
    ASSERT_FALSE(circular_buffer_new(this->circular_buffer, circular_buffer_type_size, &this->circular_buffer_data,
                                     TestFixture::MAX_BUFFER_SIZE, BUFFER_DATA_MAX_ELEMS, 0))
        << "Creating circular buffer with elem_size 0 passed instead of failing";
}

CB_TEST(push_ok) {
    auto cb = this->default_cb();

    std::random_device dev;
    std::mt19937       rng{dev()};

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam e = this->get_elem(i);
        ASSERT_TRUE(circular_buffer_push(cb, &e))
            << "Adding element " << (uint16_t)i << " returned false instead of true";
    }

    auto random_order = std::ranges::to<std::vector>(std::views::iota((uint8_t)0u, TestFixture::DEFAULT_ELEMENTS));
    std::ranges::shuffle(random_order, rng);

    for (const uint8_t i : random_order) {
        TypeParam  e   = this->get_elem(i);
        TypeParam* got = this->cb_typed_at(cb, i);
        ASSERT_NE(got, nullptr) << "Element at index " << (uint16_t)i << " was NULL";
        EXPECT_EQ(*got, e) << "Elements were not ok at index " << (uint16_t)i;
    }
}

CB_TEST(at_ok) {
    std::random_device                     dev;
    std::mt19937                           rng{dev()};
    std::uniform_int_distribution<uint8_t> dist{0};

    auto cb = this->default_cb();

    std::vector<std::pair<uint8_t, TypeParam>> elems;
    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        uint8_t real_index = dist(rng);
        elems.emplace_back(real_index, this->get_elem(real_index));
    }

    for (const auto& [cb_index, pair] : std::views::enumerate(elems)) {
        auto& [i, elem] = pair;
        ASSERT_TRUE(circular_buffer_push(cb, &elem))
            << "Adding element " << cb_index << "/" << (uint16_t)i << " failed instead of passing";
    }

    auto elems_view = std::ranges::to<std::vector>(std::views::enumerate(elems));
    std::ranges::shuffle(elems_view, rng);

    for (const auto& [cb_index, pair] : elems_view) {
        auto& [i, elem] = pair;
        TypeParam* got  = reinterpret_cast<TypeParam*>(circular_buffer_at(cb, cb_index));
        ASSERT_NE(got, nullptr) << "Element at index " << cb_index << "/" << (uint16_t)i << " was NULL";
        EXPECT_EQ(*got, elem) << "Elements were not ok at index " << cb_index << "/" << (uint16_t)i;
    }
}

CB_TEST(push_nok_cb_null) {
    TypeParam e = this->get_elem(0);
    ASSERT_FALSE(circular_buffer_push(NULL, &e))
        << "Pushing element into null circular buffer worked instead of failing";
}

CB_TEST(push_nok_elem_null) {
    auto cb = this->default_cb();
    ASSERT_FALSE(circular_buffer_push(cb, NULL)) << "Pushing null element worked instead of failing";
}

CB_TEST(push_overpush) {
    auto cb = this->default_cb();

    // Adding one more than needed
    uint8_t i = 0;
    for (; i < TestFixture::DEFAULT_ELEMENTS + 1; ++i) {
        TypeParam e = this->get_elem(i);
        ASSERT_TRUE(circular_buffer_push(cb, &e))
            << "Adding element " << (uint16_t)i << " returned false instead of true";
    }

    EXPECT_EQ(*this->cb_typed_at(cb, 0), this->get_elem(1))
        << "Adding one element past the end did not overwrite first element";
    EXPECT_EQ(*this->cb_typed_at(cb, TestFixture::DEFAULT_ELEMENTS - 1), this->get_elem(TestFixture::DEFAULT_ELEMENTS))
        << "Last element was not as expected";

    for (; i < TestFixture::DEFAULT_ELEMENTS * 2; ++i) {
        TypeParam e = this->get_elem(i);
        ASSERT_TRUE(circular_buffer_push(cb, &e))
            << "Adding element " << (uint16_t)i << " returned false instead of true";
    }

    for (uint8_t j = 0; j < TestFixture::DEFAULT_ELEMENTS; ++j) {
        TypeParam  e   = this->get_elem(j + TestFixture::DEFAULT_ELEMENTS);
        TypeParam* got = this->cb_typed_at(cb, j);
        ASSERT_NE(got, nullptr) << "Value at index " << (uint16_t)j << " was NULL";
        EXPECT_EQ(*got, e) << "Value at index " << (uint16_t)j << " was not as expected";
    }
}

CB_TEST(at_nok_cb_null) {
    ASSERT_EQ(circular_buffer_at(NULL, 0), nullptr) << "Reading element of null circular buffer was not NULL";
}

CB_TEST(at_nok_past_length) {
    auto cb = this->default_cb();

    uint8_t i = 0;
    ASSERT_EQ(circular_buffer_at(cb, i), nullptr) << "Reading element of empty circular buffer was not NULL";

    this->cb_typed_push(cb, this->get_elem(i));
    ASSERT_EQ(circular_buffer_at(cb, ++i), nullptr) << "Reading past last element of circular buffer was not NULL";

    this->cb_typed_unshift(cb, this->get_elem(i));
    ASSERT_EQ(circular_buffer_at(cb, ++i), nullptr) << "Reading past last element of circular buffer was not NULL";

    while (!circular_buffer_full(cb)) {
        this->cb_typed_push(cb, this->get_elem(i));
        ASSERT_EQ(circular_buffer_at(cb, ++i), nullptr)
            << "Reading past last element (" << (uint16_t)i << ") of circular buffer was not NULL";
    }

    ASSERT_EQ(circular_buffer_at(cb, TestFixture::DEFAULT_ELEMENTS), nullptr)
        << "Reading past size of circular buffer was not NULL";
}

CB_TEST(at_ok_irregular_start) {
    auto cb = this->default_cb();

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS / 2; ++i) {
        this->cb_typed_push(cb, this->get_elem(0));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_push(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam  e   = this->get_elem(i);
        TypeParam* got = reinterpret_cast<TypeParam*>(circular_buffer_at(cb, i));
        ASSERT_NE(got, nullptr) << "Value at index " << (uint16_t)i << " was NULL";
        EXPECT_EQ(*got, e) << "Value at index " << (uint16_t)i << " was not as expected";
    }
}

CB_TEST(pop_ok) {
    auto cb = this->default_cb();

    this->cb_typed_push(cb, this->get_elem(0));
    TypeParam  got{};
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));

    ASSERT_NE(got_ptr, nullptr) << "Popped value was NULL";
    ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
    ASSERT_EQ(got, this->get_elem(0)) << "Popped value was not equal to pushed value";
    ASSERT_EQ(this->cb_typed_at(cb, 0), nullptr) << "Value was not popped properly";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_push(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam  got{};
        TypeParam* got_ptr    = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));
        uint8_t    last_index = (TestFixture::DEFAULT_ELEMENTS - 1) - i;

        ASSERT_NE(got_ptr, nullptr) << "Popped value was NULL";
        ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
        ASSERT_EQ(got, this->get_elem(last_index)) << "Popped value was not equal to pushed value";
        ASSERT_EQ(this->cb_typed_at(cb, last_index), nullptr) << "Value was not popped properly";
    }
}

CB_TEST(pop_nok_cb_null) {
    TypeParam  got     = this->get_elem(0);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(NULL, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(0)) << "Output parameter was touched when no value was popped";
}

CB_TEST(pop_nok_output_null) {
    auto cb = this->default_cb();

    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, NULL));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
}

CB_TEST(pop_overpop) {
    auto cb = this->default_cb();

    TypeParam  got     = this->get_elem(0);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(0)) << "Output parameter was touched when no value was popped";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_push(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        circular_buffer_pop(cb, &got);
    }

    got     = this->get_elem(1);
    got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(1)) << "Output parameter was touched when no value was popped";
}

CB_TEST(pop_irregular_start) {
    auto cb = this->default_cb();

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS + (TestFixture::DEFAULT_ELEMENTS / 2); ++i) {
        this->cb_typed_push(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam  got{};
        TypeParam* got_ptr    = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));
        uint8_t    last_index = ((TestFixture::DEFAULT_ELEMENTS + (TestFixture::DEFAULT_ELEMENTS / 2)) - 1) - i;

        ASSERT_NE(got_ptr, nullptr) << "Popped value was NULL";
        ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
        ASSERT_EQ(got, this->get_elem(last_index)) << "Popped value was not equal to pushed value";
        ASSERT_EQ(this->cb_typed_at(cb, last_index), nullptr) << "Value was not popped properly";
    }

    TypeParam  got     = this->get_elem(1);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_pop(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(1)) << "Output parameter was touched when no value was popped";
}

CB_TEST(unshift_ok) {
    auto cb = this->default_cb();

    std::random_device dev;
    std::mt19937       rng{dev()};

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        uint8_t   real_index = (TestFixture::DEFAULT_ELEMENTS - 1) - i;
        TypeParam e          = this->get_elem(real_index);
        ASSERT_TRUE(circular_buffer_unshift(cb, &e))
            << "Adding element " << (uint16_t)real_index << " returned false instead of true";
    }

    auto random_order = std::ranges::to<std::vector>(std::views::iota((uint8_t)0u, TestFixture::DEFAULT_ELEMENTS));
    std::ranges::shuffle(random_order, rng);

    for (const uint8_t i : random_order) {
        TypeParam  e   = this->get_elem(i);
        TypeParam* got = this->cb_typed_at(cb, i);
        ASSERT_NE(got, nullptr) << "Element at index " << (uint16_t)i << " was NULL";
        EXPECT_EQ(*got, e) << "Elements were not ok at index " << (uint16_t)i;
    }
}

CB_TEST(unshift_nok_cb_null) {
    TypeParam e = this->get_elem(0);
    ASSERT_FALSE(circular_buffer_unshift(NULL, &e))
        << "Unshiftinging element into null circular buffer worked instead of failing";
}

CB_TEST(unshift_nok_elem_null) {
    auto cb = this->default_cb();
    ASSERT_FALSE(circular_buffer_unshift(cb, NULL)) << "Unshifting null element worked instead of failing";
}

CB_TEST(unshift_overunshift) {
    auto cb = this->default_cb();

    // Adding one more than needed
    uint8_t i = 0;
    for (; i < TestFixture::DEFAULT_ELEMENTS + 1; ++i) {
        uint8_t   real_index = ((TestFixture::DEFAULT_ELEMENTS * 2) - 1) - i;
        TypeParam e          = this->get_elem(real_index);
        ASSERT_TRUE(circular_buffer_unshift(cb, &e))
            << "Adding element " << (uint16_t)real_index << " returned false instead of true";
    }

    EXPECT_EQ(*this->cb_typed_at(cb, 0),
              this->get_elem(TestFixture::DEFAULT_ELEMENTS - 1)) // (DEFAULT_ELEMENTS * 2) - (DEFAULT_ELEMENS + 1)
        << "Adding one element past the first did not overwrite last element";
    EXPECT_EQ(*this->cb_typed_at(cb, TestFixture::DEFAULT_ELEMENTS - 1),
              this->get_elem((TestFixture::DEFAULT_ELEMENTS * 2) - 2)) // ((DEFAULT_ELEMENTS * 2) - 1) - 1
        << "Last element was not as expected";

    for (; i < TestFixture::DEFAULT_ELEMENTS * 2; ++i) {
        uint8_t   real_index = ((TestFixture::DEFAULT_ELEMENTS * 2) - 1) - i;
        TypeParam e          = this->get_elem(real_index);
        ASSERT_TRUE(circular_buffer_unshift(cb, &e))
            << "Adding element " << (uint16_t)real_index << " returned false instead of true";
    }

    for (uint8_t j = 0; j < TestFixture::DEFAULT_ELEMENTS; ++j) {
        TypeParam  e   = this->get_elem(j);
        TypeParam* got = this->cb_typed_at(cb, j);
        ASSERT_NE(got, nullptr) << "Value at index " << (uint16_t)j << " was NULL";
        EXPECT_EQ(*got, e) << "Value at index " << (uint16_t)j << " was not as expected";
    }
}

CB_TEST(shift_ok) {
    auto cb = this->default_cb();

    this->cb_typed_unshift(cb, this->get_elem(0));
    TypeParam  got{};
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));

    ASSERT_NE(got_ptr, nullptr) << "Unshifted value was NULL";
    ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
    ASSERT_EQ(got, this->get_elem(0)) << "Unshifted value was not equal to pushed value";
    ASSERT_EQ(this->cb_typed_at(cb, 0), nullptr) << "Value was not unshifted properly";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam  got{};
        TypeParam* got_ptr    = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));
        uint8_t    last_index = (TestFixture::DEFAULT_ELEMENTS - 1) - i;

        ASSERT_NE(got_ptr, nullptr) << "Unshifted value was NULL";
        ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
        ASSERT_EQ(got, this->get_elem(last_index)) << "Unshifted value was not equal to pushed value";
        ASSERT_EQ(this->cb_typed_at(cb, last_index), nullptr) << "Value was not unshifted properly";
    }
}

CB_TEST(shift_nok_cb_null) {
    TypeParam  got     = this->get_elem(0);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(NULL, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(0)) << "Output parameter was touched when no value was shifted";
}

CB_TEST(shift_nok_output_null) {
    auto cb = this->default_cb();

    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, NULL));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
}

CB_TEST(shift_overshift) {
    auto cb = this->default_cb();

    TypeParam  got     = this->get_elem(0);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(0)) << "Output parameter was touched when no value was popped";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        circular_buffer_shift(cb, &got);
    }

    got     = this->get_elem(1);
    got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(1)) << "Output parameter was touched when no value was shift";
}

CB_TEST(shift_irregular_start) {
    auto cb = this->default_cb();

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS + (TestFixture::DEFAULT_ELEMENTS / 2); ++i) {
        this->cb_typed_unshift(cb, this->get_elem(i));
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        TypeParam  got{};
        TypeParam* got_ptr    = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));
        uint8_t    last_index = ((TestFixture::DEFAULT_ELEMENTS + (TestFixture::DEFAULT_ELEMENTS / 2)) - 1) - i;

        ASSERT_NE(got_ptr, nullptr) << "Shifted value was NULL";
        ASSERT_EQ(&got, got_ptr) << "Pointer returned was not the same as address of output param";
        ASSERT_EQ(got, this->get_elem(last_index)) << "Shifted value was not equal to pushed value";
        ASSERT_EQ(this->cb_typed_at(cb, last_index), nullptr) << "Value was not shifted properly";
    }

    TypeParam  got     = this->get_elem(1);
    TypeParam* got_ptr = reinterpret_cast<TypeParam*>(circular_buffer_shift(cb, &got));

    ASSERT_EQ(got_ptr, nullptr) << "Value returned from null circular buffer was not NULL";
    ASSERT_EQ(got, this->get_elem(1)) << "Output parameter was touched when no value was shifted";
}

CB_TEST(full_ok) {
    auto cb = this->default_cb();

    TypeParam got = this->get_elem(0);

    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full with 0 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after pushing 1 elements";

    this->cb_typed_pop(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after shifting out 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_push(cb, this->get_elem(0));
        ASSERT_FALSE(circular_buffer_full(cb))
            << "Circular buffer reported full after pushing " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_TRUE(circular_buffer_full(cb)) << "Circular buffer did not report empty after pushing "
                                          << (uint16_t)TestFixture::DEFAULT_ELEMENTS << " elements";

    this->cb_typed_pop(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_TRUE(circular_buffer_full(cb)) << "Circular buffer did not report empty after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after shifting out 1 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_pop(cb, got);
        ASSERT_FALSE(circular_buffer_full(cb))
            << "Circular buffer reported full after popping " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_pop(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after popping all element";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(0));
        ASSERT_FALSE(circular_buffer_full(cb))
            << "Circular buffer reported full after unshifting " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_TRUE(circular_buffer_full(cb)) << "Circular buffer did not report empty after unshifting "
                                          << (uint16_t)TestFixture::DEFAULT_ELEMENTS << " elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_shift(cb, got);
        ASSERT_FALSE(circular_buffer_full(cb))
            << "Circular buffer reported full after shifting " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_shift(cb, got);
    ASSERT_FALSE(circular_buffer_full(cb)) << "Circular buffer reported full after shifting all element";
}

CB_TEST(full_nok_cb_null) {
    ASSERT_TRUE(circular_buffer_full(NULL)) << "Null circular buffer did not report full";
}

CB_TEST(empty_ok) {
    auto cb = this->default_cb();

    TypeParam got = this->get_elem(0);

    ASSERT_TRUE(circular_buffer_empty(cb)) << "Circular buffer did not report empty with 0 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_empty(cb)) << "Circular buffer reported empty after pushing 1 elements";

    this->cb_typed_pop(cb, got);
    ASSERT_TRUE(circular_buffer_empty(cb)) << "Circular buffer did not report empty after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_empty(cb)) << "Circular buffer reported empty after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_TRUE(circular_buffer_empty(cb)) << "Circular buffer reported empty after shifting out 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_push(cb, this->get_elem(0));
        ASSERT_FALSE(circular_buffer_empty(cb))
            << "Circular buffer reported empty after pushing " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_empty(cb))
        << "Circular buffer reported empty after pushing " << (uint16_t)TestFixture::DEFAULT_ELEMENTS << " elements";

    this->cb_typed_pop(cb, got);
    ASSERT_FALSE(circular_buffer_empty(cb)) << "Circular buffer reported empty after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_empty(cb)) << "Circular buffer reported empty after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_FALSE(circular_buffer_empty(cb)) << "Circular buffer reported empty after shifting out 1 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_pop(cb, got);
        ASSERT_FALSE(circular_buffer_empty(cb))
            << "Circular buffer reported empty after popping " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_pop(cb, got);
    ASSERT_TRUE(circular_buffer_empty(cb)) << "Circular buffer did not report empty after popping all element";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(0));
        ASSERT_FALSE(circular_buffer_empty(cb))
            << "Circular buffer reported empty after unshifting " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_FALSE(circular_buffer_empty(cb))
        << "Circular buffer reported empty after unshifting " << (uint16_t)TestFixture::DEFAULT_ELEMENTS << " elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS - 1; ++i) {
        this->cb_typed_shift(cb, got);
        ASSERT_FALSE(circular_buffer_empty(cb))
            << "Circular buffer reported empty after shifting " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_shift(cb, got);
    ASSERT_TRUE(circular_buffer_empty(cb)) << "Circular buffer did not report empty after shifting all element";
}

CB_TEST(empty_nok_cb_null) {
    ASSERT_TRUE(circular_buffer_empty(NULL)) << "Null circular buffer did not report empty";
}

CB_TEST(length_ok) {
    auto cb = this->default_cb();

    TypeParam got = this->get_elem(0);

    ASSERT_EQ(circular_buffer_length(cb), 0) << "Circular buffer did not report length of 0";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_length(cb), 1) << "Circular buffer did not report length of 1 after pushing 1 elements";

    this->cb_typed_pop(cb, got);
    ASSERT_EQ(circular_buffer_length(cb), 0) << "Circular buffer did not report length of 0 after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_length(cb), 1)
        << "Circular buffer did not report length of 1 after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_EQ(circular_buffer_length(cb), 0)
        << "Circular buffer did not report length of 0 after shifting out 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_push(cb, this->get_elem(0));
        ASSERT_EQ(circular_buffer_length(cb), i + 1) << "Circular buffer did not report length of " << (uint16_t)(i + 1)
                                                     << " after pushing " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_pop(cb, got);
    ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS - 1)
        << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS - 1
        << " after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS
        << " after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS - 1)
        << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS - 1
        << " after shifting out 1 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS
        << " after pushing in 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_pop(cb, got);
        ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS - (i + 1))
            << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS - (i + 1)
            << " after popping " << (uint16_t)(i + 1) << " elements";
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(0));
        ASSERT_EQ(circular_buffer_length(cb), i + 1) << "Circular buffer did not report length of " << (uint16_t)i + 1
                                                     << " after unshifting " << (uint16_t)(i + 1) << " elements";
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_shift(cb, got);
        ASSERT_EQ(circular_buffer_length(cb), TestFixture::DEFAULT_ELEMENTS - (i + 1))
            << "Circular buffer did not report length of " << (uint16_t)TestFixture::DEFAULT_ELEMENTS - (i + 1)
            << " after shifting " << (uint16_t)(i + 1) << " elements";
    }
}

CB_TEST(length_nok_cb_null) {
    ASSERT_EQ(circular_buffer_length(NULL), 0) << "Null circular buffer did not report length 0";
}

CB_TEST(size_ok) {
    auto cb = this->default_cb();
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer of size " << (uint16_t)TestFixture::DEFAULT_ELEMENTS << " reported something else";

    cb = this->default_cb(1);
    ASSERT_EQ(circular_buffer_size(cb), 1) << "Circular buffer of size 1 reported something else";

    cb = this->default_cb(BUFFER_DATA_MAX_ELEMS);
    ASSERT_EQ(circular_buffer_size(cb), BUFFER_DATA_MAX_ELEMS)
        << "Circular buffer of size " << BUFFER_DATA_MAX_ELEMS << " reported something else";

    std::random_device                     dev;
    std::mt19937                           rng{dev()};
    std::uniform_int_distribution<uint8_t> dist{0};
    uint8_t                                cb_size = dist(rng);

    cb = this->default_cb(cb_size);
    ASSERT_EQ(circular_buffer_size(cb), cb_size)
        << "Circular buffer of size " << (uint16_t)cb_size << " reported something else";
}

CB_TEST(size_nok_cb_null) {
    ASSERT_EQ(circular_buffer_size(NULL), 0) << "Null circular buffer did not report size 0";
}

CB_TEST(size_ok_unchanging) {
    auto cb = this->default_cb();

    TypeParam got = this->get_elem(0);

    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS) << "Circular buffer did not report correct size";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after pushing 1 elements";

    this->cb_typed_pop(cb, got);
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after shifting out 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_push(cb, this->get_elem(0));
        ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
            << "Circular buffer did not report correct size after pushing " << (uint16_t)(i + 1) << " elements";
    }

    this->cb_typed_pop(cb, got);
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after popping 1 element";

    this->cb_typed_unshift(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after unshifting in 1 elements";

    this->cb_typed_shift(cb, got);
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after shifting out 1 elements";

    this->cb_typed_push(cb, this->get_elem(0));
    ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
        << "Circular buffer did not report correct size after pushing in 1 elements";

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_pop(cb, got);
        ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
            << "Circular buffer did not report correct size after popping " << (uint16_t)(i + 1) << " elements";
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_unshift(cb, this->get_elem(0));
        ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
            << "Circular buffer did not report correct size after unshifting " << (uint16_t)(i + 1) << " elements";
    }

    for (uint8_t i = 0; i < TestFixture::DEFAULT_ELEMENTS; ++i) {
        this->cb_typed_shift(cb, got);
        ASSERT_EQ(circular_buffer_size(cb), TestFixture::DEFAULT_ELEMENTS)
            << "Circular buffer did not report correct size after shifting " << (uint16_t)(i + 1) << " elements";
    }
}