#! /bin/sh

host_architecture=""
requested_architecture="UNSET"
architecture_to_use=""

# classic-inferior-support
translate_mode=0
translate_binary=""

PATH=$PATH:/sbin:/bin:/usr/sbin:/usr/bin

# gdb is setgid procmod and dyld will truncate any DYLD_FRAMEWORK_PATH etc
# settings on exec.  The user is really trying to set these things
# in their process, not gdb.  So we smuggle it over the setgid border in
# GDB_DYLD_* where it'll be laundered inside gdb before invoking the inferior.

unset GDB_DYLD_FRAMEWORK_PATH
unset GDB_DYLD_FALLBACK_FRAMEWORK_PATH
unset GDB_DYLD_LIBRARY_PATH
unset GDB_DYLD_FALLBACK_LIBRARY_PATH
unset GDB_DYLD_ROOT_PATH
unset GDB_DYLD_PATHS_ROOT
unset GDB_DYLD_IMAGE_SUFFIX
unset GDB_DYLD_INSERT_LIBRARIES
[ -n "$DYLD_FRAMEWORK_PATH" ] && GDB_DYLD_FRAMEWORK_PATH="$DYLD_FRAMEWORK_PATH"
[ -n "$DYLD_FALLBACK_FRAMEWORK_PATH" ] && GDB_DYLD_FALLBACK_FRAMEWORK_PATH="$DYLD_FALLBACK_FRAMEWORK_PATH"
[ -n "$DYLD_LIBRARY_PATH" ] && GDB_DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH"
[ -n "$DYLD_FALLBACK_LIBRARY_PATH" ] && GDB_DYLD_FALLBACK_LIBRARY_PATH="$DYLD_FALLBACK_LIBRARY_PATH"
[ -n "$DYLD_ROOT_PATH" ] && GDB_DYLD_ROOT_PATH="$DYLD_ROOT_PATH"
[ -n "$DYLD_PATHS_ROOT" ] && GDB_DYLD_PATHS_ROOT="$DYLD_PATHS_ROOT"
[ -n "$DYLD_IMAGE_SUFFIX" ] && GDB_DYLD_IMAGE_SUFFIX="$DYLD_IMAGE_SUFFIX"
[ -n "$DYLD_INSERT_LIBRARIES" ] && GDB_DYLD_INSERT_LIBRARIES="$DYLD_INSERT_LIBRARIES"
export GDB_DYLD_FRAMEWORK_PATH
export GDB_DYLD_FALLBACK_FRAMEWORK_PATH
export GDB_DYLD_LIBRARY_PATH
export GDB_DYLD_FALLBACK_LIBRARY_PATH
export GDB_DYLD_ROOT_PATH
export GDB_DYLD_PATHS_ROOT
export GDB_DYLD_IMAGE_SUFFIX
export GDB_DYLD_INSERT_LIBRARIES

# dyld will warn if any of these are set and the user invokes a setgid program
# like gdb.
unset DYLD_FRAMEWORK_PATH
unset DYLD_FALLBACK_FRAMEWORK_PATH
unset DYLD_LIBRARY_PATH
unset DYLD_FALLBACK_LIBRARY_PATH
unset DYLD_ROOT_PATH
unset DYLD_PATHS_ROOT
unset DYLD_IMAGE_SUFFIX
unset DYLD_INSERT_LIBRARIES

host_architecture=`/usr/bin/arch 2>/dev/null` || host_architecture=""

if [ "$host_architecture" == "arm" ]; then
  host_cpusubtype=`sysctl hw.cpusubtype | awk '{ print $NF }'` || host_cputype=""
  case "$host_cpusubtype" in
    6) host_architecture="armv6" ;;
    7) host_architecture="armv5" ;;
    9) host_architecture="armv7" ;;
  esac
elif [ -z "$host_architecture" ]; then
    echo "There was an error executing 'arch(1)'; assuming 'i386'.";
    host_architecture="i386";
fi


case "$1" in
  --help)
    echo "  --translate        Debug applications running under translation." >&2
    echo "  -arch i386|arm|armv6|armv7|x86_64|ppc     Specify a gdb targetting a specific architecture" >&2
    ;;
  -arch=* | -a=* | --arch=*)
    requested_architecture=`echo "$1" | sed 's,^[^=]*=,,'`
    shift;;
  -arch | -a | --arch)
    shift
    requested_architecture="$1"
    shift;;
  -translate | --translate | -oah* | --oah*)
    translate_mode=1
    shift;;
esac

if [ -z "$requested_architecture" ]
then
  echo ERROR: No architecture specified with -arch argument. >&2
  exit 1
fi
[ "$requested_architecture" = "UNSET" ] && requested_architecture=""

