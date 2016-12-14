#pragma once

#define USE_SSE2
#define USE_AVX

#include <vector>
#include <string.h>

#define STRINGIFY(x) #x
#define REGISTER_TEST(f) static Test gRegister##f(STRINGIFY(f), normaldistf##_##f, normaldist##_##f)
#define REGISTER_TEST_FLOATONLY(f) static Test gRegister##f(STRINGIFY(f), normaldistf##_##f, 0)
