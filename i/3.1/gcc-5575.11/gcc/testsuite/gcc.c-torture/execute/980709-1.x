# pow() is not available on m6811/m6812 target, this test will not link.
if { [istarget "m6811-*-*"] || [istarget "m6812-*-*"]} {
	return 1
}
# XFAIL this test for AIX using -msoft-float.
# This test calls the system libm.a function pow.
# A false failure is reported if -msoft-float is used.
# AIX expects the the parameters to be passed in fp regs. 
if { [istarget powerpc-*-aix*] || [istarget rs6000-*-aix*] } {
    set torture_eval_before_execute {
	global compiler_conditional_xfail_data
	set compiler_conditional_xfail_data {
	    "Can not call system libm.a with -msoft-float" \
		    "*-*-aix*" \
		    { "-msoft-float" } \
		    { "" }
	}
    }
}
return 0
