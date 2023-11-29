import numpy as np 
import matplotlib.pyplot as plt
import sys

numents = [ n for n in range(1,17) ]
msgsizes = [1, 4, 16, 64, 256, 1024, 4096, 16384, 65536]
msgsizeslabels = ['1B', '4B', '16B', '64B', '256B', '1KiB', '4KiB', '16KiB', '64KiB']
#cats = [
#    'proc',
#    'pth_novci_1comm',
#    'pth_vciopt_Ncomm',
#    'pth_vciopt_Ncomm_nolock',
#    'abt_vci_1comm_Nes',
#    'abt_vci_Ncomm_Nes'
#]
#catmarker = {
#    'proc'                    : 'kx--',
#    'pth_novci_1comm'         : 'o-',
#    'pth_vciopt_Ncomm'        : 'o-',
#    'pth_vciopt_Ncomm_nolock' : 's--',
#    'abt_vci_1comm_Nes'       : 'D-',
#    'abt_vci_Ncomm_Nes'       : '*-'
#}

cats = [
]
catmarker = {
    'proc'                    : 'kx--',
    'pth_novci_1comm'         : 'o--',
    'pth_vciopt_Ncomm'        : 'o--',
    'pth_vciopt_Ncomm_nolock' : 'o--',
    'abt_vci_1comm_Nes'       : 'D--',
    'abt_vci_Ncomm_Nes'       : '*--'
}


def message_rate_plot(msgsize, mr_ydatas):
    #'abt_vci_9_Ncomm_Nes'
]
catlegend = {
    'proc'                    : 'mpi everywhere',
    'pth_novci_1comm'         : 'pthreads no vci 1 comm',
    'pth_vciopt_Ncomm'        : 'pthreads optimized vci N comms',
    'pth_vciopt_Ncomm_nolock' : 'pthreads optimized vci N comms no lock',
    'abt_vci_1comm_Nes'       : 'argobots vci 1 comm N entities',
    'abt_vci_Ncomm_Nes'       : 'argobots vci N comms N entities'
    #'abt_vci_9_Ncomm_Nes'     : 'argobots vci N comms N entities OPT=9'
}
catmarker = {
    'proc'                    : 'kx--',
    'pth_novci_1comm'         : 'o-',
    'pth_vciopt_Ncomm'        : 'o-',
    'pth_vciopt_Ncomm_nolock' : 's--',
    'abt_vci_1comm_Nes'       : 'D-',
    'abt_vci_Ncomm_Nes'       : '*-'
    #'abt_vci_9_Ncomm_Nes'     : '2-' 
}


def message_rate_plot(msgsize, mr_ydatas, machine=None):
        plt.plot(x, y, catmarker[cat], label=cat,)
        plt.plot(x, y, catmarker[cat], label=catlegend[cat],)
            

        
    # Add some text for labels, title and custom x-axis tick labels, etc.
    plt.xticks(x, numents)
    plt.grid()
    ax.set_xlabel('Number of Entities')
    ax.set_ylabel('Message Rate (Million Messages/sec)')
    if machine:
       title = f'{machine} Message Rate for message size of {msgsize} Byte(s)'
    else:
       title = f'Message Rate for message size of {msgsize} Byte(s)'
    ax.set_title(title)
    ax.legend(loc='upper left', ncols=1)
    #ax.set_ylim(0, 250)
    #plt.show()
    if machine:
        plt.savefig(f'{machine}-message_rate_{msgsize}_bytes.pdf', format='pdf')
    else:
        plt.savefig(f'message_rate_{msgsize}_bytes.pdf', format='pdf')


        

def bandwidth_plot(nentities, bw_ydatas, machine=None):

    x = np.arange(1, len(msgsizes)+1)
    ys = np.array( [ np.array(y) for y in bw_ydatas] )
    ys = ys.astype(float)
    print(ys)
    

    fig, ax = plt.subplots()

    for i,cat in enumerate(cats):
        y = ys[:,i+1] 
        plt.plot(x, y, catmarker[cat], label=catlegend[cat])

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_xticks(x, labels=msgsizes)
    plt.grid()


    #plt.yscale('log', base=10)
    ax.set_xlabel('Message Size (Bytes)')
    ax.set_ylabel('Bandwidth (MB/s)')
    if machine:
       title = f'{machine} Bandwidth, {nentities} Entities'
    else:
       title = f'Bandwidth, {nentities} Entities'
    ax.set_title(title)
    ax.legend(loc='upper left', ncols=1)
    #ax.set_ylim(0, 250)
    #plt.show()
    if machine:
        plt.savefig(f'{machine}-bandwidth_{nentities}_ents.pdf', format='pdf')
    else:
        plt.savefig(f'bandwidth_{nentities}_ents.pdf', format='pdf')


    
def main():
    print('numents')
    print(numents)
    print('msgsizes')
    print(msgsizes)
    print('cats')
    print(cats)
    #machine='Stria'
    machine='Blake'

    #filename = 'data/stria/stria-results.msgrate.txt'
    filename = 'data/blake/16ents.message_rate.txt'
    with open(filename) as fin:
        for line in fin:
            if line.startswith('\n'):
                continue

            if line.startswith('##'):
                mr_ydatas = []
                msgsize = line.split()[-1]
                continue

            if line.startswith('ents'):
                continue

            if line.startswith('END'):
            else:
                # get the data
                data = line.split(',')
                mr_ydatas.append(data)
                


    #filename = 'data/stria/stria-results.bandwidth.txt'
    filename = 'data/blake/16ents.bandwidth.txt'
    with open(filename) as fin:
        for line in fin:
            if line.startswith('\n'):
                continue

            if line.startswith('##'):
                bw_ydatas = []
                nentities = line.split()[-1]
                continue

            if line.startswith('msgsize'):
                continue

            if line.startswith('END'):
                bandwidth_plot(nentities, bw_ydatas, machine=machine)
            else:
                # get the data
                data = line.split(',')
                bw_ydatas.append(data)
                
                



if __name__ == "__main__":
    main()


