/*******************************************************************************
* Copyright 2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef CPU_JIT_UNI_DEPTHWISE_HPP
#define CPU_JIT_UNI_DEPTHWISE_HPP

#include <assert.h>

#include "c_types_map.hpp"
#include "cpu_depthwise_pd.hpp"
#include "cpu_engine.hpp"
#include "type_helpers.hpp"
#include "utils.hpp"
#include "jit_primitive_conf.hpp"
#include "jit_uni_eltwise.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

struct jit_uni_depthwise_kernel_f32;

template <cpu_isa_t isa>
struct jit_uni_depthwise_fwd_t : public cpu_primitive_t {
    struct pd_t : public cpu_depthwise_fwd_pd_t {
        pd_t(engine_t *engine, const depthwise_desc_t *adesc,
                const primitive_attr_t *attr,
                const depthwise_fwd_pd_t *hint_fwd_pd)
            : cpu_depthwise_fwd_pd_t(engine, adesc, attr, hint_fwd_pd) {}

        DECLARE_COMMON_PD_T(
                JIT_IMPL_NAME_HELPER("jit:", isa, ""),
                jit_uni_depthwise_fwd_t<isa>);

        virtual status_t init() override;
    };

    jit_uni_depthwise_fwd_t(const pd_t *pd, const input_vector &inputs,
                       const output_vector &outputs);
    ~jit_uni_depthwise_fwd_t();

    typedef typename prec_traits<data_type::f32>::type data_t;

    virtual void execute(event_t *e)
    {
        execute_forward();
        e->set_state(event_t::ready);
    }

private:
    void execute_forward();
    pd_t conf_;
    jit_uni_depthwise_kernel_f32 *kernel_;
};


template <cpu_isa_t isa>
struct jit_uni_dw_conv_row_f32: public jit_generator {
    DECLARE_CPU_JIT_AUX_FUNCTIONS(jit_uni_ds_dw_conv_kernel_f32)

    jit_uni_dw_conv_row_f32(jit_conv_conf_t ajcp): jcp(ajcp) {
        this->generate();
        jit_ker = (void (*)(jit_conv_call_s *))this->getCode();
    }

    static bool post_ops_ok(jit_conv_conf_t &jcp,
            const primitive_attr_t &attr);
    static status_t init_conf(jit_conv_conf_t &jcp,
            int ic, int ih, int iw, int oh, int ow,
            int ker_h, int ker_w, int str_h, int str_w,
            alg_kind_t eltwise_alg,
            float eltwise_alpha, float eltwise_beta, bool with_sum);

    jit_conv_conf_t jcp;
    void (*jit_ker)(jit_conv_call_s *);

private:
    using Vmm = typename utils::conditional3<isa == sse42, Xbyak::Xmm,
        isa == avx2, Xbyak::Ymm, Xbyak::Zmm>::type;
    using reg64_t = const Xbyak::Reg64;
    const Xbyak::AddressFrame &vmmword = (isa == sse42)
        ? xword : (isa == avx2) ? yword : zword;
    const int vlen = cpu_isa_traits<isa>::vlen;

    // dw convolution
    reg64_t reg_input0 = r8;
    reg64_t reg_input1 = r9;
    reg64_t reg_input2 = r10;
    reg64_t aux_reg_input0 = r11;
    reg64_t aux_reg_input1 = r12;
    reg64_t aux_reg_input2 = r13;


    reg64_t reg_kernel = r14;
    reg64_t aux_reg_kernel = r15;
    reg64_t reg_output = rdx;
    reg64_t reg_bias = rbx;
    reg64_t reg_kh = rax;
    reg64_t reg_ur_w = rbp;

    reg64_t imm_addr64 = aux_reg_input0;

    inline Vmm get_ker_reg(int idx) { return Vmm(idx + 0); }
    inline Vmm get_src_reg(int idx) { return Vmm(idx + 1); }
    inline Vmm get_acc_reg(int idx) { return Vmm(idx + 4); }

    inline void load_src(int ur_w);
    inline void apply_filter(int ur_w, int kw_size);
    inline void apply_activation(int ur_w);
    inline void store_dst(int ur_w);
    inline void loop_body();

    void generate();

    Xbyak::Label l_table;
    jit_uni_eltwise_vector_f32<isa> eltwise_generator;
};

}
}
}

#endif
