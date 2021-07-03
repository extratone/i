# Copyright (C) 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require 'ast'
require 'opt'
require 'risc'

# This file contains utilities that should be in risc.rb if it wasn't for the
# fact that risc.rb is shared with the ARMv7 backend, and may be part of a
# code dump before ARM64 goes public.
#
# FIXME: when ARM64 goes public, we should merge this file into risc.rb

#
# Lowering of the not instruction. The following:
#
# noti t0
#
# becomes:
#
# xori -1, t0
#

def riscLowerNot(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "noti", "notp"
                raise "Wrong nubmer of operands at #{node.codeOriginString}" unless node.operands.size == 1
                suffix = node.opcode[-1..-1]
                newList << Instruction.new(node.codeOrigin, "xor" + suffix,
                                           [Immediate.new(node.codeOrigin, -1), node.operands[0]])
            else
                newList << node
            end
        else
            newList << node
        end
    }
    return newList
end

#
# Lowing of complex branch ops on 64-bit. For example:
#
# bmulio foo, bar, baz
#
# becomes:
#
# smulli foo, bar, bar
# rshiftp bar, 32, tmp1
# rshifti bar, 31, tmp2
# zxi2p bar, bar 
# bineq tmp1, tmp2, baz
#

def riscLowerHardBranchOps64(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction and node.opcode == "bmulio"
            tmp1 = Tmp.new(node.codeOrigin, :gpr)
            tmp2 = Tmp.new(node.codeOrigin, :gpr)
            newList << Instruction.new(node.codeOrigin, "smulli", [node.operands[0], node.operands[1], node.operands[1]])
            newList << Instruction.new(node.codeOrigin, "rshiftp", [node.operands[1], Immediate.new(node.codeOrigin, 32), tmp1])
            newList << Instruction.new(node.codeOrigin, "rshifti", [node.operands[1], Immediate.new(node.codeOrigin, 31), tmp2])
            newList << Instruction.new(node.codeOrigin, "zxi2p", [node.operands[1], node.operands[1]])
            newList << Instruction.new(node.codeOrigin, "bineq", [tmp1, tmp2, node.operands[2]])
        else
            newList << node
        end
    }
    newList
end

#
# Lowering of test instructions. For example:
#
# btiz t0, t1, .foo
#
# becomes:
#
# andi t0, t1, tmp
# bieq tmp, 0, .foo
#
# and another example:
#
# tiz t0, t1, t2
#
# becomes:
#
# andi t0, t1, tmp
# cieq tmp, 0, t2
#

def riscLowerTest(list)
    def emit(newList, andOpcode, branchOpcode, node)
        if node.operands.size == 2
            newList << Instruction.new(node.codeOrigin, branchOpcode, [node.operands[0], Immediate.new(node.codeOrigin, 0), node.operands[1]])
            return
        end
        
        raise "Incorrect number of operands at #{codeOriginString}" unless node.operands.size == 3

        if node.operands[0].immediate? and node.operands[0].value == -1
            newList << Instruction.new(node.codeOrigin, branchOpcode, [node.operands[1], Immediate.new(node.codeOrigin, 0), node.operands[2]])
            return
        end

        if node.operands[1].immediate? and node.operands[1].value == -1
            newList << Instruction.new(node.codeOrigin, branchOpcode, [node.operands[0], Immediate.new(node.codeOrigin, 0), node.operands[2]])
            return
        end
        
        tmp = Tmp.new(node.codeOrigin, :gpr)
        newList << Instruction.new(node.codeOrigin, andOpcode, [node.operands[0], node.operands[1], tmp])
        newList << Instruction.new(node.codeOrigin, branchOpcode, [tmp, Immediate.new(node.codeOrigin, 0), node.operands[2]])
    end
    
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "btis"
                emit(newList, "andi", "bilt", node)
            when "btiz"
                emit(newList, "andi", "bieq", node)
            when "btinz"
                emit(newList, "andi", "bineq", node)
            when "btps"
                emit(newList, "andp", "bplt", node)
            when "btpz"
                emit(newList, "andp", "bpeq", node)
            when "btpnz"
                emit(newList, "andp", "bpneq", node)
            when "btqs"
                emit(newList, "andq", "bqlt", node)
            when "btqz"
                emit(newList, "andq", "bqeq", node)
            when "btqnz"
                emit(newList, "andq", "bqneq", node)
            when "btbs"
                emit(newList, "andi", "bblt", node)
            when "btbz"
                emit(newList, "andi", "bbeq", node)
            when "btbnz"
                emit(newList, "andi", "bbneq", node)
            when "tis"
                emit(newList, "andi", "cilt", node)
            when "tiz"
                emit(newList, "andi", "cieq", node)
            when "tinz"
                emit(newList, "andi", "cineq", node)
            when "tps"
                emit(newList, "andp", "cplt", node)
            when "tpz"
                emit(newList, "andp", "cpeq", node)
            when "tpnz"
                emit(newList, "andp", "cpneq", node)
            when "tqs"
                emit(newList, "andq", "cqlt", node)
            when "tqz"
                emit(newList, "andq", "cqeq", node)
            when "tqnz"
                emit(newList, "andq", "cqneq", node)
            when "tbs"
                emit(newList, "andi", "cblt", node)
            when "tbz"
                emit(newList, "andi", "cbeq", node)
            when "tbnz"
                emit(newList, "andi", "cbneq", node)
            else
                newList << node
            end
        else
            newList << node
        end
    }
    return newList
end

