#!/bin/bash

TOTALSTART=$(date +%s)

#tr1 files
echo "Starting bzip2"
START=$(date +%s)
cat traces-1M/bzip2 | ./main config/default_8 > Results/bzip2/bzip2Results_default_8
cat traces-1M/bzip2 | ./main config/all-2way_8 > Results/bzip2/bzip2Results_all-2way_8
cat traces-1M/bzip2 | ./main config/all-4way_8 > Results/bzip2/bzip2Results_all-4way_8
cat traces-1M/bzip2 | ./main config/l2-4way_8 > Results/bzip2/bzip2Results_l2-4way_8
cat traces-1M/bzip2 | ./main config/all-fa_8 > Results/bzip2/bzip2Results_all-fa_8
cat traces-1M/bzip2 | ./main config/all-fa-l2big_8 > Results/bzip2/bzip2Results_all-fa-l2big_8
cat traces-1M/bzip2 | ./main config/l1-small_8 > Results/bzip2/bzip2Results_l1-small_8
cat traces-1M/bzip2 | ./main config/l1-small-4way_8 > Results/bzip2/bzip2Results_l1-small-4way_8
cat traces-1M/bzip2 | ./main config/l2-big_8 > Results/bzip2/bzip2Results_l2-big_8
cat traces-1M/bzip2 | ./main config/l1-2way_8 > Results/bzip2/bzip2Results_l1-2way_8
cat traces-1M/bzip2 | ./main config/l1-8way_8 > Results/bzip2/bzip2Results_l1-8way_8
cat traces-1M/bzip2 | ./main config/l1-small-4way_8 > Results/bzip2/bzip2Results_l1-small-4way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed bzip2 trace in $DIFF seconds."

#tr2 files
echo "Starting h264ref"
START=$(date +%s)
cat traces-1M/h264ref | ./main config/default_8 > Results/h264/h264Results_default_8
cat traces-1M/h264ref | ./main config/all-2way_8 > Results/h264/h264Results_all-2way_8
cat traces-1M/h264ref | ./main config/all-4way_8 > Results/h264/h264Results_all-4way_8
cat traces-1M/h264ref | ./main config/l2-4way_8 > Results/h264/h264Results_l2-4way_8
cat traces-1M/h264ref | ./main config/all-fa_8 > Results/h264/h264Results_all-fa_8
cat traces-1M/h264ref | ./main config/all-fa-l2big_8 > Results/h264/h264Results_all-fa-l2big_8
cat traces-1M/h264ref | ./main config/l1-small_8 > Results/h264/h264Results_l1-small_8
cat traces-1M/h264ref | ./main config/l1-small-4way_8 > Results/h264/h264Results_l1-small-4way_8
cat traces-1M/h264ref | ./main config/l2-big_8 > Results/h264/h264Results_l2-big_8
cat traces-1M/h264ref | ./main config/l1-2way_8 > Results/h264/h264Results_l1-2way_8
cat traces-1M/h264ref | ./main config/l1-8way_8 > Results/h264/h264Results_l1-8way_8
cat traces-1M/h264ref | ./main config/l1-small-4way_8 > Results/h264/h264Results_l1-small-4way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed h264ref trace in $DIFF seconds."

#tr3 files
echo "Starting libquantum"
START=$(date +%s)
cat traces-1M/libquantum | ./main config/default_8 > Results/libquantum/libquantumResults_default_8
cat traces-1M/libquantum | ./main config/all-2way_8 > Results/libquantum/libquantumResults_all-2way_8
cat traces-1M/libquantum | ./main config/all-4way_8 > Results/libquantum/libquantumResults_all-4way_8
cat traces-1M/libquantum | ./main config/l2-4way_8 > Results/libquantum/libquantumResults_l2-4way_8
cat traces-1M/libquantum | ./main config/all-fa_8 > Results/libquantum/libquantumResults_all-fa_8
cat traces-1M/libquantum | ./main config/all-fa-l2big_8 > Results/libquantum/libquantumResults_all-fa-l2big_8
cat traces-1M/libquantum | ./main config/l1-small_8 > Results/libquantum/libquantumResults_l1-small_8
cat traces-1M/libquantum | ./main config/l1-small-4way_8 > Results/libquantum/libquantumResults_l1-small-4way_8
cat traces-1M/libquantum | ./main config/l2-big_8 > Results/libquantum/libquantumResults_l2-big_8
cat traces-1M/libquantum | ./main config/l1-2way_8 > Results/libquantum/libquantumResults_l1-2way_8
cat traces-1M/libquantum | ./main config/l1-8way_8 > Results/libquantum/libquantumResults_l1-8way_8
cat traces-1M/libquantum | ./main config/l1-small-4way_8 > Results/libquantum/libquantumResults_l1-small-4way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed libquantum trace in $DIFF seconds."

TOTALEND=$(date +%s)
DIFF=$(( $TOTALEND - $TOTALSTART ))
echo "It took $DIFF seconds to run all the short traces"
