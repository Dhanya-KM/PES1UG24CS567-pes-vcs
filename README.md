# Lab Report: Building PES-VCS
**Student Name:** Dhanya K.M  
**SRN:** PES1UG24CS567  
**Course:** Computer Science (Engineering)  
**Platform:** Ubuntu 22.04 (VirtualBox)
---
## 1. Project Overview
The objective of this lab was to build a simplified Version Control System (VCS) called **PES-VCS**, modeled after the internal architecture of Git. The project implements:
* **Content-addressable storage** for efficient data management.
* **Directory tree serialization** to maintain project structure.
* **A staging area (index)** to track file changes.
* **Commit history management** for version tracking.
---
## 2. Implementation Details

### Phase 1: Object Storage (`object.c`)
Implemented `object_write` and `object_read`. Data is stored by prepending a header (`type size\0`), hashing the content using SHA-256, and saving it in a sharded directory structure (`.pes/objects/XX/YYYY...`).

* **Screenshot 1A (Pass):** ![Pass](images/PES1UG24CS567_Phase1_Pass.png)
* **Screenshot 1B (Sharding):** ![Sharding](images/PES1UG24CS567_Phase1B_Sharding.png)

### Phase 2: Tree Objects (`tree.c`)
Implemented `tree_from_index` and recursive tree construction. This phase handles the mapping of file paths to their respective blob hashes and manages file permissions/modes.

* **Screenshot 2A (Pass):** ![Pass](images/PES1UG24CS567_Phase2_Pass.png)
* **Screenshot 2B (Hex Dump):** ![Hex Dump](images/PES1UG24CS567_Phase2B_Hex.png)

### Phase 3: Staging Area (`index.c`)
Implemented `index_load`, `index_save`, and `index_add`. The index acts as a middleman between the working directory and the next commit, tracking file metadata (`mtime`, `size`) to detect changes.

* **Screenshot 3A (Status Check):** ![Status Check](images/PES1UG24CS567_Phase3_Status_Check.png)
* **Screenshot 3B (Verification):** ![Verification](images/PES1UG24CS567_Phase3_Staging_Verification.png)

### Phase 4: Commits & History (`commit.c`)
Implemented `commit_create`. This function snapshots the index into a tree, links it to a parent commit, records the author (**Dhanya K.M**), and updates the branch reference in `.pes/refs/heads/main`.

* **Screenshot 4A (Log with Author):** ![Log](images/PES1UG24CS567_Final_Commit_Log_with_Author.png)
* **Screenshot 4B (Final Success):** ![Success](images/PES1UG24CS567_Final_Integration_Success_ObjectStore.png)
* **Screenshot 4C (Ref Hash):** ![Ref Hash](images/PES1UG24CS567_Final_Main_Ref_Hash.png)

---

## 3. Analysis Questions

### Phase 5: Branching and Checkout

* **Q5.1: How would you implement `pes checkout <branch>`?** To implement checkout, `.pes/HEAD` must be updated to point to the new branch reference. The `.pes/index` must be overwritten with the tree data from that branch's latest commit. Finally, the working directory must be updated by deleting current files and writing the blobs from the target tree back to the disk.

* **Q5.2: How to detect a "dirty working directory" conflict?** A conflict is detected by comparing the file in the working directory against the index (using `mtime` and `size`). If the file on disk differs from the index, and the file in the index differs from the version in the target branch, the directory is "dirty."

* **Q5.3: What is a "Detached HEAD" state?** A Detached HEAD occurs when `HEAD` points directly to a commit hash instead of a branch reference. Commits made here are "orphaned"—they don't belong to any branch. To recover them, one must find the commit hash in the log and manually create a new branch pointing to that hash.

---

## 4. Garbage Collection (GC)

* **Q6.1: Describe a Reachability Algorithm.** I would use a **Mark and Sweep** algorithm. First, start from all "Roots" (branch refs and HEAD) and recursively mark all reachable commits, trees, and blobs. Second, iterate through `.pes/objects/` and delete any file not marked as reachable.

* **Q6.2: Why is concurrent GC dangerous?** If GC runs while a commit is in progress, a race condition occurs. The GC might delete a new object that was just written but not yet linked to a branch ref, leading to repository corruption.

---

## 5. Conclusion
This lab successfully demonstrated the core mechanics of a Version Control System. By implementing the object store and the reference chain, I have gained a deep understanding of how Git maintains data integrity and tracks project history through content-addressable sharding.
