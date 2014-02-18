#include "unittest.h"
#include "prettifysegfault.h"

int main(int argc, char** argv) {
    PrettifySegfault::setup ();

    if (1 == argc)
        return BacktraceTest::UnitTest::test();
    else
    {
        printf("%s: Invalid argument\n", argv[0]);
        return 1;
    }
}
