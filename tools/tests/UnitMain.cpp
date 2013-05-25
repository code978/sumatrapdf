#include "BaseUtil.h"
#include "UtAssert.h"

// in src/util/tests/UtilTests.cpp
extern void BaseUtils_UnitTests();

// in src/UnitTests.cpp
extern void SumatraPDF_UnitTests();

int main(int argc, char **argv)
{
    BaseUtils_UnitTests();
    SumatraPDF_UnitTests();
    return utassert_print_results();
}
