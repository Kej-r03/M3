def build(gen, env):
    env.m3_exe(gen, out = 'unittests', ins = ['unittests.cc'] + env.glob('tests/*.cc'))