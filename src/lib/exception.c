#include <lib/exception.h>
#include <lib/stdio.h>

void raise_exception(exception_t* exception) {
    printf("%oException raised: %s (ID: %d)\nError message: %s\n", "red" , exception->name, exception->id, exception->message);
}
