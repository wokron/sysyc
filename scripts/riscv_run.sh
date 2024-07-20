#!/bin/bash

# this script will:
# 1. use riscv64-linux-gnu-gcc compiler to compile the given asm file
# 2. use the riscv64-linux-gnu-gcc to link the generated object file with the runtime library to generate the final executable
# 3. run the generated executable

# Arguments:
# $1: the assembly file to compile

BASE_DIR=".riscv"
CC="riscv64-linux-gnu-gcc"

asm_file=$1

# name of file without extension
filename=$(basename -- "$asm_file")
filename="${filename%.*}"

exe_file="$BASE_DIR/$filename"

# check if the assembly file exists
if [ ! -f $asm_file ]; then
    echo "Error: the assembly file $asm_file does not exist"
    exit 1
fi

# create the cache directory if it does not exist
mkdir -p $BASE_DIR

# compile the assembly file
obj_file="$BASE_DIR/$filename.o"
$CC -c $asm_file -o $obj_file || ! echo "error: failed to compile assembly file" || exit 1

# check if the runtime library exists, if not, compile it
lib_file="$BASE_DIR/sylib.o"
lib_src_file="$BASE_DIR/sylib.c"
lib_header_file="$BASE_DIR/sylib.h"
if [ ! -f $lib_file ]; then
    echo "the runtime library $lib_file does not exist, compiling..."

    # use cat cmd to output the runtime library to a file
    cat >$lib_src_file <<EOF
#include<stdio.h>
#include<stdarg.h>
#include<sys/time.h>
#include"sylib.h"
/* Input & output functions */
int getint(){int t; scanf("%d",&t); return t; }
int getch(){char c; scanf("%c",&c); return (int)c; }
float getfloat(){
    float n;
    scanf("%a", &n);
    return n;
}

int getarray(int a[]){
  int n;
  scanf("%d",&n);
  for(int i=0;i<n;i++)scanf("%d",&a[i]);
  return n;
}

int getfarray(float a[]) {
    int n;
    scanf("%d", &n);
    for (int i = 0; i < n; i++) {
        scanf("%a", &a[i]);
    }
    return n;
}
void putint(int a){ printf("%d",a);}
void putch(int a){ printf("%c",a); }
void putarray(int n,int a[]){
  printf("%d:",n);
  for(int i=0;i<n;i++)printf(" %d",a[i]);
  printf("\n");
}
void putfloat(float a) {
  printf("%a", a);
}
void putfarray(int n, float a[]) {
    printf("%d:", n);
    for (int i = 0; i < n; i++) {
        printf(" %a", a[i]);
    }
    printf("\n");
}

void putf(char a[], ...) {
    va_list args;
    va_start(args, a);
    vfprintf(stdout, a, args);
    va_end(args);
}

/* Timing function implementation */
__attribute((constructor)) void before_main(){
  for(int i=0;i<_SYSY_N;i++)
    _sysy_h[i] = _sysy_m[i]= _sysy_s[i] = _sysy_us[i] =0;
  _sysy_idx=1;
}  
__attribute((destructor)) void after_main(){
  for(int i=1;i<_sysy_idx;i++){
    fprintf(stderr,"Timer@%04d-%04d: %dH-%dM-%dS-%dus\n",\
      _sysy_l1[i],_sysy_l2[i],_sysy_h[i],_sysy_m[i],_sysy_s[i],_sysy_us[i]);
    _sysy_us[0]+= _sysy_us[i]; 
    _sysy_s[0] += _sysy_s[i]; _sysy_us[0] %= 1000000;
    _sysy_m[0] += _sysy_m[i]; _sysy_s[0] %= 60;
    _sysy_h[0] += _sysy_h[i]; _sysy_m[0] %= 60;
  }
  fprintf(stderr,"TOTAL: %dH-%dM-%dS-%dus\n",_sysy_h[0],_sysy_m[0],_sysy_s[0],_sysy_us[0]);
}  
void _sysy_starttime(int lineno){
  _sysy_l1[_sysy_idx] = lineno;
  gettimeofday(&_sysy_start,NULL);
}
void _sysy_stoptime(int lineno){
  gettimeofday(&_sysy_end,NULL);
  _sysy_l2[_sysy_idx] = lineno;
  _sysy_us[_sysy_idx] += 1000000 * ( _sysy_end.tv_sec - _sysy_start.tv_sec ) + _sysy_end.tv_usec - _sysy_start.tv_usec;
  _sysy_s[_sysy_idx] += _sysy_us[_sysy_idx] / 1000000 ; _sysy_us[_sysy_idx] %= 1000000;
  _sysy_m[_sysy_idx] += _sysy_s[_sysy_idx] / 60 ; _sysy_s[_sysy_idx] %= 60;
  _sysy_h[_sysy_idx] += _sysy_m[_sysy_idx] / 60 ; _sysy_m[_sysy_idx] %= 60;
  _sysy_idx ++;
}
EOF

    cat >$lib_header_file <<EOF
#ifndef __SYLIB_H_
#define __SYLIB_H_

#include<stdio.h>
#include<stdarg.h>
#include<sys/time.h>
/* Input & output functions */
int getint(),getch(),getarray(int a[]);
float getfloat();
int getfarray(float a[]);

void putint(int a),putch(int a),putarray(int n,int a[]);
void putfloat(float a);
void putfarray(int n, float a[]);

void putf(char a[], ...);

/* Timing function implementation */
struct timeval _sysy_start,_sysy_end;
#define starttime() _sysy_starttime(__LINE__)
#define stoptime()  _sysy_stoptime(__LINE__)
#define _SYSY_N 1024
int _sysy_l1[_SYSY_N],_sysy_l2[_SYSY_N];
int _sysy_h[_SYSY_N], _sysy_m[_SYSY_N],_sysy_s[_SYSY_N],_sysy_us[_SYSY_N];
int _sysy_idx;
void _sysy_starttime(int lineno);
void _sysy_stoptime(int lineno);

#endif
EOF

    # compile the runtime library
    $CC -c $lib_src_file -o $lib_file || ! echo "error: failed to compile runtime library" || exit 1

    echo "runtime library compiled"
fi

# link the object file with the runtime library (we will use qemu-user-static to run the generated executable, so `-static` is used)
$CC $obj_file $lib_file -o $exe_file -static || ! echo "error: failed to link object file with runtime library" || exit 1

# run the generated executable
./$exe_file

# store the exit code of the executable
exit_code=$?

# remove the generated executable
rm $exe_file $obj_file

# exit with the exit code
exit $exit_code
