---
name: Next task
description: Execute the next unchecked task from PROGRESS.md
invokable: true
---

Read `PROGRESS.md` and find the FIRST unchecked [ ] task.
Open its corresponding file from the tasks/ folder (pattern: tasks/taskNN_action_filename.md).
Read the target file named in the task header to confirm the path.
Overwrite it with EXACTLY the fenced code block from the task file — no placeholders, no elision, full content only.
Then mark that task [x] in PROGRESS.md.
STOP and wait for approval.