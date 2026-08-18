/*******************************************************************************
  Main Source File
*******************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "definitions.h"
#include "app/lora_app.h"

int main(void)
{
    SYS_Initialize(NULL);
    (void)LORA_APP_Initialize();

    while (true)
    {
        LORA_APP_Tasks();
        SYS_Tasks();
    }

    return EXIT_FAILURE;
}
