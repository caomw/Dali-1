#ifndef DALI_ARRAY_FUNCTION_ARGS_MSHADOW_WRAPPER_H
#define DALI_ARRAY_FUNCTION_ARGS_MSHADOW_WRAPPER_H

template<int devT, typename T>
struct TypedArray;

#include "dali/array/function/typed_array.h"
#include "dali/array/memory/device.h"
#include "dali/config.h"
#include "dali/utils/assert2.h"

////////////////////////////////////////////////////////////////////////////////
//                           MSHADOW_WRAPPER_EXP                              //
//                                   ---                                      //
//  This expression is used to inject Dali striding information to mshadow    //
//  expression processor                                                      //
//  TODO(szymon): make this code more efficient:                              //
//     -> On CPU code is evaluated serially, so we can replace modulos with   //
//        if statements => MAD_EFFICIENT (also omp parallel?)                 //
//     -> On GPU we need to make sure that strides and shapes are in the      //
//        lowest level of cache possible. I think __shared__ needs to be used //
//        but injecting this into mshadow might cause insanity                //
////////////////////////////////////////////////////////////////////////////////


template<typename Device, int srcdim, typename DType>
struct DaliWrapperExp : public mshadow::TRValue<
            DaliWrapperExp<Device, srcdim, DType>,
            Device,
            srcdim,
            DType
        > {
    typedef mshadow::Tensor<Device, srcdim, DType> src_t;
    const src_t src_;
    mshadow::Shape<srcdim> shape_;
    const Array array;

    DaliWrapperExp(const src_t& src, const Array& dali_src) :
            src_(src),
            array(dali_src) {
        ASSERT2(src_.shape_[srcdim - 1] == src_.stride_,
                "DaliWrapperExp should never reach that condition (only tensors should be passed as arguments).");
        ASSERT2(array.shape().size() <= DALI_MAX_STRIDED_DIMENSION,
                "Striding only supported for Tensors up to DALI_MAX_STRIDED_DIMENSION dimensions.");
        this->shape_ = mshadow::expr::ShapeCheck<srcdim, src_t>::Check(src_);
    }

    template<typename E, int etype>
    inline DaliWrapperExp<Device, srcdim, DType>&
    operator=(const mshadow::expr::Exp<E, DType, etype>& exp) {
        return this->__assign(exp);
    }

    inline DaliWrapperExp<Device, srcdim, DType>&
    operator=(const DType& exp) {
        return this->__assign(exp);
    }

};

template<typename Device, int srcdim, typename DType>
inline DaliWrapperExp<Device, srcdim, DType>
MakeDaliWrapperExp(const mshadow::Tensor<Device, srcdim, DType> &src, const Array& dali_src) {
    return DaliWrapperExp<Device, srcdim, DType>(src.self(), dali_src);
}

namespace mshadow {
    namespace expr {

        template<typename Device, int srcdim, typename DType>
        struct ExpInfo<DaliWrapperExp<Device, srcdim, DType>> {
            static const int kDimSrc = ExpInfo<typename DaliWrapperExp<Device, srcdim, DType>::src_t>::kDim;
            static const int kDim = kDimSrc >= 0 ? srcdim : -1;
            static const int kDevMask = ExpInfo<typename DaliWrapperExp<Device, srcdim, DType>::src_t>::kDevMask;
        };

        template<typename Device, int srcdim, typename DType>
        struct ShapeCheck<srcdim, DaliWrapperExp<Device,srcdim,DType>> {
            inline static Shape<srcdim>
            Check(const DaliWrapperExp<Device, srcdim, DType> &t) {
                return t.shape_;
            }
        };

        template<typename Device, int srcdim, typename DType>
        struct Plan<DaliWrapperExp<Device, srcdim, DType>, DType> {
          public:
            explicit Plan(const DaliWrapperExp<Device, srcdim, DType> &e) :
                    src_(MakePlan(e.src_)),
                    ndim(e.array.shape().size()),
                    has_strides(!e.array.strides().empty()) {

                for (int i = 0; i < ndim; ++i) {
                    shape[i] = e.array.shape()[i];
                    if (has_strides) strides[i] = e.array.strides()[i];
                }
            }

