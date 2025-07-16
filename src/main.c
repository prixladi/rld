#include <stdlib.h>

#include "lib/rld.h"
#include "lib/rld.h"

#include "lib/utils/string.h"
#include "lib/utils/vector.h"

__MAIN

int
config_init(struct context *context, struct config *config)
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
should_include_dir(char *dir, struct context *context)
{
}

bool
should_include_file_change(char *dir, char *file_name, struct context *context)
{
}

void
config_free(struct config* config, struct context *context)
{
}
