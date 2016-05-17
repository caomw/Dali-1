#include <chrono>
#include <vector>
#include <iomanip>
#include <gtest/gtest.h>
#include "dali/config.h"
#include <mshadow/tensor.h>

#include "dali/array/function/typed_array.h"
#include "dali/array/op/unary.h"
#include "dali/array/op/binary.h"
#include "dali/array/op/other.h"
#include "dali/array/op/initializer.h"
#include "dali/utils/print_utils.h"
#include "dali/runtime_config.h"
#include "dali/array/op.h"

#include "dali/array/lazy/binary.h"

using std::vector;

typedef vector<int> VI;

TEST(ArrayTests, slicing) {
    Array x({12});
    Array y({3,2,2});

    EXPECT_THROW(x[0][0], std::runtime_error);
    EXPECT_THROW(y[3], std::runtime_error);

    EXPECT_EQ(y[0].ndim(), 2);
    EXPECT_EQ(y[1].ndim(), 2);
    EXPECT_EQ(y[2].ndim(), 2);
    EXPECT_EQ(y[2][1].ndim(), 1);
    EXPECT_EQ(y[2][1][0].ndim(), 0);

    EXPECT_EQ(x[0].ndim(), 0);

    EXPECT_EQ(x(0).ndim(), 0);
    EXPECT_EQ(y(0).ndim(), 0);
}

TEST(ArrayTests, scalar_value) {
    Array x({12}, DTYPE_INT32);
    x(3) = 42;
    int& x_val = x(3);
    EXPECT_EQ(x_val, 42);
    x[3] = 56;
    EXPECT_EQ(x_val, 56);
}

TEST(ArrayTests, scalar_assign) {
    Array x = Array::zeros({3,2}, DTYPE_INT32);
    x = 13;

    ASSERT_EQ(x.shape(), std::vector<int>({3,2}));
    ASSERT_EQ(x.dtype(), DTYPE_INT32);
    for (int i=0; i < 6; ++i) {
        ASSERT_EQ((int)x(i), 13);
    }

    x = 69.1;
    ASSERT_EQ(x.shape(), std::vector<int>({3,2}));
    ASSERT_EQ(x.dtype(), DTYPE_INT32);
    for (int i=0; i <6; ++i) {
        ASSERT_EQ((int)x(i), 69);
    }
}

TEST(ArrayTests, inplace_addition) {
    Array x = Array::zeros({3,2}, DTYPE_INT32);
    x = 13;
    x += 2;
    ASSERT_EQ((int)(Array)x.sum(), 13 * 6 + 2 * 6);

    auto prev_memory_ptr = x.memory().get();
    // add a different number in place to each element and check
    // the result is correct
    x += Array::arange({3, 2}, DTYPE_INT32);
    // verify that memory pointer is the same
    // (to be sure this was actually done in place)
    ASSERT_EQ(prev_memory_ptr, x.memory().get());
    for (int i = 0; i < x.number_of_elements(); i++) {
        ASSERT_EQ((int)x(i), (13 + 2) + i);
    }
}

TEST(ArrayTests, inplace_substraction) {
    Array x = Array::zeros({3,2}, DTYPE_INT32);
    x = 13;
    x -= 2;
    ASSERT_EQ((int)(Array)x.sum(), 13 * 6 - 2 * 6);

    auto prev_memory_ptr = x.memory().get();
    // add a different number in place to each element and check
    // the result is correct
    x -= Array::arange({3, 2}, DTYPE_INT32);
    // verify that memory pointer is the same
    // (to be sure this was actually done in place)
    ASSERT_EQ(prev_memory_ptr, x.memory().get());
    for (int i = 0; i < x.number_of_elements(); i++) {
        ASSERT_EQ((int)x(i), (13 - 2) - i);
    }
}

TEST(ArrayTests, inplace_multiplication) {
    Array x = Array::zeros({3,2}, DTYPE_INT32);
    x = 13;
    x *= 2;
    ASSERT_EQ((int)(Array)x.sum(), 13 * 6 * 2);

    auto prev_memory_ptr = x.memory().get();
    // add a different number in place to each element and check
    // the result is correct
    x *= Array::arange({3, 2}, DTYPE_INT32);
    // verify that memory pointer is the same
    // (to be sure this was actually done in place)
    ASSERT_EQ(prev_memory_ptr, x.memory().get());
    for (int i = 0; i < x.number_of_elements(); i++) {
        ASSERT_EQ((int)x(i), (13 * 2) * i);
    }
}

