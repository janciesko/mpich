
DEVICE=$1
full=$2
if [ x"$full" = x ]; then
  echo "Usage: sh run.sh DEVICE FULL(=0|1)"
  echo "FULL = 0 : All configurations are checked, including OPT_NUM=1 ... 10."
  echo "     = 1 : Representative configurations are checked."
  echo "DEVICE = ucx|ofi|ofisys"
  exit 1
fi

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

${MPICH_PTH_PATH}/bin/mpiexec -n 2 hostname

for repeat in $(seq 10); do
    for comm_type in 0 1 2; do
        echo "# repeat $repeat / 10 (comm_type = $comm_type)"
        if [ x"$comm_type" = x"0" ]; then
            winsize_list="32"
        else
            winsize_list="1000"
        fi
        for winsize in ${winsize_list}; do
            num_entities="16"
            num_messages="500000"
            msgsize_list="1 4 16 64 256 1024 4096 16384 65536"
            for msgsize in $msgsize_list; do
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
                    for OPT_NUM in 1 2 3 4 5 6 7 8 9 10; do
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

echo "completed!"
