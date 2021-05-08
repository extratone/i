/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef X86Assembler_h
#define X86Assembler_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER) && (PLATFORM(X86) || PLATFORM(X86_64))

#include "AssemblerBuffer.h"
#include <stdint.h>
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace JSC {

inline bool CAN_SIGN_EXTEND_8_32(int32_t value) { return value == (int32_t)(signed char)value; }
#if PLATFORM(X86_64)
inline bool CAN_SIGN_EXTEND_32_64(intptr_t value) { return value == (intptr_t)(int32_t)value; }
inline bool CAN_SIGN_EXTEND_U32_64(intptr_t value) { return value == (intptr_t)(uint32_t)value; }
#endif

namespace X86 {
    typedef enum {
        eax,
        ecx,
        edx,
        ebx,
        esp,
        ebp,
        esi,
        edi,

#if PLATFORM(X86_64)
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15,
#endif
    } RegisterID;

    typedef enum {
        xmm0,
        xmm1,
        xmm2,
        xmm3,
        xmm4,
        xmm5,
        xmm6,
        xmm7,
    } XMMRegisterID;
}

class X86Assembler {
public:
    typedef X86::RegisterID RegisterID;
    typedef X86::XMMRegisterID XMMRegisterID;

    typedef enum {
        OP_ADD_EvGv                     = 0x01,
        OP_ADD_GvEv                     = 0x03,
        OP_OR_EvGv                      = 0x09,
        OP_OR_GvEv                      = 0x0B,
        OP_2BYTE_ESCAPE                 = 0x0F,
        OP_AND_EvGv                     = 0x21,
        OP_SUB_EvGv                     = 0x29,
        OP_SUB_GvEv                     = 0x2B,
        PRE_PREDICT_BRANCH_NOT_TAKEN    = 0x2E,
        OP_XOR_EvGv                     = 0x31,
        OP_CMP_EvGv                     = 0x39,
        OP_CMP_GvEv                     = 0x3B,
#if PLATFORM(X86_64)
        PRE_REX                         = 0x40,
#endif
        OP_PUSH_EAX                     = 0x50,
        OP_POP_EAX                      = 0x58,
#if PLATFORM(X86_64)
        OP_MOVSXD_GvEv                  = 0x63,
#endif
        PRE_OPERAND_SIZE                = 0x66,
        PRE_SSE_66                      = 0x66,
        OP_PUSH_Iz                      = 0x68,
        OP_IMUL_GvEvIz                  = 0x69,
        OP_GROUP1_EvIz                  = 0x81,
        OP_GROUP1_EvIb                  = 0x83,
        OP_TEST_EvGv                    = 0x85,
        OP_XCHG_EvGv                    = 0x87,
        OP_MOV_EvGv                     = 0x89,
        OP_MOV_GvEv                     = 0x8B,
        OP_LEA                          = 0x8D,
        OP_GROUP1A_Ev                   = 0x8F,
        OP_CDQ                          = 0x99,
        OP_MOV_EAXOv                    = 0xA1,
        OP_MOV_OvEAX                    = 0xA3,
        OP_MOV_EAXIv                    = 0xB8,
        OP_GROUP2_EvIb                  = 0xC1,
        OP_RET                          = 0xC3,
        OP_GROUP11_EvIz                 = 0xC7,
        OP_INT3                         = 0xCC,
        OP_GROUP2_Ev1                   = 0xD1,
        OP_GROUP2_EvCL                  = 0xD3,
        OP_CALL_rel32                   = 0xE8,
        OP_JMP_rel32                    = 0xE9,
        PRE_SSE_F2                      = 0xF2,
        OP_HLT                          = 0xF4,
        OP_GROUP3_EbIb                  = 0xF6,
        OP_GROUP3_Ev                    = 0xF7,
        OP_GROUP3_EvIz                  = 0xF7, // OP_GROUP3_Ev has an immediate, when instruction is a test. 
        OP_GROUP5_Ev                    = 0xFF,
    } OneByteOpcodeID;

    typedef enum {
        OP2_MOVSD_VsdWsd    = 0x10,
        OP2_MOVSD_WsdVsd    = 0x11,
        OP2_CVTSI2SD_VsdEd  = 0x2A,
        OP2_CVTTSD2SI_GdWsd = 0x2C,
        OP2_UCOMISD_VsdWsd  = 0x2E,
        OP2_ADDSD_VsdWsd    = 0x58,
        OP2_MULSD_VsdWsd    = 0x59,
        OP2_SUBSD_VsdWsd    = 0x5C,
        OP2_MOVD_VdEd       = 0x6E,
        OP2_MOVD_EdVd       = 0x7E,
        OP2_JO_rel32        = 0x80,
        OP2_JB_rel32        = 0x82,
        OP2_JAE_rel32       = 0x83,
        OP2_JE_rel32        = 0x84,
        OP2_JNE_rel32       = 0x85,
        OP2_JBE_rel32       = 0x86,
        OP2_JA_rel32        = 0x87,
        OP2_JS_rel32        = 0x88,
        OP2_JP_rel32        = 0x8A,
        OP2_JL_rel32        = 0x8C,
        OP2_JGE_rel32       = 0x8D,
        OP2_JLE_rel32       = 0x8E,
        OP2_JG_rel32        = 0x8F,
        OP_SETE             = 0x94,
        OP_SETNE            = 0x95,
        OP2_IMUL_GvEv       = 0xAF,
        OP2_MOVZX_GvEb      = 0xB6,
        OP2_MOVZX_GvEw      = 0xB7,
        OP2_PEXTRW_GdUdIb   = 0xC5,
    } TwoByteOpcodeID;

    typedef enum {
        GROUP1_OP_ADD = 0,
        GROUP1_OP_OR  = 1,
        GROUP1_OP_AND = 4,
        GROUP1_OP_SUB = 5,
        GROUP1_OP_XOR = 6,
        GROUP1_OP_CMP = 7,

        GROUP1A_OP_POP = 0,

        GROUP2_OP_SHL = 4,
        GROUP2_OP_SAR = 7,

        GROUP3_OP_TEST = 0,
        GROUP3_OP_NOT  = 2,
        GROUP3_OP_IDIV = 7,

        GROUP5_OP_CALLN = 2,
        GROUP5_OP_JMPN  = 4,
        GROUP5_OP_PUSH  = 6,

        GROUP11_MOV = 0,
    } GroupOpcodeID;
    
    // Opaque label types
    
private:
    class X86InstructionFormatter;
public:

    class JmpSrc {
        friend class X86Assembler;
        friend class X86InstructionFormatter;
    public:
        JmpSrc()
            : m_offset(-1)
        {
        }

    private:
        JmpSrc(int offset)
            : m_offset(offset)
        {
        }

        int m_offset;
    };
    
    class JmpDst {
        friend class X86Assembler;
        friend class X86InstructionFormatter;
    public:
        JmpDst()
            : m_offset(-1)
        {
        }

    private:
        JmpDst(int offset)
            : m_offset(offset)
        {
        }

        int m_offset;
    };

    X86Assembler()
    {
    }

    size_t size() const { return m_formatter.size(); }

    // Stack operations:

    void push_r(RegisterID reg)
    {
        m_formatter.oneByteOp(OP_PUSH_EAX, reg);
    }

    void pop_r(RegisterID reg)
    {
        m_formatter.oneByteOp(OP_POP_EAX, reg);
    }

    void push_i32(int imm)
    {
        m_formatter.oneByteOp(OP_PUSH_Iz);
        m_formatter.immediate32(imm);
    }

    void push_m(int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_PUSH, base, offset);
    }

    void pop_m(int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP1A_Ev, GROUP1A_OP_POP, base, offset);
    }

    // Arithmetic operations:

    void addl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_ADD_EvGv, src, dst);
    }

    void addl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_ADD_GvEv, dst, base, offset);
    }

    void addl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, dst);
            m_formatter.immediate32(imm);
        }
    }

    void addl_im(int imm, int offset, RegisterID base)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if PLATFORM(X86_64)
    void addq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_ADD_EvGv, src, dst);
    }

    void addq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_ADD, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_ADD, dst);
            m_formatter.immediate32(imm);
        }
    }
