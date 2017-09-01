#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
#

# Component configuration in preprocessor defines
CFLAGS += -Wno-char-subscripts 

COMPONENT_ADD_INCLUDEDIRS :=  \
device/inc	\
sensors/inc	\
telemetry/inc	\
.

COMPONENT_SRCDIRS :=  \
device/src	\
sensors/src \
telemetry/src	\
.
