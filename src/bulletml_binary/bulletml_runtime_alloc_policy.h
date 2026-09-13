#pragma once

#include "bulletmlrunner.hpp"
#include "bulletmlstate.hpp"

struct BulletMlRuntimeAllocPolicySnapshot {
    bool runnerStartupOnly;
    bool runnerStartupPreallocation;
    bool stateStartupOnly;
    bool stateStartupPreallocation;
};

inline BulletMlRuntimeAllocPolicySnapshot getBulletMlRuntimeAllocPolicySnapshot() {
    BulletMlRuntimeAllocPolicySnapshot snapshot;
    snapshot.runnerStartupOnly = BulletMLRunnerImpl::IsStartupOnlyAllocationEnabled();
    snapshot.runnerStartupPreallocation = BulletMLRunnerImpl::IsStartupPreallocationPhase();
    snapshot.stateStartupOnly = isBulletMlStateStartupOnlyAllocationEnabled();
    snapshot.stateStartupPreallocation = isBulletMlStateStartupPreallocationPhase();
    return snapshot;
}

inline void beginBulletMlStartupPreallocationPhase() {
    BulletMLRunnerImpl::BeginStartupPreallocation();
    beginBulletMlStateStartupPreallocation();
}

inline void endBulletMlStartupPreallocationPhase() {
    BulletMLRunnerImpl::EndStartupPreallocation();
    endBulletMlStateStartupPreallocation();
}

inline void setBulletMlStartupOnlyAllocationPolicy(bool enabled) {
    BulletMLRunnerImpl::EnableStartupOnlyAllocation(enabled);
    enableBulletMlStateStartupOnlyAllocation(enabled);
    // Keep policy transitions deterministic; callers enable preallocation explicitly.
    endBulletMlStartupPreallocationPhase();
}
