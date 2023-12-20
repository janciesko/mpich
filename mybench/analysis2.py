import sys
import csv
from itertools import chain

if len(sys.argv) != 2:
    print("Usage: python analysis.py PATH")
    exit(-1)

path = sys.argv[1]

data_dic = {}
bw_data_dic = {}
label_list = []
nentities_list = []
ncommunicators_list = []
optype_list = []
msgsize_list = []

csvline_lst = []
csvheader = ','.join(['label',
                      'Operation Type',
                      'Number of entities',
                      'Number of communicators',
                      'Number of vcis',
                      'Message size',
                      'Total message rates',
                      'Total bandwidth'])
#csvline_lst.append(csvheader)
csvline = {}
for line in open(path):
    if line.startswith("#### "):
        if len(csvline) > 0:
            csvline_lst.append(csvline)
            csvline = {}
        label = line.split(" ")[1].strip()
        numvcis = line.split(' ')[-1].strip()
        csvline['label'] = label
        csvline['Number of vcis'] = numvcis
        if not label in label_list:
            label_list.append(label)
    if line.startswith("Operation Type: "):
        optype = line.split("Operation Type: ")[1].strip()
        csvline['Operation Type']  = optype
        if not optype in optype_list:
            optype_list.append(optype)
    if line.startswith("Number of entities: "):
        nentities = line.split("Number of entities: ")[1].strip()
        csvline['Number of entities'] = nentities
        if not nentities in nentities_list:
            nentities_list.append(nentities)
    if line.startswith("Number of communicators: "):
        ncommunicators = line.split("Number of communicators: ")[1].strip()
        csvline['Number of communicators'] = ncommunicators
        if not ncommunicators in ncommunicators_list:
            ncommunicators_list.append(ncommunicators)
    if line.startswith("Message size: "):
        msgsize = line.split("Message size: ")[1].strip()
        csvline['Message size'] =msgsize
        if not msgsize in msgsize_list:
            msgsize_list.append(msgsize)
    if line.startswith("Total message rates: "):
        msgrate = float(line.split("Total message rates: ")[1].split(" ")[0].strip())
        csvline['Total message rates'] =   str(msgrate)
        #key = label + "_" + nentities + "_" + optype + "_" + msgsize
        key = f'{label}_{nentities}_{ncommunicators}_{optype}_{msgsize}'
        if not key in data_dic:
            data_dic[key] = []
        data_dic[key].append(msgrate)
    if line.startswith("Total bandwidth: "):
        msgrate = float(line.split("Total bandwidth: ")[1].split(" ")[0].strip())
        csvline['Total bandwidth'] = str(msgrate)
        #key = label + "_" + nentities + "_" + optype + "_" + msgsize
        key = f'{label}_{nentities}_{ncommunicators}_{optype}_{msgsize}'
        if not key in bw_data_dic:
            bw_data_dic[key] = []
        bw_data_dic[key].append(msgrate)

#print('\n'.join(csvline_lst))
with open('comm_ent_sweep.csv', 'w') as f:
    w = csv.DictWriter(f, fieldnames=csvline_lst[0].keys())
    w.writeheader()
    w.writerows(csvline_lst[1:-1])
     
exit()


for optype in optype_list:
    if len(nentities_list) > 1:
        for ncommunicators in ncommunicators_list:
            for msgsize in msgsize_list:
                print("## Message Rate: " + str(optype) + " msgsize: " + str(msgsize) + ' Communicators: '+ncommunicators)
                desc = ""
                for label in label_list:
                    desc += "\t" + label
                desc += "\n"
                for nentities in nentities_list:
                    desc += nentities
                    for label in label_list:
                        key = f'{label}_{nentities}_{ncommunicators}_{optype}_{msgsize}'
                        val = 'NaN'
                        if key in data_dic and len(data_dic[key]) > 0:
                            val = sum(data_dic[key]) / len(data_dic[key])
                        desc += "\t" + str(val)
                    desc += "\n"
                desc += "\n"
                print(desc)

    if len(msgsize_list) > 1:
        for nentities in nentities_list:
            print("## Bandwidth: " + optype + " nentities: " + str(nentities))
            desc = ""
            for label in label_list:
                desc += "\t" + label
            desc += "\n"
            for msgsize in msgsize_list:
                desc += msgsize
                for label in label_list:
                    key = label + "_" + nentities + "_" + optype + "_" + msgsize
                    val = 0
                    if key in bw_data_dic and len(bw_data_dic[key]) > 0:
                        val = sum(bw_data_dic[key]) / len(bw_data_dic[key])
                    desc += "\t" + str(val)
                desc += "\n"
            desc += "\n"
            print(desc)
