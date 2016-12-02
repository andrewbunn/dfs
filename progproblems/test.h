#pragma once

#define USE_SSE2
#define USE_AVX

#include <vector>
#include <string.h>

struct Test;
typedef std::vector<const Test *> TestList;
class TestManager {
public:
    static TestManager& Instance() {
        static TestManager singleton;
        return singleton;
    }

    void AddTest(const Test* test) {
        mTests.push_back(test);
    }

    const TestList& GetTests() const {
        return mTests;
    }

    TestList& GetTests() {
        return mTests;
    }

private:
    TestList mTests;
};

struct Test {
    Test(
        const char* fname,
        void (*normaldistf)(float*, size_t),
        void (*normaldist)(double*, size_t))
        :
        fname(fname),
        normaldistf(normaldistf),
        normaldist(normaldist)
    {
        TestManager::Instance().AddTest(this);
    }

    bool operator<(const Test& rhs) const {
        return strcmp(fname, rhs.fname) < 0;
    }

    const char* fname;
    void (*normaldistf)(float*, size_t);
    void (*normaldist)(double*, size_t);
};


#define STRINGIFY(x) #x
#define REGISTER_TEST(f) static Test gRegister##f(STRINGIFY(f), normaldistf##_##f, normaldist##_##f)
#define REGISTER_TEST_FLOATONLY(f) static Test gRegister##f(STRINGIFY(f), normaldistf##_##f, 0)
