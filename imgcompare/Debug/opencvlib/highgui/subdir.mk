################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../opencvlib/highgui/bitstrm.cpp \
../opencvlib/highgui/grfmt_base.cpp \
../opencvlib/highgui/grfmt_bmp.cpp \
../opencvlib/highgui/grfmt_jpeg.cpp \
../opencvlib/highgui/grfmt_png.cpp \
../opencvlib/highgui/loadsave.cpp \
../opencvlib/highgui/utils.cpp 

OBJS += \
./opencvlib/highgui/bitstrm.o \
./opencvlib/highgui/grfmt_base.o \
./opencvlib/highgui/grfmt_bmp.o \
./opencvlib/highgui/grfmt_jpeg.o \
./opencvlib/highgui/grfmt_png.o \
./opencvlib/highgui/loadsave.o \
./opencvlib/highgui/utils.o 

CPP_DEPS += \
./opencvlib/highgui/bitstrm.d \
./opencvlib/highgui/grfmt_base.d \
./opencvlib/highgui/grfmt_bmp.d \
./opencvlib/highgui/grfmt_jpeg.d \
./opencvlib/highgui/grfmt_png.d \
./opencvlib/highgui/loadsave.d \
./opencvlib/highgui/utils.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/highgui/%.o: ../opencvlib/highgui/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


