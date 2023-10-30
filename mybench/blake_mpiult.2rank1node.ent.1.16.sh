#!/bin/bash

#SBATCH -N 1
#SBATCH -J ent1.16.2r1n
#SBATCH --account=FY140001
##SBATCH -p all 
#SBATCH -t 12:00:00
#SBATCH --output=Rent1to16-2rank1node-trial-%x.%j.out


TRIAL=$1
if [[ -z $TRIAL ]]; then
  echo "Usage: sh blake_mpiult.2rank1node.ent.1.to.16.sh TRIAL"
  echo "TRIAL = 0,1,2,... : The trial to run for dkruse.run_ent1to16.2rank.sh"
  exit 1
fi

d=$(date +"%F")
j=${SLURM_JOBID}
nodes=$(SLURM_NNODES)
nodelist=$(SLURM_NODELIST)
name=$(SLURM_JOB_NAME)
topdir=$(pwd)

resultsdir="results/blake"
mkdir -p $resultsdir

DEVICE="ucx"
full="0"

echo "Starting :: Blake $name"
echo "JobID                : $j"
echo "TRIAL                : $TRIAL"
echo "Num nodes            : $nodes"
echo "current working dir  : $topdir"
echo "results dir          : $resultsdir"
echo "device               : $DEVICE"
echo "full                 : $full"
which mpicc
lscpu
date +"%F_%T"


#echo "sh dkruse.run_ent1.to.16.2rank.sh $DEVICE $full $TRIAL"
#sh dkruse.run_ent16.2rank.sh $DEVICE $full $TRIAL

echo "sh dkruse.run_ent1.to.16.2rank.sh $DEVICE $full $TRIAL"
sh dkruse.run_ent1.to.16.2rank.sh $DEVICE $full $TRIAL



echo "End :: Blake $name trail $TRIAL"
date +"%F_%T"
