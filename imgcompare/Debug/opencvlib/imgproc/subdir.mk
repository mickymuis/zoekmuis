################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../opencvlib/imgproc/accum.cpp \
../opencvlib/imgproc/color.cpp \
../opencvlib/imgproc/filter.cpp \
../opencvlib/imgproc/histogram.cpp \
../opencvlib/imgproc/imgwarp.cpp \
../opencvlib/imgproc/moments.cpp \
../opencvlib/imgproc/morph.cpp \
../opencvlib/imgproc/pyramids.cpp \
../opencvlib/imgproc/smooth.cpp \
../opencvlib/imgproc/sumpixels.cpp \
../opencvlib/imgproc/thresh.cpp \
../opencvlib/imgproc/undistort.cpp \
../opencvlib/imgproc/utils2.cpp 

OBJS += \
./opencvlib/imgproc/accum.o \
./opencvlib/imgproc/color.o \
./opencvlib/imgproc/filter.o \
./opencvlib/imgproc/histogram.o \
./opencvlib/imgproc/imgwarp.o \
./opencvlib/imgproc/moments.o \
./opencvlib/imgproc/morph.o \
./opencvlib/imgproc/pyramids.o \
./opencvlib/imgproc/smooth.o \
./opencvlib/imgproc/sumpixels.o \
./opencvlib/imgproc/thresh.o \
./opencvlib/imgproc/undistort.o \
./opencvlib/imgproc/utils2.o 

CPP_DEPS += \
./opencvlib/imgproc/accum.d \
./opencvlib/imgproc/color.d \
./opencvlib/imgproc/filter.d \
./opencvlib/imgproc/histogram.d \
./opencvlib/imgproc/imgwarp.d \
./opencvlib/imgproc/moments.d \
./opencvlib/imgproc/morph.d \
./opencvlib/imgproc/pyramids.d \
./opencvlib/imgproc/smooth.d \
./opencvlib/imgproc/sumpixels.d \
./opencvlib/imgproc/thresh.d \
./opencvlib/imgproc/undistort.d \
./opencvlib/imgproc/utils2.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/imgproc/%.o: ../opencvlib/imgproc/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


