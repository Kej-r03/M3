def build(gen, env):
    env.m3_rust_exe(gen, out = 'ruststdtest', std = True)