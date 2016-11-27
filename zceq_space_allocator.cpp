/* Copyright @ 2016 Pavel Moravec */
#include "portable_endian.h"
#ifndef __WINDOWS__
#include <sys/mman.h>
#endif
#include "zceq_space_allocator.h"

namespace zceq_solver {

SpaceAllocator::Space*
SpaceAllocator::CreateSpace(std::string name, u32 place, u32 size) {
  Space* space = nullptr;
  if (space_objs_buffer_.empty()) {
    space = new Space(this, time_, (u32) place,
                      (u32) size, nullptr, name);
    this->all_spaces_.push_back(space);
  } else {
    space = space_objs_buffer_.back();
    space_objs_buffer_.pop_back();
    space->name_ = name;
    space->owner_ = this;
    space->allocation_time_ = 0;
    space->release_time_ = 0;
    space->place_ = (u32)place;
    space->size_ = (u32)size;
    space->memory_ = nullptr;
  }
  return space;
}

SpaceAllocator::Space*
SpaceAllocator::Allocate(Space* space, u32 place, u32 size) {
  if (memory_ == nullptr) {
#ifdef __WINDOWS__
    memory_ = (u8*)malloc(slot_count_ * slot_size_);
#else
    int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    flags |= MAP_HUGETLB;
    auto size = slot_count_ * slot_size_;
    auto result = mmap(nullptr, size, protection, flags, -1, 0);
    if (result == MAP_FAILED) {
      flags &= ~(MAP_HUGETLB);
      result = mmap(nullptr, size, protection, flags, -1, 0);
      if (result == MAP_FAILED) {
        fprintf(stderr, "error number: %d\n", errno);
        abort();
      }
    }
    memory_ = (u8*)result;
#endif
  }

  if (space->IsUsed()) {
    assert(false);
    abort();
  }

  if (place == FirstAvailable) {
    place = FindFirstAvailable(size);
    if (place == PlaceNotFound) {
      printf("[FATAL ERROR] SpaceAllocator: Not available memory in the pre-allocated pool.\n");
      assert(false);
      abort();
    }
  }
  if (size + place > slot_count_) {
    assert(false);
    abort();
  }

  time_++;
  void* address = memory_ + place * slot_size_;
  // void* address = address = (new u8[slot_size_ * space->size_ + 2 * 10000000]) + 10000000;
  // void* address = address = (new u8[slot_size_ * space->size_]);

  space->place_ = place;
  space->size_ = size;
  space->allocation_time_ = time_;
  space->memory_ = address;

  for (auto i : range(size)) {
    auto slot = place + i;
    if (slot_states_[slot] != nullptr) {
      printf("Conflict at [%d]: '%s' and '%s', time %d\n", slot,
             slot_states_[slot]->name_.c_str(), space->name_.c_str(), time_);
      assert(false);
      abort();
    }
    slot_states_[slot] = space;
  }
  if (dump_on_change_)
    DumpState(space->name_.c_str());

  return space;
}

u32 SpaceAllocator::FindFirstAvailable(u32 size) {
  u32 count = 0;
  for (auto i : range(slot_count_)) {
    if (slot_states_[i] != nullptr)
      count = 0;
    else {
      ++count;
      if (count >= size)
        return (u32)(i + 1 - size);
    }
  }
  return PlaceNotFound;
}

SpaceAllocator::Space* SpaceAllocator::Release(Space* space, bool reallocation) {
  if (not space->IsUsed())
    return space;

  if (!reallocation)
    time_++;

  for (auto i : range(space->size_)) {
    auto slot = space->place_ + i;
    if (slot_states_[slot] != space) {
      printf("Overwritten slot at [%d] for '%s', time %d\n", slot,
             space->name_.c_str(), time_);
      assert(false);
    }
    slot_states_[slot] = nullptr;
  }
  if (!reallocation && dump_on_change_)
    DumpState(space->name_.c_str());

  // delete[] ((u8*)(space->memory_) - 0);
  space->memory_ = nullptr;
  space->release_time_ = time_;
  return space;
}

void SpaceAllocator::Reset() {
  for (auto space : space_objs_buffer_) {
    if (space->IsUsed())
      space->Release();
  }
  space_objs_buffer_.clear();
  std::copy(all_spaces_.begin(), all_spaces_.end(),
            std::back_inserter(space_objs_buffer_));
  for (auto& slot : slot_states_)
    slot = nullptr;
}

template<typename Iter, typename Callable, typename Comparator=std::equal_to<Iter>>
void for_same(Iter begin, Iter end, Callable call, Comparator equal_to) {
  if (begin == end)
    return;
  auto partition_begin = begin;
  ++begin;
  for (; begin != end; ++begin) {
    if (equal_to(*begin, *partition_begin))
      continue;
    call(partition_begin, begin);
    partition_begin = begin;
  }
  call(partition_begin, begin);
}

template<typename Callable>
void for_same_space(SpaceAllocator::Space*const* begin,
                    SpaceAllocator::Space*const* end, Callable call) {
  if (begin == end)
    return;
  auto partition_begin = begin;
  ++begin;
  for (; begin != end; ++begin) {
    if (*begin == *partition_begin)
      continue;
    call(partition_begin, begin);
    partition_begin = begin;
  }
  call(partition_begin, begin);
}

void SpaceAllocator::DumpState(const char* message) const {
  for_same_space(&slot_states_[0],
                 &slot_states_[slot_states_.size()],
           [](SpaceAllocator::Space*const*  beg, SpaceAllocator::Space*const*  end) {
             // int a = beg;
             auto space = *beg;
             if (!space) {
               for (auto i : range(end - beg)) {
                 (void)i;
                 printf(" ");
               }
             } else {
               if (space->size_ == 1)
                 printf("#");
               else if (space->size_ == 2)
                 printf("[]");
               else {
                 auto used = std::min((u32)space->name_.size(), space->size_ - 2);
                 auto left_ws = (space->size_ - used - 2) / 2;
                 auto right_ws = space->size_ - used - 2 - left_ws;
                 printf("[");
                 for (auto i : range(left_ws)) {
                   (void)i;
                   printf(".");
                 }
                 printf("%.*s", used, space->name_.c_str());
                 for (auto i : range(right_ws)) {
                   (void)i;
                   printf(".");
                 }
                 printf("]");
               }
             }
           });
  printf("<-- %s\n", message);
}

SpaceAllocator::~SpaceAllocator() {
  for (auto a : all_spaces_)
    delete a;

  if (memory_) {
#ifdef __WINDOWS__
    free(memory_);
#else
    munmap(memory_, slot_count_ * slot_size_);
#endif
    memory_ = nullptr;
  }
}

}  // namespace zceq_solver
