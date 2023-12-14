def build(gen, env):
    env.m3_exe(gen, out='linecount', ins=['linecount.cc'])