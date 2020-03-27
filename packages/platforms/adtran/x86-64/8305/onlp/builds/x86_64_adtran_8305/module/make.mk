###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_adtran_8305_INCLUDES := -I $(THIS_DIR)inc
x86_64_adtran_8305_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_adtran_8305_DEPENDMODULE_ENTRIES := init:x86_64_accton_as7712_32x ucli:x86_64_accton_as7712_32x

