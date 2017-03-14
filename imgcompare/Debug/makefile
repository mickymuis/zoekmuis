################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include opencvlib/imgproc/subdir.mk
-include opencvlib/highgui/subdir.mk
-include opencvlib/flann/subdir.mk
-include opencvlib/features2d/subdir.mk
-include opencvlib/core/subdir.mk
-include opencvlib/3rdparty/zlib/subdir.mk
-include opencvlib/3rdparty/libpng/subdir.mk
-include opencvlib/3rdparty/libjpeg/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: imgcompare

# Tool invocations
imgcompare: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++  -w -o "../imgcompare" $(OBJS) $(USER_OBJS) $(LIBS) -pthread -lrt
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C++_DEPS)$(C_DEPS)$(CC_DEPS)$(CPP_DEPS)$(EXECUTABLES)$(CXX_DEPS)$(C_UPPER_DEPS) ../imgcompare
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
