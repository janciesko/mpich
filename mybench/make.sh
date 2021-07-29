
DEVICE=$1
if [ x"$DEVICE" = x ]; then
  echo "Usage: sh make_opt.sh DEVICE"
  echo "DEVICE = ucx | ofi | ofisys"
  exit 1
fi

ABT_PATH="$(pwd)/../argobots/install-fast"
ABT_UNOPT_PATH="$(pwd)/../argobots_unopt/install-fast"
MPICH_PTH_PATH=$(pwd)/../install_${DEVICE}_pth
MPICH_PTHVCI_PATH=$(pwd)/../install_${DEVICE}_pthvci
MPICH_PTHVCIOPT_PATH=$(pwd)/../install_${DEVICE}_pthvciopt
MPICH_ABT_PATH=$(pwd)/../install_${DEVICE}_abt

echo "${MPICH_PTH_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pth.out"
${MPICH_PTH_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pth.out

echo "${MPICH_PTHVCI_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pthvci.out"
${MPICH_PTHVCI_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pthvci.out

echo "${MPICH_PTHVCIOPT_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pthvciopt.out"
${MPICH_PTHVCIOPT_PATH}/bin/mpicc pt2pt_msgrate.c -lpthread -DUSE_PTHREADS -O3 -o ${DEVICE}_pthvciopt.out

echo "${MPICH_ABT_PATH}/bin/mpicc -L${ABT_PATH}/lib -I${ABT_PATH}/include -Wl,-rpath=${ABT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt.out"
${MPICH_ABT_PATH}/bin/mpicc -L${ABT_PATH}/lib -I${ABT_PATH}/include -Wl,-rpath=${ABT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt.out

for i in 1 2 3 4 5 6 7 10; do
  if [ -f ${MPICH_ABT_PATH}_${i}/bin/mpicc ]; then
    echo "${MPICH_ABT_PATH}_${i}/bin/mpicc -L${ABT_PATH}/lib -I${ABT_PATH}/include -Wl,-rpath=${ABT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt_${i}.out"
    ${MPICH_ABT_PATH}_${i}/bin/mpicc -L${ABT_PATH}/lib -I${ABT_PATH}/include -Wl,-rpath=${ABT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt_${i}.out
  fi
done
for i in 8 9; do
  if [ -f ${MPICH_ABT_PATH}_${i}/bin/mpicc ]; then
    echo "${MPICH_ABT_PATH}_${i}/bin/mpicc -L${ABT_UNOPT_PATH}/lib -I${ABT_UNOPT_PATH}/include -Wl,-rpath=${ABT_UNOPT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt_${i}.out"
    ${MPICH_ABT_PATH}_${i}/bin/mpicc -L${ABT_UNOPT_PATH}/lib -I${ABT_UNOPT_PATH}/include -Wl,-rpath=${ABT_UNOPT_PATH}/lib pt2pt_msgrate.c -lpthread -DUSE_ARGOBOTS -labt -O3 -o ${DEVICE}_abt_${i}.out
  fi
done
