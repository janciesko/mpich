import sys

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

for line in open(path):
    if line.startswith("#### "):
        label = line.split(" ")[1].strip()
        if not label in label_list:
            label_list.append(label)
    if line.startswith("Operation Type: "):
        optype = line.split("Operation Type: ")[1].strip()
        if not optype in optype_list:
            optype_list.append(optype)
    if line.startswith("Number of entities: "):
        nentities = line.split("Number of entities: ")[1].strip()
        if not nentities in nentities_list:
            nentities_list.append(nentities)
    if line.startswith("Number of communicators: "):
        ncommunicators = line.split("Number of communicators: ")[1].strip()
        if not ncommunicators in ncommunicators_list:
            ncommunicators_list.append(ncommunicators)
    if line.startswith("Message size: "):
        msgsize = line.split("Message size: ")[1].strip()
        if not msgsize in msgsize_list:
            msgsize_list.append(msgsize)
    if line.startswith("Total message rates: "):
        msgrate = float(line.split("Total message rates: ")[1].split(" ")[0].strip())
        key = label + "_" + nentities + "_" + optype + "_" + msgsize
        if not key in data_dic:
            data_dic[key] = []
        data_dic[key].append(msgrate)
    if line.startswith("Total bandwidth: "):
        msgrate = float(line.split("Total bandwidth: ")[1].split(" ")[0].strip())
        key = label + "_" + nentities + "_" + optype + "_" + msgsize
        if not key in bw_data_dic:
            bw_data_dic[key] = []
        bw_data_dic[key].append(msgrate)

for optype in optype_list:
    if len(nentities_list) > 1:
        for msgsize in msgsize_list:
            print("## Message Rate: " + str(optype) + " msgsize: " + str(msgsize))
            desc = ""
            for label in label_list:
                desc += "\t" + label
            desc += "\n"
            for nentities in nentities_list:
                desc += nentities
                for label in label_list:
                    key = label + "_" + nentities + "_" + optype + "_" + msgsize
                    val = 0
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
