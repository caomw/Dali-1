#include "dali/math/SynchronizedMemory.h"
#include "dali/math/LazyTensor.h"
#include "dali/math/TensorOps.h"

using mshadow::AllocSpace;
using mshadow::FreeSpace;
using mshadow::Shape2;
using mshadow::Tensor;
using mshadow::Copy;

template<int dimension>
std::ostream &operator <<(std::ostream &os, const mshadow::Shape<dimension> &shape) {
    if (dimension == 0) {
        return os << "<shape ()>";
    } else {
        os << "<shape (";
        for (int i = 0; i < dimension;i++) {
            os << shape[i];
            if (i != dimension - 1) os << ", ";
        }
        os << ")>";
        return os;
    }
}

template std::ostream& operator<< <0>(std::ostream& strm, const mshadow::Shape<0>& a);
template std::ostream& operator<< <1>(std::ostream& strm, const mshadow::Shape<1>& a);
template std::ostream& operator<< <2>(std::ostream& strm, const mshadow::Shape<2>& a);
template std::ostream& operator<< <3>(std::ostream& strm, const mshadow::Shape<3>& a);
template std::ostream& operator<< <4>(std::ostream& strm, const mshadow::Shape<4>& a);
template std::ostream& operator<< <5>(std::ostream& strm, const mshadow::Shape<5>& a);
template std::ostream& operator<< <6>(std::ostream& strm, const mshadow::Shape<6>& a);
template std::ostream& operator<< <7>(std::ostream& strm, const mshadow::Shape<7>& a);
template std::ostream& operator<< <8>(std::ostream& strm, const mshadow::Shape<8>& a);
template std::ostream& operator<< <9>(std::ostream& strm, const mshadow::Shape<9>& a);

void dali_init() {
    mshadow::InitTensorEngine<mshadow::cpu>();
    #ifdef DALI_USE_CUDA
        mshadow::InitTensorEngine<mshadow::gpu>();
    #endif
}


/**** SHOULD COMPUTE GPU-land **/

template<typename R>
bool should_compute_on_gpu(const std::vector<const SynchronizedMemory<R>*>& sts) {

#ifdef DALI_USE_CUDA
    if (sts.size() == 1) {
        auto mover = (sts.front());
        return (mover->prefers_gpu() && (mover->gpu_fresh || !mover->cpu_fresh && !mover->gpu_fresh));
    }
    bool everybody_cpu = true;
    bool everybody_gpu = true;
    for (auto st : sts) {
        everybody_gpu = everybody_gpu && st->prefers_gpu();
        everybody_cpu = everybody_cpu && st->prefers_cpu();
    }
    if (everybody_cpu) {
        return false;
    } else if (everybody_gpu) {
        return true;
    } else {
        return SynchronizedMemory<R>::tie_breaker_device == DEVICE_GPU;
    }
#else
    return false;
#endif
}

template bool should_compute_on_gpu(const std::vector<const SynchronizedMemory<float>*>& sts);
template bool should_compute_on_gpu(const std::vector<const SynchronizedMemory<double>*>& sts);
template bool should_compute_on_gpu(const std::vector<const SynchronizedMemory<int>*>& sts);

/******************* SYNCHRONIZED MEMORY ************************************************/

template<typename R>
bool SynchronizedMemory<R>::prefers_cpu() const {
    return preferred_device == DEVICE_CPU;
}

template<typename R>
bool SynchronizedMemory<R>::prefers_gpu()  const {
    return preferred_device == DEVICE_GPU;
}

#ifdef DALI_USE_CUDA
    template<typename R>
    Device SynchronizedMemory<R>::tie_breaker_device = DEVICE_GPU;
#endif

template<typename R>
SynchronizedMemory<R>::SynchronizedMemory(int _total_memory,
                                          int _inner_dimension,
                                          Device _preferred_device,
                                          bool _clear_on_allocation) :
#ifdef DALI_USE_CUDA
        gpu_fresh(false),
        allocated_gpu(false),
        gpu_ptr(NULL),
#endif
        clear_on_allocation(_clear_on_allocation),
        cpu_fresh(false),
        allocated_cpu(false),
        cpu_ptr(NULL),
        total_memory(_total_memory),
        inner_dimension(_inner_dimension),
        preferred_device(_preferred_device) {
    assert(total_memory % inner_dimension == 0);
}

template<typename R>
SynchronizedMemory<R>::SynchronizedMemory(const SynchronizedMemory& other) :
        SynchronizedMemory(other.total_memory, other.inner_dimension, other.preferred_device, other.clear_on_allocation) {
    if (other.cpu_fresh) {
        const auto& data_source = other.dummy_cpu();
        copy_data_from(data_source);
    }
#ifdef DALI_USE_CUDA
    else if (other.gpu_fresh) {
        const auto& data_source = other.dummy_gpu();
        copy_data_from(data_source);
    }
#endif
    else {
        // data was not initialized on the source
        // so we also choose not to initialize.
        return;
    }
}

template<typename R>
void SynchronizedMemory<R>::free_cpu() const {
    if (allocated_cpu) {
        memory_bank<R>::deposit_cpu(total_memory, inner_dimension, cpu_ptr);
        //auto dummy = dummy_cpu();
        //FreeSpace(&dummy);
        cpu_ptr = NULL;
    }
    allocated_cpu = false;
}

