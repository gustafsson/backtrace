#include "../unittest.h"
#include "../prettifysegfault.h"

int main(int argc, char** argv)
{
    if (1 != argc)
    {
        printf("%s: Invalid argument\n", argv[0]);
        return 1;
    }

    PrettifySegfault::setup ();

    return BacktraceTest::UnitTest::test(false);
}
