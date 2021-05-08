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

#ifndef JITInlineMethods_h
#define JITInlineMethods_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#if PLATFORM(WIN)
#undef FIELD_OFFSET // Fix conflict with winnt.h.
#endif

// FIELD_OFFSET: Like the C++ offsetof macro, but you can use it with classes.
// The magic number 0x4000 is insignificant. We use it to avoid using NULL, since
// NULL can cause compiler problems, especially in cases of multiple inheritance.
#define FIELD_OFFSET(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

namespace JSC {

ALWAYS_INLINE void JIT::killLastResultRegister()
{
    m_lastResultBytecodeRegister = std::numeric_limits<int>::max();
}

// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, RegisterID dst)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    // TODO: we want to reuse values that are already in registers if we can - add a register allocator!
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValuePtr value = m_codeBlock->getConstant(src);
        move(ImmPtr(JSValuePtr::encode(value)), dst);
        killLastResultRegister();
        return;
    }

    if (src == m_lastResultBytecodeRegister && m_codeBlock->isTemporaryRegisterIndex(src)) {
        bool atJumpTarget = false;
        while (m_jumpTargetsPosition < m_codeBlock->numberOfJumpTargets() && m_codeBlock->jumpTarget(m_jumpTargetsPosition) <= m_bytecodeIndex) {
            if (m_codeBlock->jumpTarget(m_jumpTargetsPosition) == m_bytecodeIndex)
                atJumpTarget = true;
            ++m_jumpTargetsPosition;
        }

        if (!atJumpTarget) {
            // The argument we want is already stored in eax
            if (dst != X86::eax)
                move(X86::eax, dst);
            killLastResultRegister();
            return;
        }
    }

    loadPtr(Address(callFrameRegister, src * sizeof(Register)), dst);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2)
{
    if (src2 == m_lastResultBytecodeRegister) {
        emitGetVirtualRegister(src2, dst2);
        emitGetVirtualRegister(src1, dst1);
    } else {
        emitGetVirtualRegister(src1, dst1);
        emitGetVirtualRegister(src2, dst2);
    }
}

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArg(RegisterID src, unsigned argumentNumber)
{
    poke(src, argumentNumber);
}

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(unsigned value, unsigned argumentNumber)
{
    poke(Imm32(value), argumentNumber);
}

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(void* value, unsigned argumentNumber)
{
    poke(ImmPtr(value), argumentNumber);
}

ALWAYS_INLINE void JIT::emitGetJITStubArg(unsigned argumentNumber, RegisterID dst)
{
    peek(dst, argumentNumber);
}

ALWAYS_INLINE JSValuePtr JIT::getConstantOperand(unsigned src)
{
    ASSERT(m_codeBlock->isConstantRegisterIndex(src));
    return m_codeBlock->getConstant(src);
}

ALWAYS_INLINE int32_t JIT::getConstantOperandImmediateInt(unsigned src)
{
    return getConstantOperand(src).getInt32Fast();
}

ALWAYS_INLINE bool JIT::isOperandConstantImmediateInt(unsigned src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32Fast();
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValuePtr value = m_codeBlock->getConstant(src);
        emitPutJITStubArgConstant(JSValuePtr::encode(value), argumentNumber);
    } else {
        loadPtr(Address(callFrameRegister, src * sizeof(Register)), scratch);
        emitPutJITStubArg(scratch, argumentNumber);
    }

    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutCTIParam(void* value, unsigned name)
{
    poke(ImmPtr(value), name);
}

ALWAYS_INLINE void JIT::emitPutCTIParam(RegisterID from, unsigned name)
{
    poke(from, name);
}

ALWAYS_INLINE void JIT::emitGetCTIParam(unsigned name, RegisterID to)
{
    peek(to, name);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(from, Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(ImmPtr(value), Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, RegisterID to)
{
    loadPtr(Address(callFrameRegister, entry * sizeof(Register)), to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(unsigned dst, RegisterID from)
{
    storePtr(from, Address(callFrameRegister, dst * sizeof(Register)));
    m_lastResultBytecodeRegister = (from == X86::eax) ? dst : std::numeric_limits<int>::max();
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    storePtr(ImmPtr(JSValuePtr::encode(jsUndefined())), Address(callFrameRegister, dst * sizeof(Register)));
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE JIT::Jump JIT::emitNakedCall(X86::RegisterID r)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    Jump nakedCall = call(r);
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex));
    return nakedCall;
}

ALWAYS_INLINE JIT::Jump JIT::emitNakedCall(void* function)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    Jump nakedCall = call();
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex, function));
    return nakedCall;
}

