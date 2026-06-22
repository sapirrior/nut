#include "test_runner.h"
#include "engine/utils/sonet_headers.h"
#include <stdlib.h>
#include <string.h>

void test_header_serialization(void) {
    SonetHeaderMap *m = sonet_headermap_new();
    ASSERT_NOT_NULL(m);
    
    sonet_headermap_set(m, "Content-Type", "application/json");
    sonet_headermap_set(m, "accept", "text/html"); // case-insensitive handling
    
    char *serialized = sonet_headermap_serialize(m);
    ASSERT_NOT_NULL(serialized);
    
    // Check if both headers are present (order might vary, but typical implementation appends)
    ASSERT_TRUE(strstr(serialized, "Content-Type: application/json\r\n") != NULL);
    ASSERT_TRUE(strstr(serialized, "Accept: text/html\r\n") != NULL); // Capitalized correctly
    
    free(serialized);
    sonet_headermap_free(m);
}

void test_cases(void) {
    RUN_TEST(test_header_serialization);
}

TEST_SUITE_MAIN(test_cases)