TEST(ArrayTests, scalar_construct) {
    auto assignable = initializer::fill((float)3.14);
    Array scalar = assignable;
    ASSERT_EQ(scalar.shape(), std::vector<int>());
    ASSERT_EQ(scalar.dtype(), DTYPE_FLOAT);
    ASSERT_NEAR((float)scalar(0), 3.14, 1e-6);

    Array scalar2;
    scalar2 = initializer::fill((double)3.14);
    ASSERT_EQ(scalar2.shape(), std::vector<int>());
    ASSERT_EQ(scalar2.dtype(), DTYPE_DOUBLE);
    ASSERT_NEAR((double)scalar2(0), 3.14, 1e-6);

    Array scalar3 = initializer::fill(314);
    ASSERT_EQ(scalar3.shape(), std::vector<int>());
    ASSERT_EQ(scalar3.dtype(), DTYPE_INT32);
    ASSERT_EQ((int)scalar3(0), 314);
}


TEST(ArrayTests, spans_entire_memory) {
    // an array is said to span its entire memory
    // if it is not a "view" onto said memory.

    // the following 3D tensor spans its entire memory
    // (in fact it even allocated it!)
    Array x = Array::zeros({3,2,2});
    ASSERT_TRUE(x.spans_entire_memory());

    // however a slice of x may not have the same property:
    auto subx = x[0];
    ASSERT_FALSE(subx.spans_entire_memory());

    // Now let's take a corner case:
    // the leading dimension of the following
    // array is 1, so taking a view at "row" 0
    // makes no difference in terms of underlying
    // memory hence, both it and its subview will
    // "span the entire memory"
    Array y = Array::zeros({1,2,2});
    ASSERT_TRUE(y.spans_entire_memory());

    auto view_onto_y = y[0];
    ASSERT_TRUE(view_onto_y.spans_entire_memory());
}

// Some example integer 3D tensor with
// values from 0 to 23
Array build_234_arange() {
    // [
    //   [
    //     [ 0  1  2  3 ],
    //     [ 4  5  6  7 ],
    //     [ 8  9  10 11],
    //   ],
    //   [
    //     [ 12 13 14 15],
    //     [ 16 17 18 19],
    //     [ 20 21 22 23],
    //   ]
    // ]
    Array x({2,3,4}, DTYPE_INT32);
    x = initializer::arange();
    return x;
}


void copy_constructor_helper(bool copy_w) {

}

TEST(ArrayTests, copy_constructor) {
    for (auto copy_w : {true, false}) {
        Array original({3,3}, DTYPE_INT32);
        original = initializer::arange();
        Array copy(original, copy_w);
        copy += 1;

        for (int i = 0;  i < original.number_of_elements(); i++) {
            if (copy_w) {
                // since +1 was done after copy
                // change is not reflected
                EXPECT_NE((int)original(i), (int)copy(i));
            } else {
                // copy is a view, so +1 affects both
                EXPECT_EQ((int)original(i), (int)copy(i));
            }
        }
    }

    Array original = Array({3}, DTYPE_INT32)[Slice(0, 3)][Broadcast()];
    // perform copy of broadcasted data
    Array hard_copy(original, true);
    EXPECT_EQ(original.bshape(), hard_copy.bshape());

    Array soft_copy(original, false);
    EXPECT_EQ(original.bshape(), soft_copy.bshape());

    // 'operator=' uses soft copy too:
    auto soft_copy_assign = original;
    EXPECT_EQ(original.bshape(), soft_copy_assign.bshape());

    // now give the broadcasted dimension
    // a size, and assert that the copy
    // doesn't replicate those useless dimensions
    auto original_bigger = original.reshape_broadcasted({3, 20});
    Array hard_copy_bigger(original_bigger, true);
    EXPECT_EQ(hard_copy_bigger.shape(), original_bigger.shape());
    EXPECT_NE(hard_copy_bigger.bshape(), original_bigger.bshape());
}



TEST(ArrayTests, contiguous_memory) {
    auto x = build_234_arange();
    EXPECT_TRUE(x.contiguous_memory());
}

TEST(ArrayTests, pluck_axis_stride_shape) {
    auto x = build_234_arange();

    auto x_plucked = x.pluck_axis(0, 1);
    EXPECT_EQ(x_plucked.shape(),   vector<int>({3, 4}));
    EXPECT_EQ(x_plucked.number_of_elements(), 12);
    EXPECT_EQ(x_plucked.offset(),  12    );
    // if all strides are 1, then return empty vector
    EXPECT_EQ(x_plucked.strides(), vector<int>({}));

    auto x_plucked2 = x.pluck_axis(1, 2);
    EXPECT_EQ(x_plucked2.shape(),   vector<int>({2, 4}));
    EXPECT_EQ(x_plucked2.number_of_elements(), 8);
    EXPECT_EQ(x_plucked2.offset(),   8    );
    EXPECT_EQ(x_plucked2.strides(), vector<int>({12, 1}));

    auto x_plucked3 = x.pluck_axis(2, 1);
    EXPECT_EQ(x_plucked3.shape(),   vector<int>({2, 3}));
    EXPECT_EQ(x_plucked3.number_of_elements(), 6);
    EXPECT_EQ(x_plucked3.offset(),  1);
    EXPECT_EQ(x_plucked3.strides(), vector<int>({12, 4}));
}


