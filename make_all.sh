#!/bin/bash

bash make.sh opt pth ucx 32
bash make.sh opt pthvci ucx 32
bash make.sh opt pthvciopt ucx 32
OPT_NUM=8 bash make.sh opt abt ucx 32

