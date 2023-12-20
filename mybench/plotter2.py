import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def plotCombos(df, groupcols, plottype='bw'):
    groups = df.groupby(by=groupcols)
    figure = 0
    for name, group in groups:
        ncomms = name[0]
        nvcis = name[1]
        nents = name[2]
    
        labels = group['label'].unique()
        entities = group['Number of entities'].unique()
         
        fig = plt.figure(figure)
        ax = plt.gca()
        titlestr = f'N Comms: {ncomms}, N VCIs: {nvcis}, N Entities: {nents}'
        for label in labels:
            #if not label.startswith('abt'):
            #    continue
            for numents in entities:
                #labelstr = f'{label}: {numents} entities, {numcomms} communicators'
                labelstr = f'{label}'
                #mask = (group['label'] == label) & (group['Number of entities'] == numents)
                mask = (group['label'] == label) 
                dftmp = group[mask]
                if dftmp.empty:
                    print(f'{labelstr} IS EMPTY')
                    continue
                means = dftmp.groupby(['Message size'], as_index=False).mean(numeric_only=True)
                errs = dftmp.groupby(['Message size'], as_index=False).std(numeric_only=True)
                print(labelstr)
                print(means.to_string())
                print('\n\n')
    

                x = means['Message size']
                xlabel = 'Message size (Bytes)'

                if plottype == 'bw':
                    metric = 'Total bandwidth'
                    ylabel = 'Bandwidth (MB/s)'
                else:
                    metric = 'Total message rates'
                    ylabel = 'Millions of Messages per Second'

                y    = means[metric]
                yerr = errs[metric]
                plt.errorbar(x,
                             y,
                             capsize=6,
                             yerr=yerr,
                             #color=color,
                             label=labelstr,
                             #marker=markers[i% (len(markers))],
                             #markerfacecolor='none'
                             )
                ax.set_yscale('log')
                ax.set_xscale('log')
                plt.legend()
                plt.ylabel(ylabel)
                plt.xlabel(xlabel)
        plt.title(titlestr)
    
        figure += 1
    
    plt.show()


def plotENTSxMR(df, groupcols, plottype=None):
    print(df['Number of vcis'].unique())
    exit()
    groups = df.groupby(by=groupcols)
    figure = 0
    for name, group in groups:
        ncomms = name[0]
        nvcis = name[1]
    
        labels = group['label'].unique()
        msgsizes = group['Message size'].unique()
         
        fig = plt.figure(figure)
        ax = plt.gca()
        for label in labels:
            #if not label.startswith('abt'):
            #    continue
            for msgsize in msgsizes:
                titlestr = f'N Comms: {ncomms}, N VCIs: {nvcis}, Message Size: {msgsize} B'
                labelstr = f'{label}'

                mask = (group['label'] == label) 
                dftmp = group[mask]
                if dftmp.empty:
                    print(f'{labelstr} IS EMPTY')
                    continue
                means = dftmp.groupby(['Number of entities'], as_index=False).mean(numeric_only=True)
                errs  = dftmp.groupby(['Number of entities'], as_index=False).std(numeric_only=True)
                print(labelstr)
                print(means.to_string())
                print('\n\n')
    

                x = means['Number of entities']
                xlabel = 'Number of entities'

                #if plottype == 'bw':
                if False:
                    metric = 'Total bandwidth'
                    ylabel = 'Bandwidth (MB/s)'
                else:
                    metric = 'Total message rates'
                    ylabel = 'Millions of Messages per Second'

                y    = means[metric]
                yerr = errs[metric]
                plt.errorbar(x,
                             y,
                             capsize=6,
                             yerr=yerr,
                             #color=color,
                             label=labelstr,
                             #marker=markers[i% (len(markers))],
                             #markerfacecolor='none'
                             )
                ax.set_yscale('log')
                plt.legend()
                plt.ylabel(ylabel)
                plt.xlabel(xlabel)
        plt.title(titlestr)
    
        figure += 1
    
    plt.show()
    
            

csvheader = ['label',
             'Operation Type',
             'Number of entities',
             'Number of communicators',
             'Number of vcis',
             'Message size',
             'Total message rates',
             'Total bandwidth']

df = pd.read_csv('comm_ent_sweep.csv')




## x := message size, y : bandwidth
x = df['Message size'].unique()
x = np.sort(x)
labels = df['label'].unique()

dfclean = df.drop(columns=['Operation Type'])
dfclean = dfclean.dropna()
dfclean = dfclean.astype({'Number of entities': 'int64',
                     'Number of communicators': 'int64',
                     'Message size': 'int64',
                     'Number of vcis': 'int64'
                   }, copy=True)
print(dfclean.dtypes)
print(dfclean.head().to_string())



#### group by the label, bandwidth, num comms
groupcols = ['Number of communicators', 'Number of vcis', 'Number of entities']
#plotCombos(dfclean.copy(), groupcols)
#plotCombos(dfclean.copy(), groupcols, plottype='mr')


groupcols = ['Number of communicators', 'Number of vcis', 'Message size']
plotENTSxMR(dfclean.copy(), groupcols)
