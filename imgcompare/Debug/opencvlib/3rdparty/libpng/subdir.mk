################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../opencvlib/3rdparty/libpng/png.c \
../opencvlib/3rdparty/libpng/pngerror.c \
../opencvlib/3rdparty/libpng/pngget.c \
../opencvlib/3rdparty/libpng/pngmem.c \
../opencvlib/3rdparty/libpng/pngpread.c \
../opencvlib/3rdparty/libpng/pngread.c \
../opencvlib/3rdparty/libpng/pngrio.c \
../opencvlib/3rdparty/libpng/pngrtran.c \
../opencvlib/3rdparty/libpng/pngrutil.c \
../opencvlib/3rdparty/libpng/pngset.c \
../opencvlib/3rdparty/libpng/pngtrans.c \
../opencvlib/3rdparty/libpng/pngwio.c \
../opencvlib/3rdparty/libpng/pngwrite.c \
../opencvlib/3rdparty/libpng/pngwtran.c \
../opencvlib/3rdparty/libpng/pngwutil.c 

OBJS += \
./opencvlib/3rdparty/libpng/png.o \
./opencvlib/3rdparty/libpng/pngerror.o \
./opencvlib/3rdparty/libpng/pngget.o \
./opencvlib/3rdparty/libpng/pngmem.o \
./opencvlib/3rdparty/libpng/pngpread.o \
./opencvlib/3rdparty/libpng/pngread.o \
./opencvlib/3rdparty/libpng/pngrio.o \
./opencvlib/3rdparty/libpng/pngrtran.o \
./opencvlib/3rdparty/libpng/pngrutil.o \
./opencvlib/3rdparty/libpng/pngset.o \
./opencvlib/3rdparty/libpng/pngtrans.o \
./opencvlib/3rdparty/libpng/pngwio.o \
./opencvlib/3rdparty/libpng/pngwrite.o \
./opencvlib/3rdparty/libpng/pngwtran.o \
./opencvlib/3rdparty/libpng/pngwutil.o 

C_DEPS += \
./opencvlib/3rdparty/libpng/png.d \
./opencvlib/3rdparty/libpng/pngerror.d \
./opencvlib/3rdparty/libpng/pngget.d \
./opencvlib/3rdparty/libpng/pngmem.d \
./opencvlib/3rdparty/libpng/pngpread.d \
./opencvlib/3rdparty/libpng/pngread.d \
./opencvlib/3rdparty/libpng/pngrio.d \
./opencvlib/3rdparty/libpng/pngrtran.d \
./opencvlib/3rdparty/libpng/pngrutil.d \
./opencvlib/3rdparty/libpng/pngset.d \
./opencvlib/3rdparty/libpng/pngtrans.d \
./opencvlib/3rdparty/libpng/pngwio.d \
./opencvlib/3rdparty/libpng/pngwrite.d \
./opencvlib/3rdparty/libpng/pngwtran.d \
./opencvlib/3rdparty/libpng/pngwutil.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/3rdparty/libpng/%.o: ../opencvlib/3rdparty/libpng/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


