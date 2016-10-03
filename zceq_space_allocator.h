/* Copyright @ 2016 Pavel Moravec */
#ifndef SPACE_ALLOCATOR_H_
#define SPACE_ALLOCATOR_H_

#include <cassert>
#include <vector>
#include <string>
#include "zceq_misc.h"

namespace zceq_solver {

using std::vector;

class SpaceAllocator {
 public:
  static constexpr u64 granularity = 4096;
  static_assert((granularity & (granularity - 1)) == 0ul, "");

  SpaceAllocator(u64 slot_size, u64 needed_slots, bool report_changes) {
    slot_size_ = (slot_size + granularity - 1) & ~(granularity - 1);
    slot_count_ = needed_slots;
    slot_states_.resize(slot_count_);
    dump_on_change_ = report_changes;
  }
  ~SpaceAllocator();

  static constexpr u32 PlaceNotFound = 0xffffffff;
  static constexpr u32 FirstAvailable = 0xfffffffe;

  class Space {
    Space(SpaceAllocator* owner, u32 time, u32 place, u32 size, void* memory, std::string name)
        : place_(place), size_(size), memory_(memory), owner_(owner), allocation_time_(time),
          release_time_(0), name_(name) {}
    u32 place_;
    u32 size_;
    void* memory_;
    SpaceAllocator* owner_;
    u32 allocation_time_;
    u32 release_time_;
    std::string name_;

   public:
    bool IsUsed() {
      return memory_ != nullptr;
    }
    Space* Release() {
      owner_->Release(this, false);
      memory_ = nullptr;
      return this;
    }
    Space* AllocateIfNotUsed() {
      return AllocateIfNotUsed(place_);
    }
    Space* AllocateIfNotUsed(u32 place) {
      if (!IsUsed()) {
        Allocate(place);
      }
      return this;
    }
    Space* Allocate() {
      return owner_->Allocate(this, place_, size_);
    }
    Space* Allocate(u32 new_place) {
      return owner_->Allocate(this, new_place, size_);
    }
    template<typename T>
    Space* Resize() {
      return Resize(sizeof(T));
    }
    Space* Resize(u32 new_size) {
      if (!IsUsed()) {
        size_ = new_size;
        return this;
      } else {
        owner_->Release(this, true);
        return owner_->Allocate(this, place_, new_size);
      }
    }
    Space* Move(u32 new_place) {
      owner_->Release(this, true);
      return owner_->Allocate(this, new_place, size_);
    }
    template<typename T>
    Space* Reallocate(u32 new_place) {
      return Reallocate(new_place, sizeof(T));
    }
    Space* Reallocate(u32 new_place, u32 new_size) {
      assert(IsUsed());
      owner_->Release(this, true);
      return owner_->Allocate(this, new_place, new_size);
    }
    template<typename T>
    T* As(bool reallocate_if_unused=false) {
      if (reallocate_if_unused)
        AllocateIfNotUsed();
      assert(sizeof(T) <= size_);
      assert(memory_ != nullptr);
      return (T*)memory_;
    }
    friend class SpaceAllocator;
  };

  template<typename T>
  Space* CreateSpace(std::string name, u32 place) {
    return CreateSpace(name, place, sizeof(T));
  }
  Space* CreateSpace(std::string name, u32 place, u32 size);

  template<typename T>
  Space* Allocate(std::string name, u32 place) {
    return Allocate(name, place, sizeof(T));
  }
  Space* Allocate(std::string name, u32 place, u32 size) {
    auto space = CreateSpace(name, place, size);
    return Allocate(space, place, size);
  }
  Space* Release(Space* space) {
    return Release(space, false);
  }
  Space* Release(Space* space, bool reallocation);
  void Reset();
  void SetDumpOnChange(bool value) {
    dump_on_change_ = value;
  }
  bool DumpOnChange() {
    return dump_on_change_;
  }
  void DumpState(const char* message = "") const;

 protected:
  Space* Allocate(Space* space, u32 place, u32 size);
  u32 FindFirstAvailable(u32 size);

  u64 slot_size_;
  u64 slot_count_;
  u8* memory_ = nullptr;
  vector<Space*> slot_states_;
  vector<Space*> all_spaces_;
  vector<Space*> space_objs_buffer_;
  u32 time_ = 0;
  bool dump_on_change_;
};

}  // namespace zceq_solver

#endif  // SPACE_ALLOCATOR_H_
