use std::io;
use std::pin::Pin;

#[cxx::bridge]
pub(crate) mod ffi {
    extern "C++" {
        include!("hash.h");
        include!("streams.h");

        #[cxx_name = "RustDataStream"]
        type RustStream;
        unsafe fn read_u8(self: Pin<&mut RustStream>, pch: *mut u8, nSize: usize) -> Result<()>;
        unsafe fn write_u8(self: Pin<&mut RustStream>, pch: *const u8, nSize: usize) -> Result<()>;

        type CAutoFile;
        unsafe fn read_u8(self: Pin<&mut CAutoFile>, pch: *mut u8, nSize: usize) -> Result<()>;
        unsafe fn write_u8(self: Pin<&mut CAutoFile>, pch: *const u8, nSize: usize) -> Result<()>;

        type CBufferedFile;
        unsafe fn read_u8(self: Pin<&mut CBufferedFile>, pch: *mut u8, nSize: usize) -> Result<()>;

        type CHashWriter;
        unsafe fn write_u8(self: Pin<&mut CHashWriter>, pch: *const u8, nSize: usize)
            -> Result<()>;

        type CBLAKE2bWriter;
        unsafe fn write_u8(
            self: Pin<&mut CBLAKE2bWriter>,
            pch: *const u8,
            nSize: usize,
        ) -> Result<()>;

        type CSizeComputer;
        unsafe fn write_u8(
            self: Pin<&mut CSizeComputer>,
            pch: *const u8,
            nSize: usize,
        ) -> Result<()>;
    }

    impl UniquePtr<RustStream> {}
    impl UniquePtr<CAutoFile> {}
    impl UniquePtr<CBufferedFile> {}
    impl UniquePtr<CHashWriter> {}
    impl UniquePtr<CBLAKE2bWriter> {}
    impl UniquePtr<CSizeComputer> {}
}

pub(crate) fn from_data(stream: Pin<&mut ffi::RustStream>) -> Box<CppStream<'_>> {
    Box::new(CppStream::Data(stream))
}

pub(crate) fn from_auto_file(file: Pin<&mut ffi::CAutoFile>) -> Box<CppStream<'_>> {
    Box::new(CppStream::AutoFile(file))
}

pub(crate) fn from_buffered_file(file: Pin<&mut ffi::CBufferedFile>) -> Box<CppStream<'_>> {
    Box::new(CppStream::BufferedFile(file))
}

pub(crate) fn from_hash_writer(writer: Pin<&mut ffi::CHashWriter>) -> Box<CppStream<'_>> {
    Box::new(CppStream::Hash(writer))
}

pub(crate) fn from_blake2b_writer(writer: Pin<&mut ffi::CBLAKE2bWriter>) -> Box<CppStream<'_>> {
    Box::new(CppStream::Blake2b(writer))
}

pub(crate) fn from_size_computer(sc: Pin<&mut ffi::CSizeComputer>) -> Box<CppStream<'_>> {
    Box::new(CppStream::Size(sc))
}

pub(crate) enum CppStream<'a> {
    Data(Pin<&'a mut ffi::RustStream>),
    AutoFile(Pin<&'a mut ffi::CAutoFile>),
    BufferedFile(Pin<&'a mut ffi::CBufferedFile>),
    Hash(Pin<&'a mut ffi::CHashWriter>),
    Blake2b(Pin<&'a mut ffi::CBLAKE2bWriter>),
    Size(Pin<&'a mut ffi::CSizeComputer>),
}

impl<'a> io::Read for CppStream<'a> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let pch = buf.as_mut_ptr();
        let len = buf.len();
        match self {
            CppStream::Data(inner) => unsafe { inner.as_mut().read_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::AutoFile(inner) => unsafe { inner.as_mut().read_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::BufferedFile(inner) => unsafe { inner.as_mut().read_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::Hash(_) => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "Cannot read from CHashWriter",
            )),
            CppStream::Blake2b(_) => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "Cannot read from CBLAKE2bWriter",
            )),
            CppStream::Size(_) => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "Cannot read from CSizeComputer",
            )),
        }
    }
}

impl<'a> io::Write for CppStream<'a> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let pch = buf.as_ptr();
        let len = buf.len();
        match self {
            CppStream::Data(inner) => unsafe { inner.as_mut().write_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::AutoFile(inner) => unsafe { inner.as_mut().write_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::BufferedFile(_) => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "Cannot write to CBufferedFile",
            )),
            CppStream::Hash(inner) => unsafe { inner.as_mut().write_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::Blake2b(inner) => unsafe { inner.as_mut().write_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
            CppStream::Size(inner) => unsafe { inner.as_mut().write_u8(pch, len) }
                .map(|()| buf.len())
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e)),
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
