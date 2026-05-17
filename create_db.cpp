#include "database.h"

// This file exists because the Visual Studio project references it.
// The app initializes DB lazily via Database::openOrCreate().
void ensureDatabase()
{
    Database::instance().openOrCreate();
}