TEST(ArrayTests, slice_size) {
    ASSERT_EQ(5, Slice(0,5).size());
    ASSERT_EQ(2, Slice(2,4).size());
    ASSERT_EQ(3, Slice(0,5,2).size());
    ASSERT_EQ(3, Slice(0,5,-2).size());
    ASSERT_EQ(2, Slice(0,6,3).size());
    ASSERT_EQ(2, Slice(0,6,-3).size());
    ASSERT_EQ(3, Slice(0,7,3).size());
    ASSERT_EQ(3, Slice(0,7,-3).size());

    ASSERT_THROW(Slice(0,2,0),  std::runtime_error);
}

TEST(ArrayTests, slice_contains) {
    EXPECT_TRUE(Slice(0,12,2).contains(0));
    EXPECT_FALSE(Slice(0,12,2).contains(1));

    EXPECT_FALSE(Slice(0,12,-2).contains(0));
    EXPECT_TRUE(Slice(0,12,-2).contains(1));
}


TEST(ArrayTests, pluck_axis_eval) {
    auto x = build_234_arange();

    auto x_plucked = x.pluck_axis(0, 0);
    EXPECT_EQ(x.memory().get(), x_plucked.memory().get());
    EXPECT_EQ(
        (int)(Array)x_plucked.sum(),
        0 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11
    );

    auto x_plucked2 = x.pluck_axis(1, 2);
    EXPECT_EQ(x.memory().get(), x_plucked2.memory().get());
    EXPECT_FALSE(x_plucked2.contiguous_memory());
    EXPECT_EQ(
        (int)(Array)x_plucked2.sum(),
        8 + 9 + 10 + 11 + 20 + 21 + 22 + 23
    );

    auto x_plucked3 = x.pluck_axis(2, 1);

    EXPECT_EQ(x.memory().get(), x_plucked3.memory().get());
    EXPECT_FALSE(x_plucked3.contiguous_memory());
    EXPECT_EQ(
        (int)(Array)x_plucked3.sum(),
        1 + 5 + 9 + 13 + 17 + 21
    );
}

TEST(ArrayTests, inplace_strided_addition) {
    auto x = build_234_arange();
    auto x_plucked = x.pluck_axis(2, 1);
    // strided dimension pluck is a view
    EXPECT_EQ(x_plucked.memory().get(), x.memory().get());
    // we now modify this view by in-place incrementation:
    // sum was originally 66, should now be 72:
    x_plucked += 1;
    // sum is now same as before + number of elements
    EXPECT_EQ(
        (int)(Array)x_plucked.sum(),
        x_plucked.number_of_elements() + (1 + 5 + 9 + 13 + 17 + 21)
    );
}

TEST(ArrayTests, canonical_reshape) {
    ASSERT_EQ(mshadow::Shape1(60),        internal::canonical_reshape<1>({3,4,5}));
    ASSERT_EQ(mshadow::Shape2(12,5),      internal::canonical_reshape<2>({3,4,5}));
    ASSERT_EQ(mshadow::Shape3(3,4,5),     internal::canonical_reshape<3>({3,4,5}));
    ASSERT_EQ(mshadow::Shape4(1,3,4,5),   internal::canonical_reshape<4>({3,4,5}));
}

std::vector<Slice> generate_interesting_slices(int dim_size) {
    std::vector<Slice> interesting_slices;
    for (int start = 0; start < dim_size; ++start) {
        for (int end = start + 1; end <= dim_size; ++end) {
            for (int step = -2; step < 3; ++step) {
                if (step == 0) continue;
                interesting_slices.push_back(Slice(start,end,step));
            }
        }
    }
    EXPECT_TRUE(interesting_slices.size() < 50);
    return interesting_slices;
}

TEST(ArrayTests, proper_slicing) {
    Array x = build_234_arange();
    Array sliced = x[Slice(0,-1)][2][Slice(0,4,-2)];

    Array sliced_sum = sliced.sum();
    ASSERT_EQ(20, (int)sliced_sum);
}

