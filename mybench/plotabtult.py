import numpy as np 
import matplotlib.pyplot as plt


numents = [ n for n in range(1,17) ]
msgsizes = [1, 4, 16, 64, 256, 1024, 4096, 16384, 65536]
cats = [
    'proc',
    'pth_novci_1comm',
    'pth_vciopt_Ncomm',
    'pth_vciopt_Ncomm_nolock',
    'abt_vci_1comm_Nes',
    'abt_vci_Ncomm_Nes'
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

    x = np.arange(1,len(numents)+1)
    ys = np.array( [ np.array(y) for y in mr_ydatas] )
    ys = ys.astype(float)
    print(ys)

    fig, ax = plt.subplots()

    for i,cat in enumerate(cats):
        y = ys[:,i+1]
        plt.plot(x, y, catmarker[cat], label=cat,)
            

        
    # Add some text for labels, title and custom x-axis tick labels, etc.
    plt.xticks(x, numents)
    plt.grid()
    ax.set_xlabel('Num Entities')
    ax.set_ylabel('Message Rate (Messages/sec)')
    ax.set_title(f'Message Rate for message size of {msgsize} Byte(s)')
    ax.legend(loc='upper left', ncols=1)
    #ax.set_ylim(0, 250)
    #plt.show()

    plt.savefig(f'message_rate_{msgsize}_bytes.pdf', format='pdf')


def bandwidth_plot(nentities, bw_ydatas):

    x = msgsizes
    ys = np.array( [ np.array(y) for y in bw_ydatas] )
    ys = ys.astype(float)
    print(ys)
    

    fig, ax = plt.subplots()

    for i,cat in enumerate(cats):
        y = ys[:,i+1] 
        plt.plot(x, y, catmarker[cat], label=cat,)

        
    # Add some text for labels, title and custom x-axis tick labels, etc.
    plt.grid()
    plt.xscale('log', base=2)
    #plt.yscale('log', base=10)
    ax.set_xlabel('Message Size (Bytes)')
    ax.set_ylabel('Bandwidth (MB/s)')
    ax.set_title(f'Bandwidth, {nentities} Entities')
    ax.legend(loc='upper left', ncols=1)
    #ax.set_ylim(0, 250)
    #plt.show()
    plt.savefig(f'bandwidth_{nentities}_ents.pdf', format='pdf')


    
def main():
    print('numents')
    print(numents)
    print('msgsizes')
    print(msgsizes)
    print('cats')
    print(cats)

    filename = '16ents.message_rate.txt'
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
                message_rate_plot(msgsize, mr_ydatas)
            else:
                # get the data
                data = line.split(',')
                mr_ydatas.append(data)
                


    filename = '16ents.bandwidth.txt'
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
                bandwidth_plot(nentities, bw_ydatas)
            else:
                # get the data
                data = line.split(',')
                bw_ydatas.append(data)
                
                



if __name__ == "__main__":
    main()