            MSHADOW_XINLINE void map_indices_using_stride(index_t& new_i, index_t& new_j, index_t i, index_t j) const {
                index_t i_derived_offset = 0;

                for (int dim_idx = ndim - 2; dim_idx >= 0; --dim_idx) {
                    i_derived_offset += (i % shape[dim_idx]) * strides[dim_idx];
                    i /=  shape[dim_idx];
                }
                new_i = i_derived_offset / shape[ndim - 1];
                new_j = i_derived_offset % shape[ndim - 1] + j * strides[ndim - 1];
            }

            MSHADOW_XINLINE DType& REval(index_t i, index_t j) {
                if (!has_strides) {
                    return src_.REval(i, j);
                } else {
                    index_t new_i, new_j;
                    map_indices_using_stride(new_i, new_j, i, j);
                    return src_.REval(new_i, new_j);
                }
            }

            MSHADOW_XINLINE const DType& Eval(index_t i, index_t j) const {
                if (!has_strides) {
                    return src_.Eval(i, j);
                } else {
                    index_t new_i, new_j;
                    map_indices_using_stride(new_i, new_j, i, j);
                    return src_.Eval(new_i, new_j);
                }
            }

          private:
            Plan<typename DaliWrapperExp<Device, srcdim, DType>::src_t, DType> src_;
            int ndim;
            int shape[DALI_MAX_STRIDED_DIMENSION];
            int strides[DALI_MAX_STRIDED_DIMENSION];
            const bool has_strides;
        };

        template<typename SV, typename Device, typename DType,
                 typename SrcExp, typename Reducer, int m_dimkeep>
        struct ExpComplexEngine<SV,
                                DaliWrapperExp<Device, 1, DType>,
                                ReduceTo1DExp<SrcExp, DType, Reducer, m_dimkeep>,
                                DType> {
            static const int dimkeep = ExpInfo<SrcExp>::kDim - m_dimkeep;
            inline static void Eval(DaliWrapperExp<Device, 1, DType> *dst,
                                    const ReduceTo1DExp<SrcExp, DType,
                                                        Reducer, m_dimkeep> &exp) {
                TypeCheckPass<m_dimkeep != 1>
                        ::Error_Expression_Does_Not_Meet_Dimension_Req();
                MapReduceKeepHighDim<SV, Reducer, dimkeep>(dst, exp.src_, exp.scale_);
            }
        };
        template<typename SV, typename Device, typename DType,
                 typename SrcExp, typename Reducer>
        struct ExpComplexEngine<SV,
                                DaliWrapperExp<Device, 1, DType>,
                                ReduceTo1DExp<SrcExp, DType, Reducer, 1>, DType> {
            inline static void Eval(DaliWrapperExp<Device, 1, DType> *dst,
                                    const ReduceTo1DExp<SrcExp, DType, Reducer, 1> &exp) {
                MapReduceKeepLowest<SV, Reducer>(dst, exp.src_, exp.scale_);
            }
        };
    } //namespace expr
} // namespace mshadow
////////////////////////////////////////////////////////////////////////////////
//                             MSHADOW_WRAPPER                                //
//                                   ---                                      //
//  This class would not be needed at all if we defined to_mshadow_expr       //
//  function on Array. The reason not to do that is to hide all mshadow usage //
//  in cpp files whereever possible.                                          //
////////////////////////////////////////////////////////////////////////////////


template<int devT,typename T, typename ExprT>
struct MshadowWrapper {
    static inline auto wrap(const ExprT& sth, memory::Device device) ->
            decltype(sth.template to_mshadow_expr<devT,T>(device)) {
        return sth.template to_mshadow_expr<devT,T>(device);
    }
};

template<int devT,typename T>
struct MshadowWrapper<devT,T,Array> {
    static inline auto wrap(const Array& array, memory::Device device) ->
            decltype(TypedArray<devT,T>(array, device).d2()) {
        return TypedArray<devT,T>(array, device).d2();
    }
};

template<int devT,typename T>
struct MshadowWrapper<devT,T,float> {
    static inline T wrap(const float& scalar, memory::Device device) { return (T)scalar; }
};

template<int devT,typename T>
struct MshadowWrapper<devT,T,double> {
    static inline T wrap(const double& scalar, memory::Device device) { return (T)scalar; }
};

template<int devT,typename T>
struct MshadowWrapper<devT,T,int> {
    static inline T wrap(const int& scalar, memory::Device device) { return (T)scalar; }
};

#endif // DALI_ARRAY_FUNCTION_ARGS_MSHADOW_WRAPPER_H
