use std::io;
use std::pin::Pin;

#[cxx::bridge]
pub(crate) mod ffi {
    extern "C++" {
        include!("streams.h");

        #[cxx_name = "RustDataStream"]
        type RustStream;

        unsafe fn read_u8(self: Pin<&mut RustStream>, pch: *mut u8, nSize: usize) -> Result<()>;
        unsafe fn write_u8(self: Pin<&mut RustStream>, pch: *const u8, nSize: usize) -> Result<()>;
    }

    impl UniquePtr<RustStream> {}
}

pub struct CppStream<'a> {
    inner: Pin<&'a mut ffi::RustStream>,
}

impl<'a> From<Pin<&'a mut ffi::RustStream>> for CppStream<'a> {
    fn from(inner: Pin<&'a mut ffi::RustStream>) -> Self {
        Self { inner }
    }
}

impl<'a> io::Read for CppStream<'a> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        unsafe { self.inner.as_mut().read_u8(buf.as_mut_ptr(), buf.len()) }
            .map(|()| buf.len())
            .map_err(|e| io::Error::new(io::ErrorKind::Other, e))
    }
}

impl<'a> io::Write for CppStream<'a> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        unsafe { self.inner.as_mut().write_u8(buf.as_ptr(), buf.len()) }
            .map(|()| buf.len())
            .map_err(|e| io::Error::new(io::ErrorKind::Other, e))
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