#ifdef DALI_USE_CUDA
template<typename R>
void SynchronizedMemory<R>::free_gpu() const {
    if (allocated_gpu) {
        memory_bank<R>::deposit_gpu(total_memory, inner_dimension, gpu_ptr);
        //auto dummy = dummy_gpu();
        //FreeSpace(&dummy);
        gpu_ptr = NULL;
    }
    allocated_gpu = false;
}
#endif

template<typename R>
SynchronizedMemory<R>::~SynchronizedMemory() {
    free_cpu();
#ifdef DALI_USE_CUDA
    free_gpu();
#endif
}

#ifdef DALI_USE_CUDA
template<typename R>
mshadow::Tensor<mshadow::gpu, 2, R> SynchronizedMemory<R>::dummy_gpu() const {
    return mshadow::Tensor<mshadow::gpu, 2, R>(gpu_ptr, mshadow::Shape2(
            total_memory / inner_dimension, inner_dimension));
}
#endif

template<typename R>
mshadow::Tensor<mshadow::cpu, 2, R> SynchronizedMemory<R>::dummy_cpu() const {
     return mshadow::Tensor<mshadow::cpu, 2, R>(cpu_ptr, mshadow::Shape2(
            total_memory / inner_dimension, inner_dimension));
}

template<typename R>
void SynchronizedMemory<R>::clear() {
    clear_on_allocation = true;
    #ifdef DALI_USE_CUDA
    if (preferred_device == DEVICE_GPU) {
        allocate_gpu();
        dummy_gpu() = (R)0.0;
        this->cpu_fresh = false;
        this->gpu_fresh = true;
        return;
    }
    #endif
    if (preferred_device == DEVICE_CPU) {
        allocate_cpu();
        dummy_cpu() = (R)0.0;
        this->cpu_fresh = true;
        #ifdef DALI_USE_CUDA
            this->gpu_fresh = false;
        #endif
    }
}

template<typename R>
void SynchronizedMemory<R>::lazy_clear() {
    clear_on_allocation = true;
    #ifdef DALI_USE_CUDA
        if (!allocated_cpu && !allocated_cpu) {
            return;
        }
    #else
        if (!allocated_cpu) {
            return;
        }
    #endif
    clear();
}

#ifdef DALI_USE_CUDA
    template<typename R>
    bool SynchronizedMemory<R>::allocate_gpu() const {
        if (allocated_gpu) {
            return false;
        }
        //auto dummy = dummy_gpu();
        //AllocSpace(&dummy, false);
        gpu_ptr = memory_bank<R>::allocate_gpu( total_memory , inner_dimension );
        allocated_gpu = true;
        return true;
    }

    template<typename R>
    void SynchronizedMemory<R>::to_gpu() const {
        if (!this->gpu_fresh) {
            auto just_allocated_gpu = allocate_gpu();
            // now that memory was freshly allocated
            // on gpu we either copy the CPU data over
            // or clear the buffer if the `clear_on_allocation`
            // flag is true:
            if (this->cpu_fresh) {
                auto mem_gpu = dummy_gpu();
                auto mem_cpu = dummy_cpu();
                Copy(mem_gpu, mem_cpu);
            } else if (just_allocated_gpu && clear_on_allocation) {
                dummy_gpu() = (R)0.0;
            }
            this->gpu_fresh = true;
        }
    }
#endif

template<typename R>
bool SynchronizedMemory<R>::allocate_cpu() const {
    if (allocated_cpu) {
        return false;
    }
    //auto dummy = dummy_cpu();
    //AllocSpace(&dummy, false);
    cpu_ptr = memory_bank<R>::allocate_cpu( total_memory , inner_dimension );
    allocated_cpu = true;
    return true;
}

template<typename R>
void SynchronizedMemory<R>::to_cpu() const {
    if (!this->cpu_fresh) {
        auto just_allocated_cpu = allocate_cpu();
#ifdef DALI_USE_CUDA
        if (this->gpu_fresh) {
            auto mem_gpu = dummy_gpu();
            auto mem_cpu = dummy_cpu();
            Copy(mem_cpu, mem_gpu);
        } else if (just_allocated_cpu && clear_on_allocation) {
            dummy_cpu() = (R)0.0;
        }
#else
        if (just_allocated_cpu && clear_on_allocation) {
            dummy_cpu() = (R)0.0;
        }
#endif
        this->cpu_fresh = true;
    }
}

template <typename R>
R* SynchronizedMemory<R>::cpu_data() const {
    to_cpu();
    return cpu_ptr;
}
template <typename R>
R* SynchronizedMemory<R>::mutable_cpu_data() {
    to_cpu();
    #ifdef DALI_USE_CUDA
        gpu_fresh = false;
    #endif
    return cpu_ptr;
}

#ifdef DALI_USE_CUDA
template <typename R>
R* SynchronizedMemory<R>::gpu_data() const {
    to_gpu();
    return gpu_ptr;
}
template <typename R>
R* SynchronizedMemory<R>::mutable_gpu_data() {
    to_gpu();
    cpu_fresh = false;
    return gpu_ptr;
}

#endif

template<typename R>
template<typename SourceType>
void SynchronizedMemory<R>::copy_data_from(SourceType& data_source) {
    if (this->prefers_cpu()) {
        allocate_cpu();
        Copy(dummy_cpu(), data_source);
        this->cpu_fresh = true;
    } else {
#ifdef DALI_USE_CUDA
        allocate_gpu();
        Copy(dummy_gpu(), data_source);
        this->gpu_fresh = true;
#endif
    }
}

template class SynchronizedMemory<float>;
template class SynchronizedMemory<int>;
template class SynchronizedMemory<double>;
