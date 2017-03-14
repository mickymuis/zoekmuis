################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../opencvlib/flann/flann.cpp \
../opencvlib/flann/miniflann.cpp 

OBJS += \
./opencvlib/flann/flann.o \
./opencvlib/flann/miniflann.o 

CPP_DEPS += \
./opencvlib/flann/flann.d \
./opencvlib/flann/miniflann.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/flann/%.o: ../opencvlib/flann/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


