#!/bin/bash

#SBATCH -N 1
#SBATCH -J dk.ent16
#SBATCH --account=FY140001
##SBATCH -p all 
#SBATCH -t 12:00:00
#SBATCH --output=Rent16-1node-%x.%j.out

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
echo "Num nodes            : $nodes"
echo "current working dir  : $topdir"
echo "results dir          : $resultsdir"
echo "device               : $DEVICE"
echo "full                 : $full"
which mpicc
lscpu
date +"%F_%T"

echo "sh dkruse.run_ent16.sh $DEVICE $full"
sh dkruse.run_ent16.sh $DEVICE $full

#echo "sh dkruse.run_msg1.sh $DEVICE $full"
#sh dkruse.run_msg1.sh $DEVICE $full

#echo "sh run_ent16.sh $DEVICE $full"
#sh run_ent16.sh $DEVICE $full
#
#echo "sh run_msg1.sh $DEVICE $full"
#sh run_msg1.sh $DEVICE $full



echo "End :: Blake $name"
date +"%F_%T"
