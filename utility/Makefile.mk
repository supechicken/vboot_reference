
dumpRSAPublicKey-y += dumpRSAPublicKey.c
load_kernel_test-y += load_kernel_test.c
pad_digest_utility-y += pad_digest_utility.c
signature_digest_utility-y += signature_digest_utility.c
verify_data-y += verify_data.c
tpmc-y += tpmc.c

ifneq ($(filter-out 0,${USE_FLASHROM}),)
crossystem-y += crossystem.c
endif
