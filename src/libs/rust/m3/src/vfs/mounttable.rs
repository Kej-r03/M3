/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use core::fmt;

use crate::cap::Selector;
use crate::cell::RefCell;
use crate::col::{String, ToString, Vec};
use crate::errors::{Code, Error};
use crate::pes::StateSerializer;
use crate::rc::Rc;
use crate::serialize::Source;
use crate::session::M3FS;
use crate::vfs::FileSystem;

/// A reference to a file system.
pub type FSHandle = Rc<RefCell<dyn FileSystem>>;

/// Represents a mount point
pub struct MountPoint {
    path: String,
    fs: FSHandle,
}

impl MountPoint {
    /// Creates a new mount point for given path and file system.
    pub fn new(path: &str, fs: FSHandle) -> MountPoint {
        MountPoint {
            path: path.to_string(),
            fs,
        }
    }
}

/// The table of mount points.
#[derive(Default)]
pub struct MountTable {
    mounts: Vec<MountPoint>,
}

impl MountTable {
    /// Adds a new mount point at given path and given file system to the table.
    pub fn add(&mut self, path: &str, fs: FSHandle) -> Result<(), Error> {
        if self.path_to_idx(path).is_some() {
            return Err(Error::new(Code::Exists));
        }

        let pos = self.insert_pos(path);
        self.mounts.insert(pos, MountPoint::new(path, fs));
        Ok(())
    }

    /// Returns the file system mounted exactly at the given path.
    pub fn get_by_path(&self, path: &str) -> Option<FSHandle> {
        self.path_to_idx(path).map(|i| self.mounts[i].fs.clone())
    }

    /// Returns the mount point with index `mid`.
    pub fn get_by_index(&self, mid: usize) -> Option<FSHandle> {
        self.mounts.get(mid).map(|mp| mp.fs.clone())
    }

    /// Resolves the given path to the file system image and the offset of the mount point within
    /// the path.
    pub fn resolve(&self, path: &str) -> Result<(FSHandle, usize), Error> {
        for m in &self.mounts {
            if path.starts_with(m.path.as_str()) {
                return Ok((m.fs.clone(), m.path.len()));
            }
        }
        Err(Error::new(Code::NoSuchFile))
    }

    /// Removes the mount point at `path` from the table.
    pub fn remove(&mut self, path: &str) -> Result<(), Error> {
        match self.path_to_idx(path) {
            Some(i) => {
                self.mounts.remove(i);
                Ok(())
            },
            None => Err(Error::new(Code::NoSuchFile)),
        }
    }

    pub(crate) fn collect_caps(
        &self,
        vpe: Selector,
        dels: &mut Vec<Selector>,
        max_sel: &mut Selector,
    ) -> Result<(), Error> {
        for m in &self.mounts {
            m.fs.borrow().exchange_caps(vpe, dels, max_sel)?;
        }
        Ok(())
    }

    pub(crate) fn serialize(&self, s: &mut StateSerializer) {
        let count = self.mounts.len();
        s.push_word(count as u64);

        for m in &self.mounts {
            let fs = m.fs.borrow();
            let fs_type = fs.fs_type();
            s.push_str(&m.path);
            s.push_word(fs_type as u64);
            fs.serialize(s);
        }
    }

    pub(crate) fn unserialize(s: &mut Source) -> MountTable {
        let mut mt = MountTable::default();

        let count = s.pop().unwrap();
        for _ in 0..count {
            let path: String = s.pop().unwrap();
            let fs_type: u8 = s.pop().unwrap();
            mt.add(&path, match fs_type {
                b'M' => M3FS::unserialize(s),
                _ => panic!("Unexpected fs type {}", fs_type),
            })
            .unwrap();
        }

        mt
    }

    fn path_to_idx(&self, path: &str) -> Option<usize> {
        // TODO support imperfect paths
        assert!(path.starts_with('/'));
        assert!(path.ends_with('/'));
        assert!(!path.contains(".."));

        for (i, m) in self.mounts.iter().enumerate() {
            if m.path == path {
                return Some(i);
            }
        }
        None
    }

    fn insert_pos(&self, path: &str) -> usize {
        let comp_count = Self::path_comps(path);
        for (i, m) in self.mounts.iter().enumerate() {
            let cnt = Self::path_comps(m.path.as_str());
            if comp_count > cnt {
                return i;
            }
        }
        self.mounts.len()
    }

    fn path_comps(path: &str) -> usize {
        let mut comp_count = path.chars().filter(|&c| c == '/').count();
        if !path.ends_with('/') {
            comp_count += 1;
        }
        comp_count
    }
}

impl fmt::Debug for MountTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(f, "MountTable[")?;
        for m in self.mounts.iter() {
            writeln!(f, "  {} -> {:?}", m.path, m.fs.borrow())?;
        }
        write!(f, "]")
    }
}