#else
    void addl_im(int imm, void* addr)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void andl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_AND_EvGv, src, dst);
    }

    void andl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_AND, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_AND, dst);
            m_formatter.immediate32(imm);
        }
    }

#if PLATFORM(X86_64)
    void andq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_AND_EvGv, src, dst);
    }

    void andq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_AND, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_AND, dst);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void notl_r(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_NOT, dst);
    }

    void orl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_OR_EvGv, src, dst);
    }

    void orl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_OR_GvEv, dst, base, offset);
    }

    void orl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_OR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_OR, dst);
            m_formatter.immediate32(imm);
        }
    }

#if PLATFORM(X86_64)
    void orq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_OR_EvGv, src, dst);
    }

    void orq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_OR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_OR, dst);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void subl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_SUB_EvGv, src, dst);
    }

    void subl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_SUB_GvEv, dst, base, offset);
    }

    void subl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, dst);
            m_formatter.immediate32(imm);
        }
    }
    
    void subl_im(int imm, int offset, RegisterID base)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if PLATFORM(X86_64)
    void subq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_SUB_EvGv, src, dst);
    }

    void subq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_SUB, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_SUB, dst);
            m_formatter.immediate32(imm);
        }
    }
#else
    void subl_im(int imm, void* addr)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void xorl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_XOR_EvGv, src, dst);
    }

    void xorl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_XOR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_XOR, dst);
            m_formatter.immediate32(imm);
        }
    }

