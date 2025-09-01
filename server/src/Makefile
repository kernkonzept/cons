PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

TARGET       := cons
SRC_CC       := controller.cc mux_impl.cc main.cc client.cc vcon_client.cc \
                vcon_fe_base.cc vcon_fe.cc registry.cc virtio_client.cc

SRC_CC-$(CONFIG_CONS_USE_ASYNC_FE)  += async_vcon_fe.cc

REQUIRES_LIBS                             = libstdc++ cxx_libc_io cxx_io l4virtio
REQUIRES_LIBS-$(CONFIG_CONS_USE_ASYNC_FE) = libpthread

include $(L4DIR)/mk/prog.mk
