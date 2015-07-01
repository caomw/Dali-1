#include "dali/math/TensorInternal.h"

#include "dali/math/TensorOps.h"
#include "dali/math/LazyTensor.h"
#include "dali/utils/core_utils.h"

using std::vector;

template<typename R, int dimension>
TensorInternal<R,dimension>::TensorInternal(mshadow::Shape<dimension> _shape) :
        shape(_shape),
        offset(0) {
    // we treat the special case of empty matrix
    // as uninitalized memory:
    memory_ = std::make_shared<SynchronizedMemory<R>>(shape.Size(), shape[dimension - 1]);
}

template<typename R, int dimension>
TensorInternal<R,dimension>::TensorInternal(const TensorInternal& other, bool copy_memory) :
        TensorInternal(other.shape, other.memory_, other.offset) {
    if (copy_memory) {
        memory_ = std::make_shared<SynchronizedMemory<R>>(*other.memory_);
    }
}

template<typename R, int dimension>
const SynchronizedMemory<R>& TensorInternal<R, dimension>::memory() const {
    return *memory_;
}

template<typename R, int dimension>
SynchronizedMemory<R>& TensorInternal<R, dimension>::memory() {
    return *memory_;
}

template<typename R, int dimension>
TensorInternal<R,dimension>::TensorInternal(mshadow::Shape<dimension> _shape,
                                            std::shared_ptr<SynchronizedMemory<R>> _memory,
                                            int _offset) :
        shape(_shape),
        memory_(_memory),
        offset(_offset) {
}

template<typename R, int dimension>
R TensorInternal<R, dimension>::sum() const {
    #ifdef DALI_USE_CUDA
        if (compute_me_on_gpu()) {
            return TensorOps::sum(this->gpu_data(), this->number_of_elements() );
        }
    #endif

    return TensorOps::sum(this->cpu_data(), this->number_of_elements() );
}

template<typename R, int dimension>
std::vector<int> TensorInternal<R, dimension>::argmin(int reduce_dim) const {
    // reduce colwise
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmin(this->gpu_data(), reduce_dim);
    }
    #endif

    return TensorOps::arg::argmin(this->cpu_data(), reduce_dim);
}

template<typename R, int dimension>
std::vector<int> TensorInternal<R, dimension>::argsort() const {
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argsort(this->gpu_data(), this->number_of_elements());
    }
    #endif

    return TensorOps::arg::argsort(this->cpu_data(), this->number_of_elements());
}

template<typename R, int dimension>
std::vector<int> TensorInternal<R, dimension>::argmax(int reduce_dim) const {
    // reduce colwise
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmax(this->gpu_data(), reduce_dim);
    }
    #endif

    return TensorOps::arg::argmax(this->cpu_data(), reduce_dim);
}

template<typename R, int dimension>
int TensorInternal<R, dimension>::argmin() const {
    // reduce colwise
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmin( TensorOps::to_thrust(this->gpu_data()), this->number_of_elements() )[0];
    }
    #endif

    return TensorOps::arg::argmin(this->cpu_data().dptr_, this->number_of_elements() )[0];
}

template<typename R, int dimension>
int TensorInternal<R, dimension>::argmax() const {
    // reduce colwise
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmax( TensorOps::to_thrust(this->gpu_data()), this->number_of_elements() )[0];
    }
    #endif

    return TensorOps::arg::argmax(this->cpu_data().dptr_, this->number_of_elements() )[0];
}

template<typename R, int dimension>
int TensorInternal<R, dimension>::argmax_slice(int lower, int upper) const {
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmax( TensorOps::to_thrust(this->gpu_data()) + lower, upper - lower )[0];
    }
    #endif

    return TensorOps::arg::argmax(this->cpu_data().dptr_ + lower, upper - lower)[0];
}
template<typename R, int dimension>
int TensorInternal<R, dimension>::argmin_slice(int lower, int upper) const {
    #ifdef DALI_USE_CUDA
    if (compute_me_on_gpu()) {
        return TensorOps::arg::argmin( TensorOps::to_thrust(this->gpu_data()) + lower, upper - lower )[0];
    }
    #endif

    return TensorOps::arg::argmin(this->cpu_data().dptr_ + lower, upper - lower)[0];
}

