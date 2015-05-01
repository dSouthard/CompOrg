#!/bin/bash

TOTALSTART=$(date +%s)

#bzip2 files
echo "Starting bzip2: 12 Files"
START=$(date +%s)
zcat traces-5M/bzip2.gz | ./main config/default_8 > Results/bzip2/bzip2Results_default_8
echo "Finished: 1"
zcat traces-5M/bzip2.gz | ./main config/all-2way_8 > Results/bzip2/bzip2Results_all-2way_8
echo "Finished: 2"
zcat traces-5M/bzip2.gz | ./main config/all-4way_8 > Results/bzip2/bzip2Results_all-4way_8
echo "Finished: 3"
zcat traces-5M/bzip2.gz | ./main config/l2-4way_8 > Results/bzip2/bzip2Results_l2-4way_8
echo "Finished: 4"
zcat traces-5M/bzip2.gz | ./main config/all-fa_8 > Results/bzip2/bzip2Results_all-fa_8
echo "Finished: 5"
zcat traces-5M/bzip2.gz | ./main config/all-fa-l2big_8 > Results/bzip2/bzip2Results_all-fa-l2big_8
echo "Finished: 6"
zcat traces-5M/bzip2.gz | ./main config/l1-small_8 > Results/bzip2/bzip2Results_l1-small_8
echo "Finished: 7"
zcat traces-5M/bzip2.gz | ./main config/l1-small-4way_8 > Results/bzip2/bzip2Results_l1-small-4way_8
echo "Finished: 8"
zcat traces-5M/bzip2.gz | ./main config/l2-big_8 > Results/bzip2/bzip2Results_l2-big_8
echo "Finished: 9"
zcat traces-5M/bzip2.gz | ./main config/l1-2way_8 > Results/bzip2/bzip2Results_l1-2way_8
echo "Finished: 10"
zcat traces-5M/bzip2.gz | ./main config/l1-8way_8 > Results/bzip2/bzip2Results_l1-8way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed bzip2 trace in $DIFF seconds."

#h264ref files
echo "Starting h264ref: 12 Files"
START=$(date +%s)
zcat traces-5M/h264ref.gz | ./main config/default_8 > Results/h264/h264Results_default_8
echo "Finished: 1"
zcat traces-5M/h264ref.gz | ./main config/all-2way_8 > Results/h264/h264Results_all-2way_8
echo "Finished: 2"
zcat traces-5M/h264ref.gz | ./main config/all-4way_8 > Results/h264/h264Results_all-4way_8
echo "Finished: 3"
zcat traces-5M/h264ref.gz | ./main config/l2-4way_8 > Results/h264/h264Results_l2-4way_8
echo "Finished: 4"
zcat traces-5M/h264ref.gz | ./main config/all-fa_8 > Results/h264/h264Results_all-fa_8
echo "Finished: 5"
zcat traces-5M/h264ref.gz | ./main config/all-fa-l2big_8 > Results/h264/h264Results_all-fa-l2big_8
echo "Finished: 6"
zcat traces-5M/h264ref.gz | ./main config/l1-small_8 > Results/h264/h264Results_l1-small_8
echo "Finished: 7"
zcat traces-5M/h264ref.gz | ./main config/l1-small-4way_8 > Results/h264/h264Results_l1-small-4way_8
echo "Finished: 8"
zcat traces-5M/h264ref.gz | ./main config/l2-big_8 > Results/h264/h264Results_l2-big_8
echo "Finished: 9"
zcat traces-5M/h264ref.gz | ./main config/l1-2way_8 > Results/h264/h264Results_l1-2way_8
echo "Finished: 10"
zcat traces-5M/h264ref.gz | ./main config/l1-8way_8 > Results/h264/h264Results_l1-8way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed h264ref trace in $DIFF seconds."

