# 15.05 used GCC 4.8. OpenWrt 25.12.5 uses GCC 14, which turns these into errors.
# Include from a package Makefile:
#   include $(TOPDIR)/mypackages/gcc14-legacy.mk
# and append $(GCC14_LEGACY_CFLAGS) to the CFLAGS you pass into the inner make.
GCC14_LEGACY_CFLAGS:=-Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch
