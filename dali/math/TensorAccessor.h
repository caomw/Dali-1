#ifndef DALI_MATH_TENSOR_ACCESSOR_H
#define DALI_MATH_TENSOR_ACCESSOR_H


#include <mshadow/tensor.h>

#include "dali/math/TensorInternal.h"
#include "dali/math/ThrustUtils.h"



namespace TensorOps {
    using mshadow::gpu;
    using mshadow::cpu;
    /////////////////////// select from cols /////////////////////////////////////////

    #ifdef DALI_USE_CUDA

    template<typename R>
    void select_from_cols(mshadow::Tensor<gpu, 2, R> dest,
                          const mshadow::Tensor<gpu, 2, R>& source,
                          Indexing::Index targets) {
        auto t_dest   = to_thrust(dest);
        auto t_source = to_thrust(source);
        std::vector<int> offsets(targets.size());
        for (int i=0; i < targets.size(); ++i) {
            // accessing index (targets[i], i)
            offsets[i] = targets[i] * source.shape_[1] + i;
        }
        thrust::device_vector<uint> offsets_gpu(offsets);

        // typedef thrust::device_vector<R>::iterator ElementIterator;
        typedef thrust::device_vector<uint>::iterator IndexIterator;
        thrust::permutation_iterator<thrust::device_ptr<R>,IndexIterator>
                iter(t_source, offsets_gpu.begin());
        thrust::copy(iter, iter + source.shape_[1], t_dest);
    }
    #endif

    template<typename R>
    void select_from_cols(mshadow::Tensor<cpu, 2, R> dest,
                          const mshadow::Tensor<cpu, 2, R>& source,
                          Indexing::Index targets) {

        R* dest_ptr = dest.dptr_;

        for (int col = 0; col < source.shape_[1]; ++col) {
            *(dest_ptr + col) = source[targets[col]][col];
        }
    }


    template<typename R>
    void select_from_cols(TensorInternal<R,2> dest,
                          TensorInternal<R,2> source,
                          Indexing::Index targets) {
        #ifdef DALI_USE_CUDA
        if (source.compute_me_on_gpu()) {
            select_from_cols(dest.mutable_gpu_data(), source.gpu_data(), targets);
            return;
        }
        #endif
        select_from_cols(dest.mutable_cpu_data(), source.cpu_data(), targets);
    }

    /////////////////////// rows_pluck /////////////////////////////////////////


    #ifdef DALI_USE_CUDA

    template<typename R>
    void rows_pluck(mshadow::Tensor<gpu, 2, R> dest,
                    const mshadow::Tensor<gpu, 2, R>& source,
                    TensorInternal<int,1> indices) {
        using namespace thrust::placeholders;

        auto t_dest   = to_thrust(dest);
        auto t_source = to_thrust(source);

        for (int idx = 0; idx < indices.number_of_elements(); ++idx) {
            int row_size =  source.shape_[1];

            auto dest_column_idx = thrust::make_transform_iterator(
                thrust::counting_iterator<int>(0),
                _1 * dest.shape_[1] + idx
            );
            auto dest_column = make_permutation_iterator(t_dest, dest_column_idx);

            auto source_row_begin = t_source + indices(idx) * row_size;

            thrust::copy(source_row_begin , source_row_begin + row_size, dest_column);
        }
    }
    #endif

    template<typename R>
    void rows_pluck(mshadow::Tensor<cpu, 2, R> dest,
                    const mshadow::Tensor<cpu, 2, R>& source,
                    TensorInternal<int,1> indices) {
        for (int idx = 0; idx < indices.number_of_elements(); ++idx) {
            for (int col_idx = 0; col_idx < dest.shape_[0]; ++col_idx) {
                dest[col_idx][idx] = source[indices(idx)][col_idx];
            }
        }
    }


    template<typename R>
    void rows_pluck(TensorInternal<R,2> dest,
                    TensorInternal<R,2> source,
                    TensorInternal<int,1> indices) {
        #ifdef DALI_USE_CUDA
        if (source.compute_me_on_gpu()) {
            rows_pluck(dest.mutable_gpu_data(), source.gpu_data(), indices);
            return;
        }
        #endif
        rows_pluck(dest.mutable_cpu_data(), source.cpu_data(), indices);
    }


    /////////////////////// rows_pluck_backprop /////////////////////////////////


    #ifdef DALI_USE_CUDA

    template<typename R>
    void rows_pluck_backprop(mshadow::Tensor<gpu, 2, R> dest,
                    const mshadow::Tensor<gpu, 2, R>& source,
                    TensorInternal<int,1> indices) {
        using namespace thrust::placeholders;

        auto t_dest   = to_thrust(dest);
        auto t_source = to_thrust(source);

        for (int idx = 0; idx < indices.number_of_elements(); ++idx) {
            int row_size =  dest.shape_[1];

            auto source_column_idx = thrust::make_transform_iterator(
                thrust::counting_iterator<int>(0),
                _1 * source.shape_[1] + idx
            );
            auto source_column = make_permutation_iterator(t_source, source_column_idx);

            auto dest_row_begin = t_dest + indices(idx) * row_size;
            thrust::transform(dest_row_begin, dest_row_begin + row_size, source_column, dest_row_begin, _1 + _2);
        }
    }
    #endif

    template<typename R>
    void rows_pluck_backprop(mshadow::Tensor<cpu, 2, R> dest,
                    const mshadow::Tensor<cpu, 2, R>& source,
                    TensorInternal<int,1> indices) {
        for (int idx = 0; idx < indices.number_of_elements(); ++idx) {
            for (int col_idx = 0; col_idx < dest.shape_[1]; ++col_idx) {
                dest[indices(idx)][col_idx] += source[col_idx][idx];
            }
        }
    }


    template<typename R>
    void rows_pluck_backprop(TensorInternal<R,2> dest,
                    TensorInternal<R,2> source,
                    TensorInternal<int,1> indices) {
        #ifdef DALI_USE_CUDA
        if (source.compute_me_on_gpu()) {
            rows_pluck_backprop(dest.mutable_gpu_data(), source.gpu_data(), indices);
            return;
        }
        #endif
        rows_pluck_backprop(dest.mutable_cpu_data(), source.cpu_data(), indices);
    }
};

#endif