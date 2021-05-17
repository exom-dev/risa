#ifndef RISA_MACRO_H_GUARD
#define RISA_MACRO_H_GUARD

#define RISA_STRINGIFY_DIRECTLY(str) #str
#define RISA_STRINGIFY(str) RISA_STRINGIFY_DIRECTLY(str)

#define RISA_CONCAT_DIRECTLY(left, right)  left##right
#define RISA_CONCAT(left, right) RISA_CONCAT_DIRECTLY(left, right)

#endif