#if PLATFORM(X86_64)
    void xorq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_XOR_EvGv, src, dst);
    }

    void xorq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_XOR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_XOR, dst);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void sarl_i8r(int imm, RegisterID dst)
    {
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, GROUP2_OP_SAR, dst);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, GROUP2_OP_SAR, dst);
            m_formatter.immediate8(imm);
        }
    }

    void sarl_CLr(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP2_EvCL, GROUP2_OP_SAR, dst);
    }

    void shll_i8r(int imm, RegisterID dst)
    {
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, GROUP2_OP_SHL, dst);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, GROUP2_OP_SHL, dst);
            m_formatter.immediate8(imm);
        }
    }

    void shll_CLr(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP2_EvCL, GROUP2_OP_SHL, dst);
    }

#if PLATFORM(X86_64)
    void sarq_CLr(RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_GROUP2_EvCL, GROUP2_OP_SAR, dst);
    }

    void sarq_i8r(int imm, RegisterID dst)
    {
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, GROUP2_OP_SAR, dst);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, GROUP2_OP_SAR, dst);
            m_formatter.immediate8(imm);
        }
    }
#endif

    void imull_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.twoByteOp(OP2_IMUL_GvEv, dst, src);
    }
    
    void imull_i32r(RegisterID src, int32_t value, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_IMUL_GvEvIz, dst, src);
        m_formatter.immediate32(value);
    }

    void idivl_r(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_IDIV, dst);
    }

    // Comparisons:

    void cmpl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_CMP_EvGv, src, dst);
    }

    void cmpl_rm(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_CMP_EvGv, src, base, offset);
    }

    void cmpl_mr(int offset, RegisterID base, RegisterID src)
    {
        m_formatter.oneByteOp(OP_CMP_GvEv, src, base, offset);
    }

    void cmpl_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
            m_formatter.immediate32(imm);
        }
    }

    void cmpl_ir_force32(int imm, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
        m_formatter.immediate32(imm);
    }

    void cmpl_im(int imm, int offset, RegisterID base)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate32(imm);
        }
    }

    void cmpl_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate32(imm);
        }
    }

    void cmpl_im_force32(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
        m_formatter.immediate32(imm);
    }

#if PLATFORM(X86_64)
    void cmpq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_CMP_EvGv, src, dst);
    }

    void cmpq_rm(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp64(OP_CMP_EvGv, src, base, offset);
    }

    void cmpq_ir(int imm, RegisterID dst)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
            m_formatter.immediate32(imm);
        }
    }

    void cmpq_im(int imm, int offset, RegisterID base)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate32(imm);
        }
    }

    void cmpq_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate32(imm);
        }
    }
