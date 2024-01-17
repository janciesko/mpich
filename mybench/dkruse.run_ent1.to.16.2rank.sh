
DEVICE=$1
full=$2
TRIAL=$3
if [ x"$TRIAL" = x ]; then
  echo "Usage: sh dkruse.run_ent1.to.16.2rank.sh DEVICE FULL(=0|1) TRIAL"
  echo "FULL = 0 : All configurations are checked, including OPT_NUM=1 ... 10."
  echo "     = 1 : Representative configurations are checked."
  echo "DEVICE = ucx|ofi|ofisys"
  echo "TRIAL = 0,1,2,... : The trial number (sample number)"
  exit 1
fi

echo "export MPIR_CVAR_CH4_GLOBAL_PROGRESS=0"
export MPIR_CVAR_CH4_GLOBAL_PROGRESS=0

MPICH_PTH_PATH=$(pwd)/../install_${DEVICE}_pth
MPICH_PTHVCI_PATH=$(pwd)/../install_${DEVICE}_pthvci
MPICH_PTHVCIOPT_PATH=$(pwd)/../install_${DEVICE}_pthvciopt
MPICH_ABT_PATH=$(pwd)/../install_${DEVICE}_abt
TIMEOUT="timeout -s 9 600"
#BIND="numactl -m 0 --cpunodebind 0"
export NUM_REPEATS=5

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

date

# Blake 'All' partition lscpu:
#Architecture:        x86_64
#CPU op-mode(s):      32-bit, 64-bit
#Byte Order:          Little Endian
#CPU(s):              192
#On-line CPU(s) list: 0-191
#Thread(s) per core:  2
#Core(s) per socket:  48
#Socket(s):           2
#NUMA node(s):        2
#Vendor ID:           GenuineIntel
#CPU family:          6
#Model:               143
#Model name:          Intel(R) Xeon(R) Platinum 8468
#Stepping:            8
#CPU MHz:             3800.000
#CPU max MHz:         3800.0000
#CPU min MHz:         800.0000
#BogoMIPS:            4200.00
#Virtualization:      VT-x
#L1d cache:           48K
#L1i cache:           32K
#L2 cache:            2048K
#L3 cache:            107520K
#NUMA node0 CPU(s):   0-47,96-143
#NUMA node1 CPU(s):   48-95,144-191

${MPICH_PTH_PATH}/bin/mpiexec -n 2 hostname

#for repeat in $(seq 10); do
#for repeat in $(seq 1); do
for repeat in "$TRIAL"; do
        #for comm_type in 0 1 2; do
        # COMMS 1 and 2 (put and get) are broken
    for comm_type in 0; do
        echo "# repeat $repeat / 10 (comm_type = $comm_type)"
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="1000"
        fi
        for winsize in ${winsize_list}; do
            num_entities_list="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
            num_messages="500000"
            msgsize_list="1 4 16 64 256 1024 4096 16384 65536"
            for msgsize in $msgsize_list; do
                for num_entities in $num_entities_list; do 
                num_messages_sp=$num_messages
                if [ x"$num_entities" != x"1" ]; then
                    num_messages_sp="50000"
                fi

                date
                echo "#### proc $num_messages $num_entities $winsize $msgsize $comm_type"
                echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n $(($num_entities * 2)) ${BIND} ./${DEVICE}_pth.out 0 $num_entities 1 $num_messages $winsize $msgsize"
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n $(($num_entities * 2)) ${BIND} ./${DEVICE}_pth.out 0 $num_entities 1 $num_messages $winsize $msgsize

                date
                echo "#### pth_novci_1comm $num_messages_sp $num_entities $winsize $msgsize $comm_type"
                echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pth.out 1 $num_entities 1 $num_messages_sp $winsize $msgsize"
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pth.out 1 $num_entities 1 $num_messages_sp $winsize $msgsize

                if [ x"$full" != x0 ]; then
                    date
                    echo "#### pth_novci_Ncomm $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pth.out 1 $num_entities $num_entities $num_messages_sp $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ${TIMEOUT} ${MPICH_PTH_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pth.out 1 $num_entities $num_entities $num_messages_sp $winsize $msgsize

                    date
                    echo "#### pth_vci_1comm $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities 1 $num_messages_sp $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities 1 $num_messages_sp $winsize $msgsize

                    date
                    echo "#### pth_vci_Ncomm $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities $num_entities $num_messages $winsize $msgsize

                    date
                    echo "#### pth_vci_Ncomm_nolock_1comm $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=1 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=1 ${TIMEOUT} ${MPICH_PTHVCI_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvci.out 1 $num_entities $num_entities $num_messages $winsize $msgsize
                fi

                date
                echo "#### pth_vciopt_Ncomm $num_messages $num_entities $winsize $msgsize $comm_type"
                echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvciopt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=0 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvciopt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize

                date
                echo "#### pth_vciopt_Ncomm_nolock $num_messages $num_entities $winsize $msgsize $comm_type"
                echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=1 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvciopt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) NOLOCK=1 ${TIMEOUT} ${MPICH_PTHVCIOPT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_pthvciopt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize

                if [ x"$comm_type" = x"0" ]; then
                    date
                    echo "#### abt_vci_1comm_Nes $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities 1 $num_messages $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities 1 $num_messages $winsize $msgsize
                fi

                if [ x"$full" != x0 ]; then
                    date
                    echo "#### abt_vci_1comm_1es $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=1 MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities 1 $num_messages $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=1 MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities 1 $num_messages $winsize $msgsize

                    date
                    echo "#### abt_vci_Ncomm_1es $num_messages $num_entities $winsize $msgsize $comm_type"
                    echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=1 MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                    COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=1 MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize
                fi

                date
                echo "#### abt_vci_Ncomm_Nes $num_messages $num_entities $winsize $msgsize $comm_type"
                echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt.out 1 $num_entities $num_entities $num_messages $winsize $msgsize

                if [ x"$full" != x0 ]; then
                    #for OPT_NUM in 1 2 3 4 5 6 7 8 9 10; do
                    for OPT_NUM in 9 ; do
                        date
                        echo "#### abt_vci_${OPT_NUM}_Ncomm_Nes $num_messages $num_entities $winsize $msgsize $comm_type"
                        echo "${ENVMSG}COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}_${OPT_NUM}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt_${OPT_NUM}.out 1 $num_entities $num_entities $num_messages $winsize $msgsize"
                        COMM_TYPE=${comm_type} NUM_REPEATS=${NUM_REPEATS} ABT_MEM_LP_ALLOC=mmap_rp ABT_NUM_XSTREAMS=$num_entities MPIR_CVAR_CH4_NUM_VCIS=$(($num_entities + 2)) ${TIMEOUT} ${MPICH_ABT_PATH}_${OPT_NUM}/bin/mpiexec -n 2 ${BIND} ./${DEVICE}_abt_${OPT_NUM}.out 1 $num_entities $num_entities $num_messages $winsize $msgsize
                    done
                fi
                done
            done
        done
    done
done

echo "completed!"
