################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../opencvlib/core/alloc.cpp \
../opencvlib/core/arithm.cpp \
../opencvlib/core/array.cpp \
../opencvlib/core/cmdparser.cpp \
../opencvlib/core/convert.cpp \
../opencvlib/core/copy.cpp \
../opencvlib/core/datastructs.cpp \
../opencvlib/core/drawing.cpp \
../opencvlib/core/dxt.cpp \
../opencvlib/core/lapack.cpp \
../opencvlib/core/mathfuncs.cpp \
../opencvlib/core/matmul.cpp \
../opencvlib/core/matop.cpp \
../opencvlib/core/matrix.cpp \
../opencvlib/core/out.cpp \
../opencvlib/core/persistence.cpp \
../opencvlib/core/rand.cpp \
../opencvlib/core/stat.cpp \
../opencvlib/core/system.cpp 

OBJS += \
./opencvlib/core/alloc.o \
./opencvlib/core/arithm.o \
./opencvlib/core/array.o \
./opencvlib/core/cmdparser.o \
./opencvlib/core/convert.o \
./opencvlib/core/copy.o \
./opencvlib/core/datastructs.o \
./opencvlib/core/drawing.o \
./opencvlib/core/dxt.o \
./opencvlib/core/lapack.o \
./opencvlib/core/mathfuncs.o \
./opencvlib/core/matmul.o \
./opencvlib/core/matop.o \
./opencvlib/core/matrix.o \
./opencvlib/core/out.o \
./opencvlib/core/persistence.o \
./opencvlib/core/rand.o \
./opencvlib/core/stat.o \
./opencvlib/core/system.o 

CPP_DEPS += \
./opencvlib/core/alloc.d \
./opencvlib/core/arithm.d \
./opencvlib/core/array.d \
./opencvlib/core/cmdparser.d \
./opencvlib/core/convert.d \
./opencvlib/core/copy.d \
./opencvlib/core/datastructs.d \
./opencvlib/core/drawing.d \
./opencvlib/core/dxt.d \
./opencvlib/core/lapack.d \
./opencvlib/core/mathfuncs.d \
./opencvlib/core/matmul.d \
./opencvlib/core/matop.d \
./opencvlib/core/matrix.d \
./opencvlib/core/out.d \
./opencvlib/core/persistence.d \
./opencvlib/core/rand.d \
./opencvlib/core/stat.d \
./opencvlib/core/system.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/core/%.o: ../opencvlib/core/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


