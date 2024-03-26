DEVICE=$1
full=$2
LOCK_VCI=$3
if [ x"$full" = x ]; then
  echo "Usage: sh run.sh DEVICE FULL(=0|1)"
  echo "FULL = 0 : All configurations are checked, including OPT_NUM=1 ... 10."
  echo "     = 1 : Representative configurations are checked."
  echo "DEVICE = ucx | ofi | ofisys"
  exit 1
fi

MPICH_PTH_PATH=$(pwd)/../install_${DEVICE}_pth
MPICH_PTHVCI_PATH=$(pwd)/../install_${DEVICE}_pthvci
MPICH_PTHVCIOPT_PATH=$(pwd)/../install_${DEVICE}_pthvciopt
MPICH_ABT_PATH=$(pwd)/../install_${DEVICE}_abt
TIMEOUT="timeout -s 9 600"
#BIND_NUMA_CTL="numactl -m 0 --cpunodebind 0"
BIND_NUMA_CTL="numactl --interleave=all"
#BIND_MPICH="--map-by socket --bind-to core"
export NUM_REPEATS=5

HARD_SET_VCI=$LOCK_VCI
HARD_SET_VCI_VALUE=1

HASH=`date|md5sum|head -c 5`

ENVMSG=""
## Machine-specific setting
# if [ x"${DEVICE}" = x"ofi" ]; then
#   # bebop
#   export HFI_NO_CPUAFFINITY=1
#   ENVMSG="HFI_NO_CPUAFFINITY=$HFI_NO_CPUAFFINITY "
# elif [ x"${DEVICE}" = x"ofisys" ]; then
#   # spock
#   export FI_PROVIDER=verbs
#   export MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=1
#   export MPIR_CVAR_CH4_OFI_ENABLE_RMA=1
#   ENVMSG="FI_PROVIDER=$FI_PROVIDER MPIR_CVAR_CH4_OFI_ENABLE_TAGGED=$MPIR_CVAR_CH4_OFI_ENABLE_TAGGED MPIR_CVAR_CH4_OFI_ENABLE_RMA=$MPIR_CVAR_CH4_OFI_ENABLE_RMA "
# else
#   # gomez
#   ENVMSG=""
# fi

USE_GLOBAL_PROGRESS="0"


NUM_MESSAGES="8000"
NUM_MESSAGES_SMALL="8000"

echo " ./${DEVICE}_pth.out"

FILENAME_ALL="mybench_${DEVICE}_${HASH}.res"
echo "threading,API,num_msg,msg_size,win_size,entities,comms,iters,msg_rate,bw" > $FILENAME_ALL 


