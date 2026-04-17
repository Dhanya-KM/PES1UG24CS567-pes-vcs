// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c

#include "index.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── HELPER FUNCTIONS ────────────────────────────────────────────────────────

// Comparison function for qsort to keep index entries sorted by path.
static int compare_entries(const void *a, const void *b) {
    const IndexEntry *entry_a = (const IndexEntry *)a;
    const IndexEntry *entry_b = (const IndexEntry *)b;
    return strcmp(entry_a->path, entry_b->path);
}

// ─── PROVIDED ────────────────────────────────────────────────────────────────

// Find an index entry by path (linear scan).
IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// Remove a file from the index.
int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

// Print the status of the working directory.
int index_status(const Index *index) {
    printf("Staged changes:\n");
    int staged_count = 0;
    for (int i = 0; i < index->count; i++) {
        printf("  staged:      %s\n", index->entries[i].path);
        staged_count++;
    }
    if (staged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged_count = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged_count++;
        } else {
            if (st.st_mtime != (time_t)index->entries[i].mtime_sec || st.st_size != (off_t)index->entries[i].size) {
                printf("  modified:   %s\n", index->entries[i].path);
                unstaged_count++;
            }
        }
    }
    if (unstaged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    int untracked_count = 0;
    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (strcmp(ent->d_name, ".pes") == 0) continue;
            if (strcmp(ent->d_name, "pes") == 0) continue; 
            if (strstr(ent->d_name, ".o") != NULL) continue; 

            int is_tracked = 0;
            for (int i = 0; i < index->count; i++) {
                if (strcmp(index->entries[i].path, ent->d_name) == 0) {
                    is_tracked = 1; 
                    break;
                }
            }
            
            if (!is_tracked) {
                struct stat st;
                stat(ent->d_name, &st);
                if (S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked_count++;
                }
            }
        }
        closedir(dir);
    }
    if (untracked_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    return 0;
}

// ─── IMPLEMENTED ─────────────────────────────────────────────────────────────

// Load the index from .pes/index.
int index_load(Index *index) {
    index->count = 0;
    
    // 1. Try to open the index
    FILE *f = fopen(".pes/index", "r");
    
    // 2. CRITICAL FIX: If the file doesn't exist, it's NOT an error.
    // It just means the staging area is empty. Return 0 to continue.
    if (!f) {
        return 0; 
    }

    char hex_hash[65];
    // 3. Read the file (using %u for the uint32_t size field)
    while (fscanf(f, "%o %64s %ld %u ", 
                  &index->entries[index->count].mode, 
                  hex_hash, 
                  &index->entries[index->count].mtime_sec, 
                  &index->entries[index->count].size) == 4) {
        
        hex_to_hash(hex_hash, &index->entries[index->count].hash);

        if (fgets(index->entries[index->count].path, sizeof(index->entries[index->count].path), f)) {
            index->entries[index->count].path[strcspn(index->entries[index->count].path, "\n")] = 0;
        }

        index->count++;
        if (index->count >= MAX_INDEX_ENTRIES) break;
    }

    fclose(f);
    return 0; // Success
}
// Save the index to .pes/index atomically.
int index_save(const Index *index) {
    // 1. Sort the entries by path for tree construction.
    qsort((void*)index->entries, index->count, sizeof(IndexEntry), compare_entries);

    // 2. Open a temporary file for writing.
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) {
        perror("Error opening index.tmp");
        return -1;
    }

    // 3. Write each entry to the text file.
    for (int i = 0; i < index->count; i++) {
        char hex[65];
        hash_to_hex(&index->entries[i].hash, hex);
        
        // FIXED: Use %u for uint32_t size.
        if (fprintf(f, "%o %s %ld %u %s\n", 
                index->entries[i].mode, 
                hex, 
                (long)index->entries[i].mtime_sec, 
                index->entries[i].size, 
                index->entries[i].path) < 0) {
            fclose(f);
            return -1;
        }
    }

    // 4. Flush and sync to disk for safety.
    fflush(f);
    fsync(fileno(f)); 
    fclose(f);

    // 5. Atomic Rename.
    if (rename(".pes/index.tmp", ".pes/index") != 0) {
        perror("Error renaming index");
        unlink(".pes/index.tmp"); 
        return -1;
    }

    return 0;
}

// Stage a file for the next commit.
int index_add(Index *index, const char *path) {
    struct stat st;

    // 1. Get file metadata.
    if (stat(path, &st) != 0) {
        perror("stat failed");
        return -1;
    }

    // 2. Read content into memory.
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen failed");
        return -1;
    }

    void *data = malloc(st.st_size);
    if (st.st_size > 0 && !data) {
        fclose(f);
        return -1;
    }
    
    if (st.st_size > 0) {
        fread(data, 1, st.st_size, f);
    }
    fclose(f);

    // 3. Write as OBJ_BLOB.
    ObjectID hash;
    if (object_write(OBJ_BLOB, data, st.st_size, &hash) != 0) {
        if (data) free(data);
        return -1;
    }
    if (data) free(data);

    // 4. Update existing or create new entry.
    IndexEntry *entry = index_find(index, path);
    if (!entry) {
        if (index->count >= MAX_INDEX_ENTRIES) {
            fprintf(stderr, "error: index is full\n");
            return -1;
        }
        entry = &index->entries[index->count++];
        strncpy(entry->path, path, sizeof(entry->path) - 1);
        entry->path[sizeof(entry->path) - 1] = '\0';
    }

    // 5. Populate metadata.
    entry->mode = st.st_mode;
    entry->mtime_sec = (long)st.st_mtime;
    entry->size = (uint32_t)st.st_size;
    memcpy(entry->hash.hash, hash.hash, HASH_SIZE);

    // 6. Atomic save.
    return index_save(index);
}