#else
    void cmpl_rm(RegisterID reg, void* addr)
    {
        m_formatter.oneByteOp(OP_CMP_EvGv, reg, addr);
    }

    void cmpl_im(int imm, void* addr)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void cmpw_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, src, base, index, scale, offset);
    }

    void testl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_TEST_EvGv, src, dst);
    }
    
    void testl_i32r(int imm, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, dst);
        m_formatter.immediate32(imm);
    }

    void testl_i32m(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, offset);
        m_formatter.immediate32(imm);
    }

    void testl_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }

#if PLATFORM(X86_64)
    void testq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_TEST_EvGv, src, dst);
    }

    void testq_i32r(int imm, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, dst);
        m_formatter.immediate32(imm);
    }

    void testq_i32m(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, offset);
        m_formatter.immediate32(imm);
    }

    void testq_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }
#endif 

    void testb_i8r(int imm, RegisterID dst)
    {
        m_formatter.oneByteOp8(OP_GROUP3_EbIb, GROUP3_OP_TEST, dst);
        m_formatter.immediate8(imm);
    }

    void sete_r(RegisterID dst)
    {
        m_formatter.twoByteOp8(OP_SETE, (GroupOpcodeID)0, dst);
    }

    void setz_r(RegisterID dst)
    {
        sete_r(dst);
    }

    void setne_r(RegisterID dst)
    {
        m_formatter.twoByteOp8(OP_SETNE, (GroupOpcodeID)0, dst);
    }

    void setnz_r(RegisterID dst)
    {
        setne_r(dst);
    }

    // Various move ops:

    void cdq()
    {
        m_formatter.oneByteOp(OP_CDQ);
    }

    void xchgl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_XCHG_EvGv, src, dst);
    }

#if PLATFORM(X86_64)
    void xchgq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_XCHG_EvGv, src, dst);
    }
#endif

    void movl_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_MOV_EvGv, src, dst);
    }
    
    void movl_rm(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, offset);
    }

    void movl_rm_disp32(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp_disp32(OP_MOV_EvGv, src, base, offset);
    }

    void movl_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, index, scale, offset);
    }
    
    void movl_mEAX(void* addr)
    {
        m_formatter.oneByteOp(OP_MOV_EAXOv);
#if PLATFORM(X86_64)
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int>(addr));
#endif
    }

    void movl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_MOV_GvEv, dst, base, offset);
    }

    void movl_mr_disp32(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp_disp32(OP_MOV_GvEv, dst, base, offset);
    }

    void movl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_MOV_GvEv, dst, base, index, scale, offset);
    }

    void movl_i32r(int imm, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_MOV_EAXIv, dst);
        m_formatter.immediate32(imm);
    }

    void movl_i32m(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, base, offset);
        m_formatter.immediate32(imm);
    }

    void movl_EAXm(void* addr)
    {
        m_formatter.oneByteOp(OP_MOV_OvEAX);
#if PLATFORM(X86_64)
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int>(addr));
#endif
    }

#if PLATFORM(X86_64)
    void movq_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, dst);
    }

    void movq_rm(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, base, offset);
    }

    void movq_rm_disp32(RegisterID src, int offset, RegisterID base)
    {
        m_formatter.oneByteOp64_disp32(OP_MOV_EvGv, src, base, offset);
    }

    void movq_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, base, index, scale, offset);
    }

    void movq_mEAX(void* addr)
    {
        m_formatter.oneByteOp64(OP_MOV_EAXOv);
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
    }

    void movq_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_MOV_GvEv, dst, base, offset);
    }

    void movq_mr_disp32(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp64_disp32(OP_MOV_GvEv, dst, base, offset);
    }

    void movq_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_MOV_GvEv, dst, base, index, scale, offset);
    }

    void movq_i64r(int64_t imm, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_MOV_EAXIv, dst);
        m_formatter.immediate64(imm);
    }
    
    void movsxd_rr(RegisterID src, RegisterID dst)
    {
        m_formatter.oneByteOp64(OP_MOVSXD_GvEv, dst, src);
    }
    
    
#else
    void movl_mr(void* addr, RegisterID dst)
    {
        if (dst == X86::eax)
            movl_mEAX(addr);
        else
            m_formatter.oneByteOp(OP_MOV_GvEv, dst, addr);
    }

    void movl_i32m(int imm, void* addr)
    {
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, addr);
        m_formatter.immediate32(imm);
    }
