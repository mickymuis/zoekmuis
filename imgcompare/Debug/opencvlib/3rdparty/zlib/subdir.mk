################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../opencvlib/3rdparty/zlib/adler32.c \
../opencvlib/3rdparty/zlib/compress.c \
../opencvlib/3rdparty/zlib/crc32.c \
../opencvlib/3rdparty/zlib/deflate.c \
../opencvlib/3rdparty/zlib/gzclose.c \
../opencvlib/3rdparty/zlib/gzlib.c \
../opencvlib/3rdparty/zlib/gzread.c \
../opencvlib/3rdparty/zlib/gzwrite.c \
../opencvlib/3rdparty/zlib/infback.c \
../opencvlib/3rdparty/zlib/inffast.c \
../opencvlib/3rdparty/zlib/inflate.c \
../opencvlib/3rdparty/zlib/inftrees.c \
../opencvlib/3rdparty/zlib/trees.c \
../opencvlib/3rdparty/zlib/uncompr.c \
../opencvlib/3rdparty/zlib/zutil.c 

OBJS += \
./opencvlib/3rdparty/zlib/adler32.o \
./opencvlib/3rdparty/zlib/compress.o \
./opencvlib/3rdparty/zlib/crc32.o \
./opencvlib/3rdparty/zlib/deflate.o \
./opencvlib/3rdparty/zlib/gzclose.o \
./opencvlib/3rdparty/zlib/gzlib.o \
./opencvlib/3rdparty/zlib/gzread.o \
./opencvlib/3rdparty/zlib/gzwrite.o \
./opencvlib/3rdparty/zlib/infback.o \
./opencvlib/3rdparty/zlib/inffast.o \
./opencvlib/3rdparty/zlib/inflate.o \
./opencvlib/3rdparty/zlib/inftrees.o \
./opencvlib/3rdparty/zlib/trees.o \
./opencvlib/3rdparty/zlib/uncompr.o \
./opencvlib/3rdparty/zlib/zutil.o 

C_DEPS += \
./opencvlib/3rdparty/zlib/adler32.d \
./opencvlib/3rdparty/zlib/compress.d \
./opencvlib/3rdparty/zlib/crc32.d \
./opencvlib/3rdparty/zlib/deflate.d \
./opencvlib/3rdparty/zlib/gzclose.d \
./opencvlib/3rdparty/zlib/gzlib.d \
./opencvlib/3rdparty/zlib/gzread.d \
./opencvlib/3rdparty/zlib/gzwrite.d \
./opencvlib/3rdparty/zlib/infback.d \
./opencvlib/3rdparty/zlib/inffast.d \
./opencvlib/3rdparty/zlib/inflate.d \
./opencvlib/3rdparty/zlib/inftrees.d \
./opencvlib/3rdparty/zlib/trees.d \
./opencvlib/3rdparty/zlib/uncompr.d \
./opencvlib/3rdparty/zlib/zutil.d 


# Each subdirectory must supply rules for building sources it contributes
opencvlib/3rdparty/zlib/%.o: ../opencvlib/3rdparty/zlib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -w -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


