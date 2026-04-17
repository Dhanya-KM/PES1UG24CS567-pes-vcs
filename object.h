#ifndef OBJECT_H
#define OBJECT_H

#include "pes.h"

/* * Writes an object to the store. 
 * Prepends header, computes hash, and saves to .pes/objects/XX/YYY...
 */
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

/* * Reads an object from the store and verifies its integrity.
 */
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

/* * Helper to get the file path for a given ObjectID.
 */
void object_path(const ObjectID *id, char *buf, size_t size);

/* * Returns 1 if the object exists on disk, 0 otherwise.
 */
int object_exists(const ObjectID *id);

#endif