#libquantum files
echo "Starting libquantum: 12 Files"
START=$(date +%s)
zcat traces-5M/libquantum.gz | ./main config/default_8 > Results/libquantum/libquantumResults_default_8
echo "Finished: 1"
zcat traces-5M/libquantum.gz | ./main config/all-2way_8 > Results/libquantum/libquantumResults_all-2way_8
echo "Finished: 2"
zcat traces-5M/libquantum.gz | ./main config/all-4way_8 > Results/libquantum/libquantumResults_all-4way_8
echo "Finished: 3"
zcat traces-5M/libquantum.gz | ./main config/l2-4way_8 > Results/libquantum/libquantumResults_l2-4way_8
echo "Finished: 4"
zcat traces-5M/libquantum.gz | ./main config/all-fa_8 > Results/libquantum/libquantumResults_all-fa_8
echo "Finished: 5"
zcat traces-5M/libquantum.gz | ./main config/all-fa-l2big_8 > Results/libquantum/libquantumResults_all-fa-l2big_8
echo "Finished: 6"
zcat traces-5M/libquantum.gz | ./main config/l1-small_8 > Results/libquantum/libquantumResults_l1-small_8
echo "Finished: 7"
zcat traces-5M/libquantum.gz | ./main config/l1-small-4way_8 > Results/libquantum/libquantumResults_l1-small-4way_8
echo "Finished: 8"
zcat traces-5M/libquantum.gz | ./main config/l2-big_8 > Results/libquantum/libquantumResults_l2-big_8
echo "Finished: 9"
zcat traces-5M/libquantum.gz | ./main config/l1-2way_8 > Results/libquantum/libquantumResults_l1-2way_8
echo "Finished: 10"
zcat traces-5M/libquantum.gz | ./main config/l1-8way_8 > Results/libquantum/libquantumResults_l1-8way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed libquantum trace in $DIFF seconds."

#omnetpp files
echo "Starting omnetpp: 12 Files"
START=$(date +%s)
zcat traces-5M/omnetpp.gz | ./main config/default_8 > Results/omnetpp/omnetppResults_default_8
echo "Finished: 1"
zcat traces-5M/omnetpp.gz | ./main config/all-2way_8 > Results/omnetpp/omnetppResults_all-2way_8
echo "Finished: 2"
zcat traces-5M/omnetpp.gz | ./main config/all-4way_8 > Results/omnetpp/omnetppResults_all-4way_8
echo "Finished: 3"
zcat traces-5M/omnetpp.gz | ./main config/l2-4way_8 > Results/omnetpp/omnetppResults_l2-4way_8
echo "Finished: 4"
zcat traces-5M/omnetpp.gz | ./main config/all-fa_8 > Results/omnetpp/omnetppResults_all-fa_8
echo "Finished: 5"
zcat traces-5M/omnetpp.gz | ./main config/all-fa-l2big_8 > Results/omnetpp/omnetppResults_all-fa-l2big_8
echo "Finished: 6"
zcat traces-5M/omnetpp.gz | ./main config/l1-small_8 > Results/omnetpp/omnetppResults_l1-small_8
echo "Finished: 7"
zcat traces-5M/omnetpp.gz | ./main config/l1-small-4way_8 > Results/omnetpp/omnetppResults_l1-small-4way_8
echo "Finished: 8"
zcat traces-5M/omnetpp.gz | ./main config/l2-big_8 > Results/omnetpp/omnetppResults_l2-big_8
echo "Finished: 9"
zcat traces-5M/omnetpp.gz | ./main config/l1-2way_8 > Results/omnetpp/omnetppResults_l1-2way_8
echo "Finished: 10"
zcat traces-5M/omnetpp.gz | ./main config/l1-8way_8 > Results/omnetpp/omnetppResults_l1-8way_8
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed omnetpp trace in $DIFF seconds."

#sjeng files
echo "Starting sjeng: 48 Files, 4 per configuration. Starting: Default"
START=$(date +%s)
zcat traces-5M/sjeng.gz | ./main config/default_8 > Results/sjeng/sjengResults_default_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/default_16 > Results/sjeng/sjengResults_default_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/default_32 > Results/sjeng/sjengResults_default_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/default_64 > Results/sjeng/sjengResults_default_64

echo "Starting sjeng: All 2-Way"
zcat traces-5M/sjeng.gz | ./main config/all-2way_8 > Results/sjeng/sjengResults_all-2way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/all-2way_16 > Results/sjeng/sjengResults_all-2way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/all-2way_32 > Results/sjeng/sjengResults_all-2way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/all-2way_64 > Results/sjeng/sjengResults_all-2way_64

