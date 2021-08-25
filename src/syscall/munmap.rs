//! The `munmap` system call allows the process to free memory that was allocated with `mmap`.

use core::ffi::c_void;
use core::intrinsics::wrapping_add;
use crate::errno::Errno;
use crate::errno;
use crate::memory;
use crate::process::Process;
use crate::util::math;
use crate::util;

/// The implementation of the `munmap` syscall.
pub fn munmap(regs: &util::Regs) -> Result<i32, Errno> {
	let addr = regs.ebx as *mut c_void;
	let length = regs.ecx as usize;

	let mut mutex = Process::get_current().unwrap();
	let mut guard = mutex.lock(false);
	let proc = guard.get_mut();
	let mem_space = proc.get_mem_space_mut();

	let addr = util::down_align(addr, memory::PAGE_SIZE);
	let pages = math::ceil_division(length, memory::PAGE_SIZE);
	let length = pages * memory::PAGE_SIZE;

	// Checking for overflow
	let end = wrapping_add(addr as usize, length);
	if end < addr as usize {
		return Err(errno::EINVAL);
	}

	// Prevent from unmapping kernel memory
	if (addr as usize) >= (memory::PROCESS_END as usize)
		|| end > (memory::PROCESS_END as usize) {
		return Err(errno::EINVAL);
	}

	mem_space.unmap(addr, pages)?;
	Ok(0)
}
