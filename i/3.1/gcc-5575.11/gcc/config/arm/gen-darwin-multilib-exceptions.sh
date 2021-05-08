#!/bin/sh
# ALQAAHIRA LOCAL file 6611402 configurable multilib architectures
# This recursive function generates all of the pairwise combinations from a
# list of multilib options. The result is suitable for a multilib
# exceptions list.
EXCEPTIONS=
function gen_exceptions()
{
  if [ $# == 1 ] ; then
    return
  fi
  local opt=$1
  shift 1
  for opt2 in $@ ; do
    EXCEPTIONS+="*$opt*/*$opt2* "
  done
  gen_exceptions $@
}
if [ $# == 0 ] ; then exit ; fi
gen_exceptions $@
echo $EXCEPTIONS