if [ $translate_mode -eq 1 ]
then
  if [ "$host_architecture" = i386 -a -x /usr/libexec/oah/translate ]
  then
    requested_architecture="ppc"
    translate_binary="/usr/libexec/oah/translate -execOAH"
  else
    echo ERROR: translate not available.  Running in normal debugger mode. >&2
  fi
fi

if [ -n "$requested_architecture" ]
then
  case $requested_architecture in
    ppc* | i386 | x86_64 | arm*)
     ;;
    *)
      echo Unrecognized architecture \'$requested_architecture\', using host arch. >&2
      requested_architecture=""
      ;;
  esac
fi

if [ -n "$requested_architecture" ]
then
  architecture_to_use="$requested_architecture"
else
  # No architecture was specified. We will try to find the executable
  # or a core file in the list of arguments, and launch the correct
  # gdb for the job. If there are multiple architectures in the executable,
  # we will search for the architecture that matches the host architecture.
  # If all this searching doesn't produce a match, we will use a gdb that
  # matches the host architecture by default.
  best_arch=
  exec_file=
  core_file=
  for arg in "$@"
  do
    case "$arg" in
      -*)
        # Skip all option arguments
        ;;
      *)
        # Call file to determine the file type of the argument
        file_result=`file "$arg"`;
        case "$file_result" in
          *Mach-O*core*)
            core_file=$arg
            ;;
          *Mach-O*)
            exec_file=$arg
            ;;
          *)
            if [ -x "$arg" ]; then
              exec_file="$arg"
            fi
            ;;
        esac
        ;;
    esac
  done

  # Get a list of possible architectures in FILE_ARCHS.
  # If we have a core file, we must use it to determine the architecture,
  # else we use the architectures in the executable file.
  file_archs=
  if [ -n "$core_file" ]; then
    file_archs=`file "$core_file" | awk '{ print $NF }'`
  else
    if [ -n "$exec_file" ]; then
      if [ ! -d "$exec_file" ]; then
      	lipo_info=`lipo -info "$exec_file" 2>/dev/null` && file_archs=`echo -n "$lipo_info" | sed 's/.*: //'`
      fi
    fi
  fi

  # Iterate through the architectures and try and find the best match.
  for file_arch in $file_archs 
  do
    # If we don't have any best architecture set yet, use this in case
    # none of them match the host architecture.
    if [ -z "$best_arch" ]; then
      best_arch="$file_arch"
    fi

    # See if the file architecture matches the host, and if so set the
    # best architecture to that.
    if [ "$file_arch" = "$host_architecture" ]; then
      best_arch="$file_arch"
    fi
  done

  case "$best_arch" in
    ppc* | i386 | x86_64 | arm*)
      # We found a plausible architecture and we will use it
      architecture_to_use="$best_arch"
      ;;
    *)
      # We did not find a plausible architecture, use the host architecture
      architecture_to_use="$host_architecture"
      ;;
  esac
fi

# If GDB_ROOT is not set, then figure it out
# from $0.  We need this for gdb's that are
# not installed in /usr/bin.

GDB_ROOT_SET=${GDB_ROOT:+set}
if [ "$GDB_ROOT_SET" != "set" ]
then
  gdb_bin="$0"
  if [ -L "$gdb_bin" ]
  then
    gdb_bin=`readlink "$gdb_bin"`
  fi
  gdb_bin_dirname=`dirname "$gdb_bin"`
  GDB_ROOT=`cd "$gdb_bin_dirname"/../.. ; pwd`
  if [ "$GDB_ROOT" = "/" ]
      then
        GDB_ROOT=
  fi
fi

case "$architecture_to_use" in
  ppc*)
    # Make sure we specify the architecture to gdb when launching if the
    # gdb can handle both 32 and 64 bit variants.
    if [ -z "$requested_architecture" ]; then
      requested_architecture=$architecture_to_use;
    fi
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
    ;;
  i386 | x86_64)
    # Make sure we specify the architecture to gdb when launching if the
    # gdb can handle both 32 and 64 bit variants.
    if [ -z "$requested_architecture" ]; then
      requested_architecture=$architecture_to_use;
    fi
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-darwin"
    ;;
  arm*)
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-arm-apple-darwin"
      case "$architecture_to_use" in
        armv6) 
          osabiopts="--osabi DarwinV6" 
          ;;
        armv7) 
          osabiopts="--osabi DarwinV7" 
          ;;
        *)
          # Make the REQUESTED_ARCHITECTURE the empty string so
          # we can let gdb auto-detect the cpu type and subtype
          requested_architecture=""
          ;;
      esac
      ;;
  *)
    echo "Unknown architecture '$architecture_to_use'; using 'ppc' instead.";
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
    ;;
esac

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

if [ -n "$requested_architecture" -a $translate_mode -eq 0 ]
then
  exec $translate_binary "$gdb" --arch "$requested_architecture" "$@"
else
  exec $translate_binary "$gdb" $osabiopts "$@"
fi