for comm_type in 0 1 2; do
    
    FILENAME="mybench_${DEVICE}_${HASH}_${comm_type}.res"
    
    echo $FILENAME
    echo "threading,API,num_msg,msg_size,win_size,entities,comms,iters,msg_rate,bw" | tee $FILENAME 

    #|====================|
    #|======MPI + ABT ====|
    #|====================|

    echo " ./${DEVICE}_pth.out, MPIR_CVAR_CH4_NUM_VCIS, ABT"


    for reps in 0 1 2; do
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="32"
        fi
        for winsize in ${winsize_list}; do
            num_entities_list="1 2 4 6 8 12 16"
            num_messages=${NUM_MESSAGES}
            msgsize=1
            for num_entities in ${num_entities_list}; do
                num_messages_sp=$num_messages
                if [ x"$HARD_SET_VCI" = x"1" ]; then
                NUM_VCIs=${HARD_SET_VCI_VALUE}
                else
                NUM_VCIs=$((${num_entities} + 2))
                fi
                if [ x"$num_entities" != x"1" ]; then
                    num_messages_sp=${NUM_MESSAGES_SMALL}
                fi
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_GLOBAL_PROGRESS=${USE_GLOBAL_PROGRESS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=${num_entities} MPIR_CVAR_CH4_NUM_VCIS=${NUM_VCIs} ${TIMEOUT} \
                ${MPICH_ABT_PATH}/bin/mpiexec ${BIND_MPICH} -n 2 ${BIND_NUMA_CTL} ./${DEVICE}_abt.out 1 ${num_entities} ${num_entities} ${num_messages} ${winsize} ${msgsize} | tee -a ${FILENAME}
            done
        done
    done

    #dump into common file
    tail -n +2 -q  $FILENAME >> $FILENAME_ALL 

    #|====================|
    #|======MPI + PTH ====|
    #|====================|

    echo " ./${DEVICE}_pth.out"

    for reps in 0 1 2; do
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="1000"
        fi
        for winsize in ${winsize_list}; do
            num_entities_list="1 2 4 6 8 12 16"
            num_messages=${NUM_MESSAGES}
            msgsize=1
            for num_entities in ${num_entities_list}; do
                if [ x"$HARD_SET_VCI" = x"1" ]; then
                NUM_VCIs=${HARD_SET_VCI_VALUE}
                else
                NUM_VCIs=$((${num_entities} + 2))
                fi
                #NUM_VCIs=1
                num_messages_sp=$num_messages
                if [ x"$num_entities" != x"1" ]; then
                    num_messages_sp=${NUM_MESSAGES_SMALL}
                fi
            COMM_TYPE=${comm_type} MPIR_CVAR_CH4_GLOBAL_PROGRESS=${USE_GLOBAL_PROGRESS} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=${NUM_VCIs} NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec ${BIND_MPICH} -n 2 ${BIND_NUMA_CTL} ./${DEVICE}_pthvciopt.out 1 ${num_entities} ${num_entities} ${num_messages} ${winsize} ${msgsize} | tee -a ${FILENAME}
            done
        done
    done

    #dump into common file
    tail -n +2 -q  $FILENAME >> $FILENAME_ALL 

    #|===========================|
    #|======MPI + PTH NOLOCK ====|
    #|===========================|

    echo " ./${DEVICE}_pth.out, NOLOCK"

    for reps in 0 1 2; do
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="1000"
        fi
        for winsize in ${winsize_list}; do
            num_entities_list="1 2 4 6 8 12 16"
            num_messages=${NUM_MESSAGES}
            msgsize=1
            for num_entities in ${num_entities_list}; do
                if [ x"$HARD_SET_VCI" = x"1" ]; then
                NUM_VCIs=${HARD_SET_VCI_VALUE}
                else
                NUM_VCIs=$((${num_entities} + 2))
                fi
                num_messages_sp=$num_messages
                if [ x"$num_entities" != x"1" ]; then
                    num_messages_sp=${NUM_MESSAGES_SMALL}
                fi
                COMM_TYPE=${comm_type} MPIR_CVAR_CH4_GLOBAL_PROGRESS=${USE_GLOBAL_PROGRESS} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=${NUM_VCIs} NOLOCK=1 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec ${BIND_MPICH} -n 2 ${BIND_NUMA_CTL} ./${DEVICE}_pthvciopt.out 1 ${num_entities} ${num_entities} ${num_messages} ${winsize} ${msgsize} | tee -a ${FILENAME}
            done
        done
    done

    #dump into common file
    tail -n +2 -q  $FILENAME >> $FILENAME_ALL 

    #|=========================|
    #|======MPI Everywhere ====|
    #|=========================|

    echo " ./${DEVICE}_pth.out"

    for reps in 0 1 2; do
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="1000"
        fi
        for winsize in ${winsize_list}; do
            num_entities_list="1 2 4 6 8 12 16"
            num_messages=${NUM_MESSAGES}
            msgsize=1
            for num_entities in ${num_entities_list}; do
                num_messages_sp=$num_messages
                if [ x"$num_entities" != x"1" ]; then
                    num_messages_sp=${NUM_MESSAGES_SMALL}
                fi
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec ${BIND_MPICH} -n $((${num_entities} * 2)) ${BIND_NUMA_CTL} ./${DEVICE}_pth.out 0 ${num_entities} 1 ${num_messages} ${winsize} ${msgsize} | tee -a ${FILENAME}
            done
        done
    done

    #dump into common file
    tail -n +2 -q  $FILENAME >> $FILENAME_ALL 

done

