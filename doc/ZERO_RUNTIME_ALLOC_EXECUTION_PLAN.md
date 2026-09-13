# Zero Runtime Allocation Migration: Sequential Execution Plan

Objective: ensure all gameplay-phase memory is preallocated before entering gameplay and verify each migration step with repeatable tests.

## Rules of execution

1. Execute steps in numeric order only.
2. Do not start step N+1 until all mandatory tests for step N pass.
3. Every step must add or update tests that can be rerun later.
4. Keep test commands deterministic and checked into the repository.

## Step matrix

### Step 1: Production wiring for startup-only allocation policy

Scope:
- Wire existing startup-only controls into production startup flow.
- Keep behavior feature-flagged and observable via logs/counters.

Mandatory tests:
- `bash Tests/test_factory_campaign.sh --emulator mednafen --strict`
- `bash Tests/test_bulletml_campaign.sh --emulator mednafen --strict`
- `ctest --test-dir build --output-on-failure -R "factory|bulletml|native_sh2"`

Exit criteria:
- All tests pass.
- No regressions in startup/title/gameplay transitions.

### Step 2: Preallocate BulletMLState pools from startup budget

Scope:
- Prepopulate state pool + node array buckets + param array buckets.
- Enforce pool-miss fail-fast after gameplay allocation lock.

Mandatory tests:
- Existing Step 1 tests
- Add factory UT cases validating configured preallocation counts are consumed/reused with zero runtime fallback.

Exit criteria:
- In-game state creation does not allocate when cache is sufficient.
- Pool-miss path is deterministic and test-covered.

### Step 3: Preallocate task-buffer pool and block runtime growth

Scope:
- Replace single cache behavior with startup-prewarmed capacity plan.
- Ensure `ensureTaskCapacity` cannot allocate after gameplay lock.

Mandatory tests:
- Existing Step 2 tests
- Add runner UT for growth-block behavior under startup-only policy.

Exit criteria:
- No dynamic task-buffer growth allocation after lock.
- Fanout stress tests pass without runtime allocation.

### Step 4: Eliminate runtime parameter temporary allocations

Scope:
- Remove runtime heap allocations from parameter copy/ref parameter collection paths.
- Use preallocated frames/buffers only.

Mandatory tests:
- Existing Step 3 tests
- Add unit tests for deep ref nesting + parameter fanout bounds.

Exit criteria:
- No allocation in param copy/ref collection paths during gameplay.

### Step 5: Remove FoeCommand runtime fallback allocation

Scope:
- Require pool-only creation after lock.
- Convert fallback allocation into controlled spawn suppression.

Mandatory tests:
- Existing Step 4 tests
- Add functional tests for pool exhaustion handling without crash.

Exit criteria:
- FoeCommand creation after lock never allocates from heap.

### Step 6: Manifest-driven startup sizing

Scope:
- Generate build-time capacity manifest from BLB corpus.
- Use manifest to size all startup preallocations.

Mandatory tests:
- Existing Step 5 tests
- Add manifest consistency tests (generator + runtime consumer).

Exit criteria:
- Startup preallocation sizing fully comes from manifest.

### Step 7: End-to-end zero-runtime-allocation verification

Scope:
- Add explicit “post-lock allocation attempts” counters/assertions.
- Gate campaigns on zero allocation violations.

Mandatory tests:
- Existing Step 6 tests
- Emulator strict campaigns
- Hardware campaigns (USBGamers path) when bench is available

Exit criteria:
- Zero post-lock allocation violations in emulator + hardware functional campaigns.

## Re-execution procedure

Use the harness script below to run steps with gates:
- `bash Tests/run_zero_alloc_steps.sh --step 1`
- `bash Tests/run_zero_alloc_steps.sh --step 2`
- `bash Tests/run_zero_alloc_steps.sh --all`

The script fails fast when any step test gate fails.
