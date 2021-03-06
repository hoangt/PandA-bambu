#!/bin/bash
script=`readlink -e $0`
root_dir=`dirname $script`
export PATH=../../src:../../../src:/opt/panda/bin:$PATH

mkdir -p hls
cd hls
echo "#synthesis with VIVADO RTL and simulation"
bambu --print-dot $root_dir/asm.c --simulate --generate-tb=$root_dir/test.xml -v2 --C-no-parse=$root_dir/c_stub.c $@
return_value=$? 
if test $return_value != 0; then
   exit $return_value
fi
cd ..
exit 0
