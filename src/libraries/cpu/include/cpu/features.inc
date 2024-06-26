
CPU_FEATURE_OPT(pcid,       0x00000001, 0, ecx, 17)
CPU_FEATURE_OPT(x2apic,     0x00000001, 0, ecx, 21)
CPU_FEATURE_REQ(xsave,      0x00000001, 0, ecx, 26)
CPU_FEATURE_OPT(hypervisor, 0x00000001, 0, ecx, 31)

CPU_FEATURE_REQ(fpu,        0x00000001, 0, edx,  0)
CPU_FEATURE_REQ(pse,        0x00000001, 0, edx,  3)
CPU_FEATURE_OPT(tsc,        0x00000001, 0, edx,  4)
CPU_FEATURE_REQ(msr,        0x00000001, 0, edx,  5)
CPU_FEATURE_REQ(apic,       0x00000001, 0, edx,  9)
CPU_FEATURE_REQ(mtrr,       0x00000001, 0, edx, 12)
CPU_FEATURE_REQ(pge,        0x00000001, 0, edx, 13)
CPU_FEATURE_REQ(pat,        0x00000001, 0, edx, 16)
CPU_FEATURE_REQ(fxsr,       0x00000001, 0, edx, 24)

CPU_FEATURE_OPT(fsgsbase,   0x00000007, 0, ebx,  0)
CPU_FEATURE_OPT(bmi1,       0x00000007, 0, ebx,  3)
CPU_FEATURE_OPT(invpcid,    0x00000007, 0, ebx, 10)

CPU_FEATURE_OPT(pku,        0x00000007, 0, ecx,  3)
CPU_FEATURE_OPT(rdpid,      0x00000007, 0, ecx, 22)

CPU_FEATURE_OPT(xsaveopt,   0x0000000d, 1, eax,  0)
CPU_FEATURE_OPT(xsavec,     0x0000000d, 1, eax,  1)
CPU_FEATURE_OPT(xinuse,     0x0000000d, 1, eax,  2)
CPU_FEATURE_OPT(xsaves,     0x0000000d, 1, eax,  3)

CPU_FEATURE_OPT(extapic,    0x80000001, 0, ecx,  3)

CPU_FEATURE_OPT(rdtscp,     0x80000001, 0, edx, 27)

CPU_FEATURE_REQ(syscall,    0x80000001, 0, edx, 11)
CPU_FEATURE_REQ(pdpe1gb,    0x80000001, 0, edx, 26)

CPU_FEATURE_WRN(invtsc,     0x80000007, 0, edx,  8)

// vim: et
