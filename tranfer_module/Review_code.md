- exclude tests folder and example, 
- after review update code and update README.md, design file if needed

## 1. Coding Convention
- Verify that the code follows the rules defined in @coding_style.md.
- Check naming, formatting, comments, file structure, macros, and API usage consistency.
- Reject unclear, inconsistent, or non-standard implementations.

## 2. Compare With Previous Review
- Compare the current changes with the last reviewed version.
- Focus only on modified logic and affected areas.
- Check for:
  - Undefined Behavior (UB)
  - Memory issues
  - Race conditions
  - Null pointer access
  - Buffer overflow / out-of-bound access
  - Integer overflow / underflow
  - Uninitialized variables
  - Invalid state transitions
  - Resource leaks
  - Hidden side effects

- Analyze all potential corner cases and failure paths.

## 3. Impact Analysis on Existing Flow
- Analyze how the new changes affect the existing system behavior.

### Variable / State Changes
- If a variable, flag, or state is modified:
  - Find all locations where it is used.
  - Analyze how the new value changes existing behavior.
  - Verify that old assumptions are still valid.

### Added Conditions
- If new conditions are introduced:
  - Check whether the condition can actually occur.
  - Analyze which existing paths are blocked or changed.
  - Verify impacts on previous execution flow.

### Function Call Changes
- If function call conditions are changed:
  - Verify whether the function may now:
    - Be skipped unexpectedly
    - Be called more frequently
    - Be called in invalid states
  - Analyze dependencies and side effects.

### Regression Risk
- Ensure the modification does not break:
  - Existing features
  - Timing behavior
  - State machines
  - Retry / timeout logic
  - Communication flow
  - Synchronization logic

## 4. Real-Time & Concurrency Analysis
- Analyze system-level runtime impact.

### Blocking Analysis
- Check whether new logic introduces:
  - Long blocking operations
  - Busy waiting
  - Infinite loops
  - Excessive locking

### Deadlock Analysis
- Verify lock ordering and shared resource usage.
- Check mutex/semaphore interactions carefully.

### Latency Analysis
- Analyze worst-case execution time.
- Check impacts on:
  - Task scheduling
  - Interrupt response
  - Communication timing
  - Real-time deadlines

### Interrupt Safety
- Identify functions that may run inside ISR context.
- Verify ISR-safe behavior:
  - No blocking APIs
  - No dynamic memory allocation
  - No unsafe shared resource access
  - Proper atomic/shared variable handling

### Thread Safety
- Verify synchronization for shared data access.
- Check race conditions between:
  - Tasks
  - ISR ↔ Task
  - Multi-threaded access

---

## 5. General Review Principles
- Prefer simple and maintainable solutions.
- Avoid hidden logic and side effects.
- Ensure behavior is deterministic and predictable.
- Validate error handling and recovery paths.
- Review both normal flow and abnormal scenarios.
- Do not assume inputs or states are always valid.