template<typename R, int dimension>
R TensorInternal<R, dimension>::L2_norm() const {
    #ifdef DALI_USE_CUDA
        if (compute_me_on_gpu()) {
            return TensorOps::L2_norm(this->gpu_data(), this->number_of_elements() );
        }
    #endif

    return TensorOps::L2_norm(this->cpu_data(), this->number_of_elements() );
}

template<typename R, int dimension>
bool TensorInternal<R,dimension>::operator==(const TensorInternal<R,dimension>& other) const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu(vector<const SynchronizedMemory<R>*>({memory_.get(), other.memory_.get()}))) {
            return TensorOps::comparison::equals(this->gpu_data(), other.gpu_data(), this->number_of_elements());
        }
    #endif
    return TensorOps::comparison::equals(this->cpu_data(), other.cpu_data(), this->number_of_elements());
}

template<typename R, int dimension>
bool TensorInternal<R,dimension>::allclose(const TensorInternal<R,dimension>& other, R tol) const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu(vector<const SynchronizedMemory<R>*>({memory_.get(), other.memory_.get()}))) {
            return TensorOps::comparison::allclose(this->gpu_data(), other.gpu_data(), this->number_of_elements(), tol);
        }
    #endif
    return TensorOps::comparison::allclose(this->cpu_data(), other.cpu_data(), this->number_of_elements(), tol);
}

template<typename R, int dimension>
typename TensorInternal<R, dimension>::lazy_t TensorInternal<R,dimension>::wrapper() const {
    return lazy_t(*this);
}

template<typename R, int dimension>
TensorInternal<R,dimension>::operator lazy_t() const {
    return wrapper();
}

template<typename R, int dimension>
bool TensorInternal<R, dimension>::compute_me_on_gpu() const {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu(vector<const SynchronizedMemory<R>*>({memory_.get()}))) {
            return true;
        }
    #endif
    return false;
}


template<typename R, int dimension>
R& TensorInternal<R,dimension>::operator()(int i, int j) {
    int offset = this->cpu_data().stride_  * i + j;
    return *(this->mutable_cpu_data().dptr_ + offset);
}

template<typename R, int dimension>
R TensorInternal<R,dimension>::operator()(int i, int j) const {
    // this is wrong for dimension > 2 or == 1
    int offset = this->cpu_data().stride_  * i + j;
    return *(this->cpu_data().dptr_ + offset);
}

template<typename R, int dimension>
R& TensorInternal<R,dimension>::operator()(int i) {
    return *(this->mutable_cpu_data().dptr_ + i);
}

template<typename R, int dimension>
R TensorInternal<R,dimension>::operator()(int i) const {
    return *(this->cpu_data().dptr_ + i);
}

template<typename R, int dimension>
const R* TensorInternal<R,dimension>::data() const {
    return this->cpu_data().dptr_;
}

template<typename R, int dimension>
R* TensorInternal<R,dimension>::data() {
    return this->mutable_cpu_data().dptr_;
}

template <> void TensorInternal<float, 1>::print(int indent) const {
    std::cout << std::string(indent, ' ');
    std::cout << "[";
    for(int i=0; i<shape[0]; ++i) {
        std::cout << std::fixed
                  << std::setw( 7 ) // keep 7 digits
                  << std::setprecision( 3 ) // use 3 decimals
                  << std::setfill( ' ' ) << (*this)(i); // pad values with blanks this->w(i,j)
        if (i != shape[0] - 1) std::cout << " ";
    }
    std::cout << "]";
    std::cout << std::endl;
}
template <> void TensorInternal<double, 1>::print(int indent) const {
    std::cout << std::string(indent, ' ');
    std::cout << "[";
    for(int i=0; i<shape[0]; ++i) {
        std::cout << std::fixed
                  << std::setw( 7 ) // keep 7 digits
                  << std::setprecision( 3 ) // use 3 decimals
                  << std::setfill( ' ' ) << (*this)(i); // pad values with blanks this->w(i,j)
        if (i != shape[0] - 1) std::cout << " ";
    }
    std::cout << "]";
    std::cout << std::endl;
}


