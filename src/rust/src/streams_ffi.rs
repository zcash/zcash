use std::io;
use std::ptr::NonNull;

use libc::{c_long, c_void, size_t};

pub type StreamObj = NonNull<c_void>;
pub type ReadCb =
    unsafe extern "C" fn(obj: Option<StreamObj>, pch: *mut u8, size: size_t) -> c_long;
pub type WriteCb =
    unsafe extern "C" fn(obj: Option<StreamObj>, pch: *const u8, size: size_t) -> c_long;

pub struct CppStreamReader {
    inner: Option<StreamObj>,
    cb: ReadCb,
}

impl CppStreamReader {
    pub fn from_raw_parts(inner: Option<StreamObj>, cb: ReadCb) -> Self {
        CppStreamReader { inner, cb }
    }
}

impl io::Read for CppStreamReader {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match unsafe { (self.cb)(self.inner, buf.as_mut_ptr(), buf.len()) } {
            -1 => Err(io::Error::new(io::ErrorKind::Other, "C++ stream error")),
            n => Ok(n as usize),
        }
    }
}

pub struct CppStreamWriter {
    inner: Option<StreamObj>,
    cb: WriteCb,
}

impl CppStreamWriter {
    pub fn from_raw_parts(inner: Option<StreamObj>, cb: WriteCb) -> Self {
        CppStreamWriter { inner, cb }
    }
}

impl io::Write for CppStreamWriter {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        match unsafe { (self.cb)(self.inner, buf.as_ptr(), buf.len()) } {
            -1 => Err(io::Error::new(io::ErrorKind::Other, "C++ stream error")),
            n => Ok(n as usize),
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