echo "Starting sjeng: All 4-Way"
zcat traces-5M/sjeng.gz | ./main config/all-4way_8 > Results/sjeng/sjengResults_all-4way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/all-4way_16 > Results/sjeng/sjengResults_all-4way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/all-4way_32 > Results/sjeng/sjengResults_all-4way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/all-4way_64 > Results/sjeng/sjengResults_all-4way_64

echo "Starting sjeng: L2 4-Way"
zcat traces-5M/sjeng.gz | ./main config/l2-4way_8 > Results/sjeng/sjengResults_l2-4way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l2-4way_16 > Results/sjeng/sjengResults_l2-4way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l2-4way_32 > Results/sjeng/sjengResults_l2-4way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l2-4way_64 > Results/sjeng/sjengResults_l2-4way_64

echo "Starting sjeng: All FA"
zcat traces-5M/sjeng.gz | ./main config/all-fa_8 > Results/sjeng/sjengResults_all-fa_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/all-fa_16 > Results/sjeng/sjengResults_all-fa_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/all-fa_32 > Results/sjeng/sjengResults_all-fa_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/all-fa_64 > Results/sjeng/sjengResults_all-fa_64

echo "Starting sjeng: All FA, L2 Big"
zcat traces-5M/sjeng.gz | ./main config/all-fa-l2big_8 > Results/sjeng/sjengResults_all-fa-l2big_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/all-fa-l2big_16 > Results/sjeng/sjengResults_all-fa-l2big_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/all-fa-l2big_32 > Results/sjeng/sjengResults_all-fa-l2big_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/all-fa-l2big_64 > Results/sjeng/sjengResults_all-fa-l2big_64

echo "Starting sjeng: L1 Small"
zcat traces-5M/sjeng.gz | ./main config/l1-small_8 > Results/sjeng/sjengResults_l1-small_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l1-small_16 > Results/sjeng/sjengResults_l1-small_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l1-small_32 > Results/sjeng/sjengResults_l1-small_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l1-small_64 > Results/sjeng/sjengResults_l1-small_64

echo "Starting sjeng: L2 Big"
zcat traces-5M/sjeng.gz | ./main config/l2-big_8 > Results/sjeng/sjengResults_l2-big_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l2-big_16 > Results/sjeng/sjengResults_l2-big_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l2-big_32 > Results/sjeng/sjengResults_l2-big_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l2-big_64 > Results/sjeng/sjengResults_l2-big_64

echo "Starting sjeng: L1 2-Way"
zcat traces-5M/sjeng.gz | ./main config/l1-2way_8 > Results/sjeng/sjengResults_l1-2way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l1-2way_16 > Results/sjeng/sjengResults_l1-2way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l1-2way_32 > Results/sjeng/sjengResults_l1-2way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l1-2way_64 > Results/sjeng/sjengResults_l1-2way_64

echo "Starting sjeng: L1 8-Way"
zcat traces-5M/sjeng.gz | ./main config/l1-8way_8 > Results/sjeng/sjengResults_l1-8way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l1-8way_16 > Results/sjeng/sjengResults_l1-8way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l1-8way_32 > Results/sjeng/sjengResults_l1-8way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l1-8way_64 > Results/sjeng/sjengResults_l1-8way_64

echo "Starting sjeng: L1 Small, 4-Way"
zcat traces-5M/sjeng.gz | ./main config/l1-small-4way_8 > Results/sjeng/sjengResults_l1-small-4way_8
echo "Finished: 1"
zcat traces-5M/sjeng.gz | ./main config/l1-small-4way_16 > Results/sjeng/sjengResults_l1-small-4way_16
echo "Finished: 2"
zcat traces-5M/sjeng.gz | ./main config/l1-small-4way_32 > Results/sjeng/sjengResults_l1-small-4way_32
echo "Finished: 3"
zcat traces-5M/sjeng.gz | ./main config/l1-small-4way_64 > Results/sjeng/sjengResults_l1-small-4way_64
END=$(date +%s)
DIFF=$(( $END - $START ))
echo "Completed sjeng trace in $DIFF seconds."

TOTALEND=$(date +%s)
DIFF=$(( $TOTALEND - $TOTALSTART ))
echo "It took $DIFF seconds to run all the LONG traces"
