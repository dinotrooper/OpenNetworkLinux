
###############################################################################
#
# Inclusive Makefile for the x86_64_mlnx_mtq8100 module.
#
# Autogenerated 2019-07-30 18:51:58.984764
#
###############################################################################
x86_64_mlnx_mtq8100_BASEDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(x86_64_mlnx_mtq8100_BASEDIR)module/make.mk
include $(x86_64_mlnx_mtq8100_BASEDIR)module/auto/make.mk
include $(x86_64_mlnx_mtq8100_BASEDIR)module/src/make.mk

