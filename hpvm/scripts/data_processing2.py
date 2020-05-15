#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt

cost_by_pmc = pd.read_csv(
    'results.csv',
    index_col=['program', 'mode', 'core'],
)
cost_by_pmc['traffic'] = cost_by_pmc['load traffic'] + cost_by_pmc['store traffic']
cost_by_pm = cost_by_pmc.sum(level=['program', 'mode'])

cost_by_p_normed = cost_by_pm / cost_by_pm.xs('MESI', level='mode', drop_level=True)

s = cost_by_p_normed.unstack().plot(kind='bar', y='avg. load latency', rot=0, title='Avg. load latency\n(normalized to MESI)')
s.legend(loc='lower left')
plt.savefig('loads.png')

cost_by_p_normed.unstack().plot(kind='bar', y='avg. store latency', rot=0, title='Avg. store latency\n(normalized to MESI)')
plt.savefig('stores.png')

cost_by_p_normed.unstack().plot(kind='bar', y='traffic', rot=0, title='NoC flits routed\n(normalized to MESI)')
plt.savefig('traffic.png')

# from plot_clustered_stacked import plot_clustered_stacked
# levels = cost_by_p_normed.index.levels[1]
# dfs = [cost_by_p_normed.loc[level, ['load traffic', 'store traffic']] for level in levels]
# plot_clustered_stacked(dfs, levels, title='NoC Traffic')
# plt.savefig('traffic.png')
