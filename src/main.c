#include <stdlib.h>

#include "lib/rld.h"
#include "lib/rld.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

__MAIN

struct config
config_create(struct context *context)
{
}

struct command *
commands_create(struct changes_context *changes_context, struct context *context)
{
}

void
commands_free(struct command *commands, struct context *context)
{
}

bool
should_include_dir(char *dir)
{
}

bool
should_include_file_change(char *dir, char *file_name)
{
}

void
config_free(struct config config, struct context *context)
{
}