#endif

    void movzwl_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, dst, base, offset);
    }

    void movzwl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, dst, base, index, scale, offset);
    }

    void movzbl_rr(RegisterID src, RegisterID dst)
    {
        // In 64-bit, this may cause an unnecessary REX to be planted (if the dst register
        // is in the range ESP-EDI, and the src would not have required a REX).  Unneeded
        // REX prefixes are defined to be silently ignored by the processor.
        m_formatter.twoByteOp8(OP2_MOVZX_GvEb, dst, src);
    }

    void leal_mr(int offset, RegisterID base, RegisterID dst)
    {
        m_formatter.oneByteOp(OP_LEA, dst, base, offset);
    }

    // Flow control:

    JmpSrc call()
    {
        m_formatter.oneByteOp(OP_CALL_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc call(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_CALLN, dst);
        return JmpSrc(m_formatter.size());
    }

    JmpSrc jmp()
    {
        m_formatter.oneByteOp(OP_JMP_rel32);
        return m_formatter.immediateRel32();
    }
    
    void jmp_r(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_JMPN, dst);
    }
    
    void jmp_m(int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_JMPN, base, offset);
    }

    JmpSrc jne()
    {
        m_formatter.twoByteOp(OP2_JNE_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jnz()
    {
        return jne();
    }

    JmpSrc je()
    {
        m_formatter.twoByteOp(OP2_JE_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jl()
    {
        m_formatter.twoByteOp(OP2_JL_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jb()
    {
        m_formatter.twoByteOp(OP2_JB_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jle()
    {
        m_formatter.twoByteOp(OP2_JLE_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jbe()
    {
        m_formatter.twoByteOp(OP2_JBE_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jge()
    {
        m_formatter.twoByteOp(OP2_JGE_rel32);
        return m_formatter.immediateRel32();
    }

    JmpSrc jg()
    {
        m_formatter.twoByteOp(OP2_JG_rel32);
        return m_formatter.immediateRel32();
    }

    JmpSrc ja()
    {
        m_formatter.twoByteOp(OP2_JA_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jae()
    {
        m_formatter.twoByteOp(OP2_JAE_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jo()
    {
        m_formatter.twoByteOp(OP2_JO_rel32);
        return m_formatter.immediateRel32();
    }

    JmpSrc jp()
    {
        m_formatter.twoByteOp(OP2_JP_rel32);
        return m_formatter.immediateRel32();
    }
    
    JmpSrc js()
    {
        m_formatter.twoByteOp(OP2_JS_rel32);
        return m_formatter.immediateRel32();
    }

    // SSE operations:

    void addsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_ADDSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void addsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_ADDSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void cvtsi2sd_rr(RegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTSI2SD_VsdEd, (RegisterID)dst, src);
    }

    void cvttsd2si_rr(XMMRegisterID src, RegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTTSD2SI_GdWsd, dst, (RegisterID)src);
    }

    void movd_rr(XMMRegisterID src, RegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_MOVD_EdVd, (RegisterID)src, dst);
    }

#if PLATFORM(X86_64)
    void movq_rr(XMMRegisterID src, RegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp64(OP2_MOVD_EdVd, (RegisterID)src, dst);
    }

    void movq_rr(RegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp64(OP2_MOVD_VdEd, (RegisterID)dst, src);
    }
#endif

    void movsd_rm(XMMRegisterID src, int offset, RegisterID base)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_WsdVsd, (RegisterID)src, base, offset);
    }

    void movsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void mulsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MULSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void mulsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MULSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void pextrw_irr(int whichWord, XMMRegisterID src, RegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_PEXTRW_GdUdIb, (RegisterID)dst, (RegisterID)src);
        m_formatter.immediate8(whichWord);
    }

    void subsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_SUBSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void subsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_SUBSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void ucomisd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_UCOMISD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    // Misc instructions:

    void int3()
    {
        m_formatter.oneByteOp(OP_INT3);
    }
    
    void ret()
    {
        m_formatter.oneByteOp(OP_RET);
    }

    void predictNotTaken()
    {
        m_formatter.prefix(PRE_PREDICT_BRANCH_NOT_TAKEN);
    }

    // Assembler admin methods:

    JmpDst label()
    {
        return JmpDst(m_formatter.size());
    }
    
    JmpDst align(int alignment)
    {
        while (!m_formatter.isAligned(alignment))
            m_formatter.oneByteOp(OP_HLT);

        return label();
    }

    // Linking & patching:

    void link(JmpSrc from, JmpDst to)
    {
        ASSERT(to.m_offset != -1);
        ASSERT(from.m_offset != -1);
        
        reinterpret_cast<int*>(reinterpret_cast<ptrdiff_t>(m_formatter.data()) + from.m_offset)[-1] = to.m_offset - from.m_offset;
    }
    
    static void patchAddress(void* code, JmpDst position, void* value)
    {
        ASSERT(position.m_offset != -1);
        
        reinterpret_cast<void**>(reinterpret_cast<ptrdiff_t>(code) + position.m_offset)[-1] = value;
    }
    
    static void link(void* code, JmpSrc from, void* to)
    {
        ASSERT(from.m_offset != -1);
        
        reinterpret_cast<int*>(reinterpret_cast<ptrdiff_t>(code) + from.m_offset)[-1] = reinterpret_cast<ptrdiff_t>(to) - (reinterpret_cast<ptrdiff_t>(code) + from.m_offset);
    }
    
    static void* getRelocatedAddress(void* code, JmpSrc jump)
    {
        return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + jump.m_offset);
    }
    
    static void* getRelocatedAddress(void* code, JmpDst destination)
    {
        ASSERT(destination.m_offset != -1);

        return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + destination.m_offset);
    }
    
    static int getDifferenceBetweenLabels(JmpDst src, JmpDst dst)
    {
        return dst.m_offset - src.m_offset;
    }
    
    static int getDifferenceBetweenLabels(JmpDst src, JmpSrc dst)
    {
        return dst.m_offset - src.m_offset;
    }
    
    static int getDifferenceBetweenLabels(JmpSrc src, JmpDst dst)
    {
        return dst.m_offset - src.m_offset;
    }
    
    static void patchImmediate(intptr_t where, int32_t value)
    {
        reinterpret_cast<int32_t*>(where)[-1] = value;
    }
    
    static void patchPointer(intptr_t where, intptr_t value)
    {
        reinterpret_cast<intptr_t*>(where)[-1] = value;
    }
    
    static void patchBranchOffset(intptr_t where, void* destination)
    {
        intptr_t offset = reinterpret_cast<intptr_t>(destination) - where;
        ASSERT(offset == static_cast<int32_t>(offset));
        reinterpret_cast<int32_t*>(where)[-1] = static_cast<int32_t>(offset);
    }
    
    void* executableCopy(ExecutablePool* allocator)
    {
        void* copy = m_formatter.executableCopy(allocator);
        ASSERT(copy);
        return copy;
    }

private:

    class X86InstructionFormatter {

        static const int maxInstructionSize = 16;

    public:

        // Legacy prefix bytes:
        //
        // These are emmitted prior to the instruction.

        void prefix(OneByteOpcodeID pre)
        {
            m_buffer.putByte(pre);
        }

        // Word-sized operands / no operand instruction formatters.
        //
        // In addition to the opcode, the following operand permutations are supported:
        //   * None - instruction takes no operands.
        //   * One register - the low three bits of the RegisterID are added into the opcode.
        //   * Two registers - encode a register form ModRm (for all ModRm formats, the reg field is passed first, and a GroupOpcodeID may be passed in its place).
        //   * Three argument ModRM - a register, and a register and an offset describing a memory operand.
        //   * Five argument ModRM - a register, and a base register, an index, scale, and offset describing a memory operand.
        //
        // For 32-bit x86 targets, the address operand may also be provided as a void*.
        // On 64-bit targets REX prefixes will be planted as necessary, where high numbered registers are used.
        //
        // The twoByteOp methods plant two-byte Intel instructions sequences (first opcode byte 0x0F).

        void oneByteOp(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void oneByteOp_disp32(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(reg, base, offset);
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

#if !PLATFORM(X86_64)
        void oneByteOp(OneByteOpcodeID opcode, int reg, void* address)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, address);
        }
#endif

        void twoByteOp(TwoByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

#if PLATFORM(X86_64)
        // Quad-word-sized operands:
        //
        // Used to format 64-bit operantions, planting a REX.w prefix.
        // When planting d64 or f64 instructions, not requiring a REX.w prefix,
        // the normal (non-'64'-postfixed) formatters should be used.

        void oneByteOp64(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(0, 0, 0);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp64(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void oneByteOp64_disp32(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(reg, base, offset);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }
#endif

        // Byte-operands:
        //
        // These methods format byte operations.  Byte operations differ from the normal
        // formatters in the circumstances under which they will decide to emit REX prefixes.
        // These should be used where any register operand signifies a byte register.
        //
        // The disctinction is due to the handling of register numbers in the range 4..7 on
        // x86-64.  These register numbers may either represent the second byte of the first
        // four registers (ah..bh) or the first byte of the second four registers (spl..dil).
        //
        // Since ah..bh cannot be used in all permutations of operands (specifically cannot
        // be accessed where a REX prefix is present), these are likely best treated as
        // deprecated.  In order to ensure the correct registers spl..dil are selected a
        // REX prefix will be emitted for any byte register operand in the range 4..15.
        //
        // These formatters may be used in instructions where a mix of operand sizes, in which
        // case an unnecessary REX will be emitted, for example:
        //     movzbl %al, %edi
        // In this case a REX will be planted since edi is 7 (and were this a byte operand
        // a REX would be required to specify dil instead of bh).  Unneeded REX prefixes will
        // be silently ignored by the processor.
        //
        // Address operands should still be checked using regRequiresRex(), while byteRegRequiresRex()
        // is provided to check byte register operands.

        void oneByteOp8(OneByteOpcodeID opcode, GroupOpcodeID groupOp, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(groupOp, rm);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, RegisterID reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg)|byteRegRequiresRex(rm), reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, GroupOpcodeID groupOp, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(groupOp, rm);
        }

        // Immediates:
        //
        // An immedaite should be appended where appropriate after an op has been emitted.
        // The writes are unchecked since the opcode formatters above will have ensured space.

        void immediate8(int imm)
        {
            m_buffer.putByteUnchecked(imm);
        }

        void immediate32(int imm)
        {
            m_buffer.putIntUnchecked(imm);
        }

        void immediate64(int64_t imm)
        {
            m_buffer.putInt64Unchecked(imm);
        }

        JmpSrc immediateRel32()
        {
            m_buffer.putIntUnchecked(0);
            return JmpSrc(m_buffer.size());
        }

        // Administrative methods:

        size_t size() const { return m_buffer.size(); }
        bool isAligned(int alignment) const { return m_buffer.isAligned(alignment); }
        void* data() const { return m_buffer.data(); }
        void* executableCopy(ExecutablePool* allocator) { return m_buffer.executableCopy(allocator); }

    private:

        // Internals; ModRm and REX formatters.

        static const RegisterID noBase = X86::ebp;
        static const RegisterID hasSib = X86::esp;
        static const RegisterID noIndex = X86::esp;
#if PLATFORM(X86_64)
        static const RegisterID noBase2 = X86::r13;
        static const RegisterID hasSib2 = X86::r12;

        // Registers r8 & above require a REX prefixe.
        inline bool regRequiresRex(int reg)
        {
            return (reg >= X86::r8);
        }

        // Byte operand register spl & above require a REX prefix (to prevent the 'H' registers be accessed).
        inline bool byteRegRequiresRex(int reg)
        {
            return (reg >= X86::esp);
        }

        // Format a REX prefix byte.
        inline void emitRex(bool w, int r, int x, int b)
        {
            m_buffer.putByteUnchecked(PRE_REX | ((int)w << 3) | ((r>>3)<<2) | ((x>>3)<<1) | (b>>3));
        }

        // Used to plant a REX byte with REX.w set (for 64-bit operations).
        inline void emitRexW(int r, int x, int b)
        {
            emitRex(true, r, x, b);
        }

        // Used for operations with byte operands - use byteRegRequiresRex() to check register operands,
        // regRequiresRex() to check other registers (i.e. address base & index).
        inline void emitRexIf(bool condition, int r, int x, int b)
        {
            if (condition) emitRex(false, r, x, b);
        }

        // Used for word sized operations, will plant a REX prefix if necessary (if any register is r8 or above).
        inline void emitRexIfNeeded(int r, int x, int b)
        {
            emitRexIf(regRequiresRex(r) || regRequiresRex(x) || regRequiresRex(b), r, x, b);
        }
#else
        // No REX prefix bytes on 32-bit x86.
        inline bool regRequiresRex(int) { return false; }
        inline bool byteRegRequiresRex(int) { return false; }
        inline void emitRexIf(bool, int, int, int) {}
        inline void emitRexIfNeeded(int, int, int) {}
#endif

        enum ModRmMode {
            ModRmMemoryNoDisp,
            ModRmMemoryDisp8,
            ModRmMemoryDisp32,
            ModRmRegister,
        };

        void putModRm(ModRmMode mode, int reg, RegisterID rm)
        {
            m_buffer.putByteUnchecked((mode << 6) | ((reg & 7) << 3) | (rm & 7));
        }

        void putModRmSib(ModRmMode mode, int reg, RegisterID base, RegisterID index, int scale)
        {
            ASSERT(mode != ModRmRegister);

            // Encode sacle of (1,2,4,8) -> (0,1,2,3)
            int shift = 0;
            while (scale >>= 1)
                shift++;

            putModRm(mode, reg, hasSib);
            m_buffer.putByteUnchecked((shift << 6) | ((index & 7) << 3) | (base & 7));
        }

        void registerModRM(int reg, RegisterID rm)
        {
            putModRm(ModRmRegister, reg, rm);
        }

        void memoryModRM(int reg, RegisterID base, int offset)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a sib with no index & put the base in there.
#if PLATFORM(X86_64)
            if ((base == hasSib) || (base == hasSib2)) {
#else
            if (base == hasSib) {
#endif
                if (!offset) // No need to check if the base is noBase, since we know it is hasSib!
                    putModRmSib(ModRmMemoryNoDisp, reg, base, noIndex, 0);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRmSib(ModRmMemoryDisp8, reg, base, noIndex, 0);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRmSib(ModRmMemoryDisp32, reg, base, noIndex, 0);
                    m_buffer.putIntUnchecked(offset);
                }
            } else {
#if PLATFORM(X86_64)
                if (!offset && (base != noBase) && (base != noBase2))
#else
                if (!offset && (base != noBase))
#endif
                    putModRm(ModRmMemoryNoDisp, reg, base);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRm(ModRmMemoryDisp8, reg, base);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRm(ModRmMemoryDisp32, reg, base);
                    m_buffer.putIntUnchecked(offset);
                }
            }
        }
    
        void memoryModRM_disp32(int reg, RegisterID base, int offset)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a sib with no index & put the base in there.
#if PLATFORM(X86_64)
            if ((base == hasSib) || (base == hasSib2)) {
#else
            if (base == hasSib) {
#endif
                putModRmSib(ModRmMemoryDisp32, reg, base, noIndex, 0);
                m_buffer.putIntUnchecked(offset);
            } else {
                putModRm(ModRmMemoryDisp32, reg, base);
                m_buffer.putIntUnchecked(offset);
            }
        }
    
        void memoryModRM(int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            ASSERT(index != noIndex);

#if PLATFORM(X86_64)
            if (!offset && (base != noBase) && (base != noBase2))
#else
            if (!offset && (base != noBase))
#endif
                putModRmSib(ModRmMemoryNoDisp, reg, base, index, scale);
            else if (CAN_SIGN_EXTEND_8_32(offset)) {
                putModRmSib(ModRmMemoryDisp8, reg, base, index, scale);
                m_buffer.putByteUnchecked(offset);
            } else {
                putModRmSib(ModRmMemoryDisp32, reg, base, index, scale);
                m_buffer.putIntUnchecked(offset);
            }
        }

#if !PLATFORM(X86_64)
        void memoryModRM(int reg, void* address)
        {
            // noBase + ModRmMemoryNoDisp means noBase + ModRmMemoryDisp32!
            putModRm(ModRmMemoryNoDisp, reg, noBase);
            m_buffer.putIntUnchecked(reinterpret_cast<int32_t>(address));
        }
#endif

        AssemblerBuffer m_buffer;
    } m_formatter;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && PLATFORM(X86)

#endif // X86Assembler_h
