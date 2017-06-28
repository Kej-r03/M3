import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
dtupos = int(os.environ.get('M3_GEM5_DTUPOS', 0))
mmu = int(os.environ.get('M3_GEM5_MMU', 0))
num_spm = 4 if num_pes >= 4 else 4 - num_pes
mem_pe = num_pes

pes = []

# create the core PEs
for i in range(0, num_pes - num_spm):
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='32kB',
                      l2size='256kB',
                      dtupos=dtupos,
                      mmu=mmu == 1)
    pes.append(pe)
for i in range(num_pes - num_spm, num_pes):
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      spmsize='8MB')
    pes.append(pe)

# create the memory PEs
for i in range(0, num_mem):
    pe = createMemPE(noc=root.noc,
                     options=options,
                     no=num_pes + i,
                     size='1024MB',
                     content=fsimg if i == 0 else None)
    pes.append(pe)

# create accelerator PEs
accs = ['hash', 'fft', 'toupper']
for i in range(0, 3):
    pe = createAccelPE(noc=root.noc,
                       options=options,
                       no=num_pes + num_mem + i,
                       accel=accs[i],
                       memPE=mem_pe,
                       spmsize='40kB')
                       #l1size='32kB')
    pes.append(pe)

# pes[1].dtu.watch_range_start  = 0x43d2ff0
# pes[1].dtu.watch_range_end    = 0x43d2fff

runSimulation(root, options, pes)
