################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../opencvlib/features2d/keypoint.cpp \
../opencvlib/features2d/matchers.cpp 

OBJS += \
./opencvlib/features2d/keypoint.o \
./opencvlib/features2d/matchers.o 

CPP_DEPS += \
./opencvlib/features2d/keypoint.d \
./opencvlib/features2d/matchers.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/features2d/%.o: ../opencvlib/features2d/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


