##
# Copyright (c) 2009 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# <rdar://problem/6927148> link failure with -O4 on i386#
# Check that LTO can bring in an ObjC class from an archive member
#

LLVMGCC = /Developer/usr/bin/llvm-gcc-4.2 -arch ${ARCH}
LLVMGXX = /Developer/usr/bin/llvm-g++-4.2 -arch ${ARCH}

run: all

all:
	${CC} ${CCFLAGS} foo.m -c -o foo.o
	${CC} ${CCFLAGS} bar.m -c -o bar.o
	${CC} ${CCFLAGS} foo2.c -c -o foo2.o
	${CC} ${CCFLAGS} bar2.c -c -o bar2.o
	libtool -static foo.o bar.o foo2.o bar2.o -o libfoobar.a 
	${LLVMGCC} ${CCFLAGS} --emit-llvm main.m -c -o main.o
	${LLVMGCC} ${CCFLAGS} main.o -o main libfoobar.a -framework Foundation
	${PASS_IFF_GOOD_MACHO} main
	
clean:
	rm -rf main foo.o bar.o foo2.o bar2.o libfoobar.a main.o
