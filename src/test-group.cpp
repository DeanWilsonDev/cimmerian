#include "umbra/test-group.hpp"

static const char* DEFAULT_GROUP_NAME = "Ungrouped";

const char* CheckGroupName(const char* groupName)
{
    return groupName ? groupName : DEFAULT_GROUP_NAME;
}
