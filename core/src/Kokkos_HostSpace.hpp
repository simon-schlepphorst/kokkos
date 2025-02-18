//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER

#ifndef KOKKOS_IMPL_PUBLIC_INCLUDE
#include <Kokkos_Macros.hpp>
static_assert(false,
              "Including non-public Kokkos header files is not allowed.");
#endif
#ifndef KOKKOS_HOSTSPACE_HPP
#define KOKKOS_HOSTSPACE_HPP

#include <cstring>
#include <string>
#include <iosfwd>
#include <typeinfo>

#include <Kokkos_Core_fwd.hpp>
#include <Kokkos_Concepts.hpp>
#include <Kokkos_MemoryTraits.hpp>

#include <impl/Kokkos_Traits.hpp>
#include <impl/Kokkos_Error.hpp>
#include <impl/Kokkos_SharedAlloc.hpp>
#include <impl/Kokkos_Tools.hpp>

#include "impl/Kokkos_HostSpace_deepcopy.hpp"

/*--------------------------------------------------------------------------*/

namespace Kokkos {
/// \class HostSpace
/// \brief Memory management for host memory.
///
/// HostSpace is a memory space that governs host memory.  "Host"
/// memory means the usual CPU-accessible memory.
class HostSpace {
 public:
  //! Tag this class as a kokkos memory space
  using memory_space = HostSpace;
  using size_type    = size_t;

  /// \typedef execution_space
  /// \brief Default execution space for this memory space.
  ///
  /// Every memory space has a default execution space.  This is
  /// useful for things like initializing a View (which happens in
  /// parallel using the View's default execution space).
  using execution_space = DefaultHostExecutionSpace;

  //! This memory space preferred device_type
  using device_type = Kokkos::Device<execution_space, memory_space>;

  HostSpace()                     = default;
  HostSpace(HostSpace&& rhs)      = default;
  HostSpace(const HostSpace& rhs) = default;
  HostSpace& operator=(HostSpace&&) = default;
  HostSpace& operator=(const HostSpace&) = default;
  ~HostSpace()                           = default;

#ifdef KOKKOS_ENABLE_DEPRECATED_CODE_4
  /**\brief  Non-default memory space instance to choose allocation mechansim,
   * if available */

#if defined(KOKKOS_COMPILER_GNU) && KOKKOS_COMPILER_GNU < 1100
  // We see deprecation warnings even when not using the deprecated
  // HostSpace constructor below when using gcc before release 11.
  enum
#else
  enum KOKKOS_DEPRECATED
#endif
      AllocationMechanism {
        STD_MALLOC,
        POSIX_MEMALIGN,
        POSIX_MMAP,
        INTEL_MM_ALLOC
      };

  KOKKOS_DEPRECATED
  explicit HostSpace(const AllocationMechanism&);
#endif

  /**\brief  Allocate untracked memory in the space */
  void* allocate(const size_t arg_alloc_size) const;
  void* allocate(const char* arg_label, const size_t arg_alloc_size,
                 const size_t arg_logical_size = 0) const;

  /**\brief  Deallocate untracked memory in the space */
  void deallocate(void* const arg_alloc_ptr, const size_t arg_alloc_size) const;
  void deallocate(const char* arg_label, void* const arg_alloc_ptr,
                  const size_t arg_alloc_size,
                  const size_t arg_logical_size = 0) const;

 private:
  void* impl_allocate(const char* arg_label, const size_t arg_alloc_size,
                      const size_t arg_logical_size = 0,
                      const Kokkos::Tools::SpaceHandle =
                          Kokkos::Tools::make_space_handle(name())) const;
  void impl_deallocate(const char* arg_label, void* const arg_alloc_ptr,
                       const size_t arg_alloc_size,
                       const size_t arg_logical_size = 0,
                       const Kokkos::Tools::SpaceHandle =
                           Kokkos::Tools::make_space_handle(name())) const;

 public:
  /**\brief Return Name of the MemorySpace */
  static constexpr const char* name() { return m_name; }

 private:
  static constexpr const char* m_name = "Host";
};

}  // namespace Kokkos

//----------------------------------------------------------------------------

namespace Kokkos {

namespace Impl {

static_assert(Kokkos::Impl::MemorySpaceAccess<Kokkos::HostSpace,
                                              Kokkos::HostSpace>::assignable);

template <typename S>
struct HostMirror {
 private:
  // If input execution space can access HostSpace then keep it.
  // Example: Kokkos::OpenMP can access, Kokkos::Cuda cannot
  enum {
    keep_exe = Kokkos::Impl::MemorySpaceAccess<
        typename S::execution_space::memory_space,
        Kokkos::HostSpace>::accessible
  };

  // If HostSpace can access memory space then keep it.
  // Example:  Cannot access Kokkos::CudaSpace, can access Kokkos::CudaUVMSpace
  enum {
    keep_mem =
        Kokkos::Impl::MemorySpaceAccess<Kokkos::HostSpace,
                                        typename S::memory_space>::accessible
  };

 public:
  using Space = std::conditional_t<
      keep_exe && keep_mem, S,
      std::conditional_t<keep_mem,
                         Kokkos::Device<Kokkos::HostSpace::execution_space,
                                        typename S::memory_space>,
                         Kokkos::HostSpace>>;
};

}  // namespace Impl

}  // namespace Kokkos

//----------------------------------------------------------------------------

KOKKOS_IMPL_SHARED_ALLOCATION_SPECIALIZATION(Kokkos::HostSpace);

//----------------------------------------------------------------------------

namespace Kokkos {

namespace Impl {

template <>
struct DeepCopy<HostSpace, HostSpace, DefaultHostExecutionSpace> {
  DeepCopy(void* dst, const void* src, size_t n) {
    hostspace_parallel_deepcopy(dst, src, n);
  }

  DeepCopy(const DefaultHostExecutionSpace& exec, void* dst, const void* src,
           size_t n) {
    hostspace_parallel_deepcopy_async(exec, dst, src, n);
  }
};

template <class ExecutionSpace>
struct DeepCopy<HostSpace, HostSpace, ExecutionSpace> {
  DeepCopy(void* dst, const void* src, size_t n) {
    hostspace_parallel_deepcopy(dst, src, n);
  }

  DeepCopy(const ExecutionSpace& exec, void* dst, const void* src, size_t n) {
    exec.fence(
        "Kokkos::Impl::DeepCopy<HostSpace, HostSpace, "
        "ExecutionSpace>::DeepCopy: fence before copy");
    hostspace_parallel_deepcopy_async(dst, src, n);
  }
};

}  // namespace Impl

}  // namespace Kokkos

#endif  // #define KOKKOS_HOSTSPACE_HPP