template<typename R, int dimension>
void TensorInternal<R,dimension>::print(int indent) const {
    static_assert (dimension > 1, "Print called with wrong dimension.");
    std::cout << std::string(indent, ' ') << "[" << std::endl;
    for (int i=0; i < shape[0]; ++i)
        (*this)[i].print(indent + 4);
    std::cout << std::string(indent, ' ') <<"]" << std::endl;
}

template<typename R, int dimension>
void TensorInternal<R,dimension>::clear() {
    *this = (R) 0.0;
}

template<typename R, int dimension>
TensorInternal<R,dimension> TensorInternal<R, dimension>::zeros(mshadow::Shape<dimension> shape) {
    auto tensor = TensorInternal<R,dimension>(shape);
    tensor.clear();
    return tensor;
}

template<typename R, int dimension>
const typename TensorInternal<R,dimension>::cpu_tensor_t TensorInternal<R,dimension>::cpu_data() const {
    return cpu_tensor_t(memory_->cpu_data() + offset, shape);
}

template<typename R, int dimension>
typename TensorInternal<R,dimension>::cpu_tensor_t TensorInternal<R,dimension>::mutable_cpu_data() {
    return cpu_tensor_t(memory_->mutable_cpu_data() + offset, shape);
}

#ifdef DALI_USE_CUDA
    template<typename R, int dimension>
    const typename TensorInternal<R,dimension>::gpu_tensor_t TensorInternal<R,dimension>::gpu_data() const {
        return gpu_tensor_t(memory_->gpu_data() + offset, shape);
    }

    template<typename R, int dimension>
    typename TensorInternal<R,dimension>::gpu_tensor_t TensorInternal<R,dimension>::mutable_gpu_data() {
        return gpu_tensor_t(memory_->mutable_gpu_data() + offset, shape);
    }
#endif

template<typename R, int dimension>
int TensorInternal<R,dimension>::number_of_elements() const {
    return shape.Size();
}

template<typename R, int dimension>
TensorInternal<R, dimension - 1> TensorInternal<R,dimension>::operator[](mshadow::index_t idx) const {
    auto subshape = shape.SubShape();
    return TensorInternal<R, dimension - 1>(subshape,
                                            memory_,
                                            offset + subshape.Size() * idx);
}

template<typename R, int dimension>
TensorInternal<R, 1> TensorInternal<R,dimension>::ravel() const {
    auto newshape = mshadow::Shape1(number_of_elements());
    return TensorInternal<R, 1>(newshape, memory_, offset);
}

template<typename R, int dimension>
TensorInternal<R, dimension> TensorInternal<R,dimension>::Slice(
        mshadow::index_t begin, mshadow::index_t end) const {
    auto newshape = shape;
    newshape[0] = end - begin;
    return TensorInternal<R, dimension>(newshape,
                                        memory_,
                                        offset + shape.SubShape().Size() * begin);
}

template<typename R, int dimension>
TensorInternal<R,dimension>& TensorInternal<R,dimension>::operator=(const lazy_t& expr) {
    #ifdef DALI_USE_CUDA
        if (should_compute_on_gpu(extract_memory(expr.dependent_tensors))) {
            /* refresh the gpu memory from cpu*/
            for (auto participant : expr.dependent_tensors) {
                participant->update_tensor(DEVICE_GPU);
            }
            mshadow::Copy(this->mutable_gpu_data(), expr.right)
            return;
        }
    #endif
    for (auto participant : expr.dependent_tensors) {
        participant->update_tensor(DEVICE_CPU);
    }
    mshadow::Copy(this->mutable_cpu_data(), expr.left);
    return *this;
}


template class TensorInternal<float, 1>;
template class TensorInternal<double,1>;
template class TensorInternal<float, 2>;
template class TensorInternal<double,2>;
// template class TensorInternal<float, 3>;
// template class TensorInternal<double,3>;
// template class TensorInternal<float, 4>;
// template class TensorInternal<double,4>;
// template class TensorInternal<float, 5>;
// template class TensorInternal<double,5>;
// template class TensorInternal<float, 6>;
// template class TensorInternal<double,6>;
// template class TensorInternal<float, 7>;
// template class TensorInternal<double,7>;
// template class TensorInternal<float, 8>;
// template class TensorInternal<double,8>;
// template class TensorInternal<float, 9>;
// template class TensorInternal<double,9>;