#if USE(JIT_STUB_ARGUMENT_REGISTER)
ALWAYS_INLINE void JIT::restoreArgumentReference()
{
#if PLATFORM(X86_64)
    move(X86::esp, X86::edi);
#else
    move(X86::esp, X86::ecx);
#endif
    emitPutCTIParam(callFrameRegister, STUB_ARGS_callFrame);
}
ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline()
{
    // In the trampoline on x86-64, the first argument register is not overwritten.
#if !PLATFORM(X86_64)
    move(X86::esp, X86::ecx);
    addPtr(Imm32(sizeof(void*)), X86::ecx);
#endif
}
#elif USE(JIT_STUB_ARGUMENT_STACK)
ALWAYS_INLINE void JIT::restoreArgumentReference()
{
    storePtr(X86::esp, X86::esp);
    emitPutCTIParam(callFrameRegister, STUB_ARGS_callFrame);
}
ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline() {}
#else // JIT_STUB_ARGUMENT_VA_LIST
ALWAYS_INLINE void JIT::restoreArgumentReference()
{
    emitPutCTIParam(callFrameRegister, STUB_ARGS_callFrame);
}
ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline() {}
#endif

ALWAYS_INLINE JIT::Jump JIT::emitCTICall_internal(void* helper)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin() + m_bytecodeIndex, true);
#endif
    restoreArgumentReference();
    Jump ctiCall = call();
    m_calls.append(CallRecord(ctiCall, m_bytecodeIndex, helper));
#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin() + m_bytecodeIndex, false);
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JIT::Jump JIT::checkStructure(RegisterID reg, Structure* structure)
{
    return jnePtr(Address(reg, FIELD_OFFSET(JSCell, m_structure)), ImmPtr(structure));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfJSCell(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return jzPtr(reg, tagMaskRegister);
#else
    return jz32(reg, Imm32(JSImmediate::TagMask));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfBothJSCells(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    orPtr(reg2, scratch);
    return emitJumpIfJSCell(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfJSCell(RegisterID reg)
{
    addSlowCase(emitJumpIfJSCell(reg));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotJSCell(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return jnzPtr(reg, tagMaskRegister);
#else
    return jnz32(reg, Imm32(JSImmediate::TagMask));
#endif
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg)
{
    addSlowCase(emitJumpIfNotJSCell(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        emitJumpSlowCaseIfNotJSCell(reg);
}

ALWAYS_INLINE void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        linkSlowCase(iter);
}

#if USE(ALTERNATE_JSIMMEDIATE)
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateNumber(RegisterID reg)
{
    return jnzPtr(reg, tagTypeNumberRegister);
}
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateNumber(RegisterID reg)
{
    return jzPtr(reg, tagTypeNumberRegister);
}
#endif

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateInteger(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return jaePtr(reg, tagTypeNumberRegister);
#else
    return jnz32(reg, Imm32(JSImmediate::TagTypeNumber));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateInteger(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return jbPtr(reg, tagTypeNumberRegister);
#else
    return jz32(reg, Imm32(JSImmediate::TagTypeNumber));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateIntegers(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    andPtr(reg2, scratch);
    return emitJumpIfNotImmediateInteger(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmediateInteger(RegisterID reg)
{
    addSlowCase(emitJumpIfNotImmediateInteger(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmediateIntegers(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    addSlowCase(emitJumpIfNotImmediateIntegers(reg1, reg2, scratch));
}

#if !USE(ALTERNATE_JSIMMEDIATE)
ALWAYS_INLINE void JIT::emitFastArithDeTagImmediate(RegisterID reg)
{
    subPtr(Imm32(JSImmediate::TagTypeNumber), reg);
}

ALWAYS_INLINE JIT::Jump JIT::emitFastArithDeTagImmediateJumpIfZero(RegisterID reg)
{
    return jzSubPtr(Imm32(JSImmediate::TagTypeNumber), reg);
}
#endif

ALWAYS_INLINE void JIT::emitFastArithReTagImmediate(RegisterID src, RegisterID dest)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    emitFastArithIntToImmNoCheck(src, dest);
#else
    if (src != dest)
        move(src, dest);
    addPtr(Imm32(JSImmediate::TagTypeNumber), dest);
#endif
}

ALWAYS_INLINE void JIT::emitFastArithImmToInt(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    UNUSED_PARAM(reg);
#else
    rshiftPtr(Imm32(JSImmediate::IntegerPayloadShift), reg);
#endif
}

// operand is int32_t, must have been zero-extended if register is 64-bit.
ALWAYS_INLINE void JIT::emitFastArithIntToImmNoCheck(RegisterID src, RegisterID dest)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    if (src != dest)
        move(src, dest);
    orPtr(tagTypeNumberRegister, dest);
#else
    signExtend32ToPtr(src, dest);
    addPtr(dest, dest);
    emitFastArithReTagImmediate(dest, dest);
#endif
}

ALWAYS_INLINE void JIT::emitTagAsBoolImmediate(RegisterID reg)
{
    lshift32(Imm32(JSImmediate::ExtendedPayloadShift), reg);
    or32(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool)), reg);
}

ALWAYS_INLINE void JIT::addSlowCase(Jump jump)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_slowCases.append(SlowCaseEntry(jump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addJump(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_jmpTable.append(JumpTable(jump, m_bytecodeIndex + relativeOffset));
}

ALWAYS_INLINE void JIT::emitJumpSlowToHot(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    jump.linkTo(m_labels[m_bytecodeIndex + relativeOffset], this);
}

}

#endif // ENABLE(JIT)

#endif