TEST(ArrayTests, double_striding) {
    const int NRETRIES = 2;
    for (int retry=0; retry < NRETRIES; ++retry) {

        Array x({2,3,4}, DTYPE_INT32);
        x = initializer::uniform(-1000, 1000);

        for (auto& slice0: generate_interesting_slices(2)) {
            for (auto& slice1: generate_interesting_slices(3)) {
                for (auto& slice2: generate_interesting_slices(4)) {
                    // SCOPED_TRACE(std::string(utils::MS() << "x[" << slice0 << "][" << slice1 <<  "][" <<  slice2 << "]"));
                    Array sliced = x[slice0][slice1][slice2];
                    int actual_sum = (Array)sliced.sum();
                    int expected_sum = 0;
                    for (int i=0; i < 2; ++i) {
                        for (int j=0; j<3; ++j) {
                            for (int k=0; k<4; ++k) {
                                if (slice0.contains(i) && slice1.contains(j) && slice2.contains(k)) {
                                    // avoiding the use of [] here because [] itself
                                    // does striding.
                                    expected_sum += (int)x(i*12 + j*4 + k);
                                }
                            }
                        }
                    }
                    ASSERT_EQ(expected_sum, actual_sum);
                }
            }
        }
    }
}

TEST(ArrayLazyOpsTests, reshape_broadcasted) {
    auto B = Array::ones({3},     DTYPE_INT32);

    B = B[Broadcast()][Slice(0,3)][Broadcast()];
    B = B.reshape_broadcasted({2,3,4});

    ASSERT_EQ((int)(Array)B.sum(), 2 * 3 * 4);
}

TEST(ArrayLazyOpsTests, reshape_broadcasted2) {
    auto B = Array::ones({3},     DTYPE_INT32);
    B = B[Broadcast()][Slice(0,3)][Broadcast()];

    B = B.reshape_broadcasted({2, 3, 1});
    B = B.reshape_broadcasted({2, 3, 1});
    B = B.reshape_broadcasted({2, 3, 5});
    B = B.reshape_broadcasted({2, 3, 5});

    EXPECT_THROW(B.reshape_broadcasted({5,3,5}), std::runtime_error);
    EXPECT_THROW(B.reshape_broadcasted({1,3,5}), std::runtime_error);
    EXPECT_THROW(B.reshape_broadcasted({2,3,1}), std::runtime_error);
}

TEST(ArrayTests, strides_compacted_after_expansion) {
    Array x = Array::zeros({2,3,4});

    EXPECT_EQ(x.expand_dims(0).strides(), vector<int>());
    EXPECT_EQ(x.expand_dims(1).strides(), vector<int>());
    EXPECT_EQ(x.expand_dims(2).strides(), vector<int>());
    EXPECT_EQ(x.expand_dims(3).strides(), vector<int>());
}

// use bracket to compute flat sequence of element in array.
void sequence_array(Array x, std::vector<int>& output) {
    if (x.ndim() == 0) {
        output.push_back((int)x);
    } else {
        for (int i = 0; i < x.shape()[0]; ++i) {
            sequence_array(x[i], output);
        }
    }
}

void ensure_call_operator_correct(Array x) {
    std::vector<int> correct_elem_sequence;

    sequence_array(x, correct_elem_sequence);
    for (int i = 0; i < x.number_of_elements(); ++i) {
        EXPECT_EQ(correct_elem_sequence[i], (int)x(i));
    }
}

TEST(ArrayTest, strided_call_operator) {
    Array x = build_234_arange();
    ensure_call_operator_correct(x);

    Array x2 = x[Slice(0,2)][2];
    ensure_call_operator_correct(x2);

    Array x3 = x[Slice(0,2, -1)][2];
    ensure_call_operator_correct(x2);

    Array y({2, 2}, DTYPE_INT32);
    y = initializer::arange();
    ensure_call_operator_correct(y);

    Array y2 = y[Slice(0,2)][Broadcast()][Slice(0,2)];
    ensure_call_operator_correct(y2);

    Array y3 = y2.reshape_broadcasted({2,3,2});
    ensure_call_operator_correct(y3);
}

TEST(ArrayTest, transpose) {
    Array x = Array::zeros({2},     DTYPE_INT32);
    Array y = Array::zeros({2,3},   DTYPE_INT32);
    Array z = Array::zeros({2,3,4}, DTYPE_INT32);

    auto x_T = x.transpose();
    auto y_T = y.transpose();
    auto z_T = z.transpose();

    ASSERT_EQ(VI({2}),     x_T.shape());
    ASSERT_EQ(VI({3,2}),   y_T.shape());
    ASSERT_EQ(VI({4,3,2}), z_T.shape());

    for (int i = 0; i < 2; ++i) {
        ASSERT_EQ((int)x[i], (int)x_T[i]);
    }

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) {
            ASSERT_EQ((int)y[i][j], (int)y_T[j][i]);
        }
    }

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 4; ++k) {
                ASSERT_EQ((int)z[i][j][k], (int)z_T[k][j][i]);
            }
        }
    }

    auto z_T_funny = z.transpose({1,0,2});
    ASSERT_EQ(VI({3,2,4}), z_T_funny.shape());

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 4; ++k) {
                ASSERT_EQ((int)z[i][j][k], (int)z_T_funny[j][i][k]);
            }
        }
    }
}
