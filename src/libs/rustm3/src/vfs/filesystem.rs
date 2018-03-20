use cap::Selector;
use col::Vec;
use com::VecSink;
use core::any::Any;
use core::fmt;
use errors::Error;
use vfs::{OpenFlags, FileHandle, FileInfo, FileMode};

pub trait FileSystem : fmt::Debug {
    fn as_any(&self) -> &Any;

    fn open(&self, path: &str, perms: OpenFlags) -> Result<FileHandle, Error>;

    fn stat(&self, path: &str) -> Result<FileInfo, Error>;

    fn mkdir(&self, path: &str, mode: FileMode) -> Result<(), Error>;
    fn rmdir(&self, path: &str) -> Result<(), Error>;

    fn link(&self, old_path: &str, new_path: &str) -> Result<(), Error>;
    fn unlink(&self, path: &str) -> Result<(), Error>;

    fn fs_type(&self) -> u8;
    fn exchange_caps(&self, vpe: Selector,
                            dels: &mut Vec<Selector>,
                            max_sel: &mut Selector) -> Result<(), Error>;
    fn serialize(&self, s: &mut VecSink);
}
