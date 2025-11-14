//This file's only job is to "implement" the stb_image library
// header file (downloaded from a github repo to read jpg/png) becomes C file
#define STB_IMAGE_IMPLEMENTATION // raising the flag, in this one file i want you to build the actual functions\
if i just included stb in multiple .c files i would end up with multiple copies of the function code which can lead to errors.
#include <stb_image.h>  