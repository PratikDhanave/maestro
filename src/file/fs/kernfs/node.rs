//! This module implements kernfs nodes.

use super::content::KernFSContent;
use crate::errno::EResult;
use crate::errno::Errno;
use crate::file::perm;
use crate::file::perm::Gid;
use crate::file::perm::Uid;
use crate::file::FileContent;
use crate::file::Mode;
use crate::time::clock;
use crate::time::clock::CLOCK_MONOTONIC;
use crate::time::unit::Timestamp;
use crate::time::unit::TimestampScale;
use crate::util::io::IO;
use core::any::Any;

/// Trait representing a node in a kernfs.
pub trait KernFSNode: Any + IO {
	/// Returns the number of hard links to the node.
	fn get_hard_links_count(&self) -> u16 {
		1
	}

	/// Sets the number of hard links to the node.
	fn set_hard_links_count(&mut self, _hard_links_count: u16) {}

	/// Returns the permissions of the file.
	fn get_mode(&self) -> Mode {
		0o444
	}

	/// Sets the permissions of the file.
	fn set_mode(&mut self, _mode: Mode) {}

	/// Returns the UID of the file's owner.
	fn get_uid(&self) -> Uid {
		perm::ROOT_UID
	}

	/// Sets the UID of the file's owner.
	fn set_uid(&mut self, _uid: Uid) {}

	/// Returns the GID of the file's owner.
	fn get_gid(&self) -> Gid {
		perm::ROOT_GID
	}

	/// Sets the GID of the file's owner.
	fn set_gid(&mut self, _gid: Gid) {}

	/// Returns the timestamp of the last access to the file.
	fn get_atime(&self) -> Timestamp {
		0
	}

	/// Sets the timestamp of the last access to the file.
	fn set_atime(&mut self, _ts: Timestamp) {}

	/// Returns the timestamp of the last modification of the file's metadata.
	fn get_ctime(&self) -> Timestamp {
		0
	}

	/// Sets the timestamp of the last modification of the file's metadata.
	fn set_ctime(&mut self, _ts: Timestamp) {}

	/// Returns the timestamp of the last modification of the file's content.
	fn get_mtime(&self) -> Timestamp {
		0
	}

	/// Sets the timestamp of the last modification of the file's content.
	fn set_mtime(&mut self, _ts: Timestamp) {}

	/// Returns an immutable reference to the node's content.
	fn get_content(&mut self) -> EResult<KernFSContent<'_>>;
}

/// Structure representing a dummy kernfs node (with the default behaviour).
///
/// This node doesn't implement regular files' content handling.
///
/// Calling `read` or `write` on this structure does nothing.
pub struct DummyKernFSNode {
	/// The number of hard links to the node.
	hard_links_count: u16,

	/// The directory's permissions.
	mode: Mode,
	/// The directory's owner user ID.
	uid: Uid,
	/// The directory's owner group ID.
	gid: Gid,

	/// Timestamp of the last modification of the metadata.
	ctime: Timestamp,
	/// Timestamp of the last modification of the file.
	mtime: Timestamp,
	/// Timestamp of the last access to the file.
	atime: Timestamp,

	/// The node's content.
	content: FileContent,
}

impl DummyKernFSNode {
	/// Creates a new node.
	///
	/// Arguments:
	/// - `mode` is the node's mode.
	/// - `uid` is the node owner's user ID.
	/// - `gid` is the node owner's group ID.
	/// - `content` is the node's content.
	pub fn new(mode: Mode, uid: Uid, gid: Gid, content: FileContent) -> Self {
		// The current timestamp
		let ts = clock::current_time(CLOCK_MONOTONIC, TimestampScale::Second).unwrap_or(0);

		Self {
			hard_links_count: 1,

			mode,
			uid,
			gid,

			ctime: ts,
			mtime: ts,
			atime: ts,

			content,
		}
	}
}

impl KernFSNode for DummyKernFSNode {
	fn get_hard_links_count(&self) -> u16 {
		self.hard_links_count
	}

	fn set_hard_links_count(&mut self, hard_links_count: u16) {
		self.hard_links_count = hard_links_count;
	}

	fn get_mode(&self) -> Mode {
		self.mode
	}

	fn set_mode(&mut self, mode: Mode) {
		self.mode = mode;
	}

	fn get_uid(&self) -> Uid {
		self.uid
	}

	fn set_uid(&mut self, uid: Uid) {
		self.uid = uid;
	}

	fn get_gid(&self) -> Gid {
		self.gid
	}

	fn set_gid(&mut self, gid: Gid) {
		self.gid = gid;
	}

	fn get_atime(&self) -> Timestamp {
		self.atime
	}

	fn set_atime(&mut self, ts: Timestamp) {
		self.atime = ts;
	}

	fn get_ctime(&self) -> Timestamp {
		self.ctime
	}

	fn set_ctime(&mut self, ts: Timestamp) {
		self.ctime = ts;
	}

	fn get_mtime(&self) -> Timestamp {
		self.mtime
	}

	fn set_mtime(&mut self, ts: Timestamp) {
		self.mtime = ts;
	}

	fn get_content(&mut self) -> EResult<KernFSContent<'_>> {
		Ok(KernFSContent::Owned(&mut self.content))
	}
}

impl IO for DummyKernFSNode {
	fn get_size(&self) -> u64 {
		0
	}

	fn read(&mut self, _offset: u64, _buff: &mut [u8]) -> Result<(u64, bool), Errno> {
		Ok((0, true))
	}

	fn write(&mut self, _offset: u64, _buff: &[u8]) -> Result<u64, Errno> {
		Ok(0)
	}

	fn poll(&mut self, _mask: u32) -> Result<u32, Errno> {
		Ok(0)
	}
}
