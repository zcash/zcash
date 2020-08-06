use libc::c_char;
use std::ffi::CStr;
use std::path::Path;
use std::slice;
use std::str;
use std::sync::atomic::{AtomicUsize, Ordering};
use tracing::{
    callsite::{Callsite, Identifier},
    field::{FieldSet, Value},
    level_enabled,
    metadata::Kind,
    span::Entered,
    subscriber::{Interest, Subscriber},
    Event, Metadata, Span,
};
use tracing_appender::non_blocking::WorkerGuard;
use tracing_core::Once;
use tracing_subscriber::{
    filter::EnvFilter,
    layer::Layer,
    reload::{self, Handle},
};

#[cfg(not(target_os = "windows"))]
use std::ffi::OsStr;
#[cfg(not(target_os = "windows"))]
use std::os::unix::ffi::OsStrExt;

#[cfg(target_os = "windows")]
use std::ffi::OsString;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStringExt;

trait ReloadHandle {
    fn reload(&self, new_filter: EnvFilter) -> Result<(), reload::Error>;
}

impl<L, S> ReloadHandle for Handle<L, S>
where
    L: From<EnvFilter> + Layer<S> + 'static,
    S: Subscriber,
{
    fn reload(&self, new_filter: EnvFilter) -> Result<(), reload::Error> {
        self.reload(new_filter)
    }
}

pub struct TracingHandle {
    _file_guard: Option<WorkerGuard>,
    reload_handle: Box<dyn ReloadHandle>,
}

#[no_mangle]
pub extern "C" fn tracing_init(
    #[cfg(not(target_os = "windows"))] log_path: *const u8,
    #[cfg(target_os = "windows")] log_path: *const u16,
    log_path_len: usize,
    initial_filter: *const c_char,
    log_timestamps: bool,
) -> *mut TracingHandle {
    let initial_filter = unsafe { CStr::from_ptr(initial_filter) }
        .to_str()
        .expect("initial filter should be a valid string");

    if log_path.is_null() {
        tracing_init_stdout(initial_filter, log_timestamps)
    } else {
        let log_path = unsafe { slice::from_raw_parts(log_path, log_path_len) };

        #[cfg(not(target_os = "windows"))]
        let log_path = Path::new(OsStr::from_bytes(log_path));

        #[cfg(target_os = "windows")]
        let log_path = Path::new(OsString::from_wide(log_path));

        tracing_init_file(log_path, initial_filter, log_timestamps)
    }
}

fn tracing_init_stdout(initial_filter: &str, log_timestamps: bool) -> *mut TracingHandle {
    let builder = tracing_subscriber::fmt()
        .with_ansi(true)
        .with_env_filter(initial_filter);

    let reload_handle = if log_timestamps {
        let builder = builder.with_filter_reloading();
        let reload_handle = builder.reload_handle();
        builder.init();
        Box::new(reload_handle) as Box<dyn ReloadHandle>
    } else {
        let builder = builder.without_time().with_filter_reloading();
        let reload_handle = builder.reload_handle();
        builder.init();
        Box::new(reload_handle) as Box<dyn ReloadHandle>
    };

    Box::into_raw(Box::new(TracingHandle {
        _file_guard: None,
        reload_handle,
    }))
}

fn tracing_init_file(
    log_path: &Path,
    initial_filter: &str,
    log_timestamps: bool,
) -> *mut TracingHandle {
    let file_appender =
        tracing_appender::rolling::never(log_path.parent().unwrap(), log_path.file_name().unwrap());
    let (non_blocking, file_guard) = tracing_appender::non_blocking(file_appender);

    let builder = tracing_subscriber::fmt()
        .with_ansi(false)
        .with_env_filter(initial_filter)
        .with_writer(non_blocking);

    let reload_handle = if log_timestamps {
        let builder = builder.with_filter_reloading();
        let reload_handle = builder.reload_handle();
        builder.init();
        Box::new(reload_handle) as Box<dyn ReloadHandle>
    } else {
        let builder = builder.without_time().with_filter_reloading();
        let reload_handle = builder.reload_handle();
        builder.init();
        Box::new(reload_handle) as Box<dyn ReloadHandle>
    };

    Box::into_raw(Box::new(TracingHandle {
        _file_guard: Some(file_guard),
        reload_handle,
    }))
}

#[no_mangle]
pub extern "C" fn tracing_free(handle: *mut TracingHandle) {
    drop(unsafe { Box::from_raw(handle) });
}

#[no_mangle]
pub extern "C" fn tracing_reload(handle: *mut TracingHandle, new_filter: *const c_char) -> bool {
    let handle = unsafe { &mut *handle };

    match unsafe { CStr::from_ptr(new_filter) }
        .to_str()
        .map(EnvFilter::new)
    {
        Err(e) => {
            tracing::error!("New filter is not valid UTF-8: {}", e);
            false
        }
        Ok(new_filter) => {
            if let Err(e) = handle.reload_handle.reload(new_filter) {
                tracing::error!("Filter reload failed: {}", e);
                false
            } else {
                true
            }
        }
    }
}

pub struct FfiCallsite {
    interest: AtomicUsize,
    meta: Option<Metadata<'static>>,
    registration: Once,
    fields: Vec<&'static str>,
}

impl FfiCallsite {
    fn new(fields: Vec<&'static str>) -> Self {
        FfiCallsite {
            interest: AtomicUsize::new(0),
            meta: None,
            registration: Once::new(),
            fields,
        }
    }

    #[inline(always)]
    fn is_enabled(&self) -> bool {
        let interest = self.interest();
        if interest.is_always() {
            return true;
        }
        if interest.is_never() {
            return false;
        }

        tracing::dispatcher::get_default(|current| current.enabled(self.metadata()))
    }

    #[inline(always)]
    pub fn register(&'static self) {
        self.registration
            .call_once(|| tracing::callsite::register(self));
    }

    #[inline(always)]
    fn interest(&self) -> Interest {
        match self.interest.load(Ordering::Relaxed) {
            0 => Interest::never(),
            2 => Interest::always(),
            _ => Interest::sometimes(),
        }
    }
}

impl Callsite for FfiCallsite {
    fn set_interest(&self, interest: Interest) {
        let interest = match () {
            _ if interest.is_never() => 0,
            _ if interest.is_always() => 2,
            _ => 1,
        };
        self.interest.store(interest, Ordering::SeqCst);
    }

    #[inline(always)]
    fn metadata(&self) -> &Metadata<'static> {
        self.meta.as_ref().unwrap()
    }
}

#[no_mangle]
pub extern "C" fn tracing_callsite(
    name: *const c_char,
    target: *const c_char,
    level: *const c_char,
    file: *const c_char,
    line: u32,
    fields: *const *const c_char,
    fields_len: usize,
    is_span: bool,
) -> *mut FfiCallsite {
    let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
    let target = unsafe { CStr::from_ptr(target) }.to_str().unwrap();
    let level = unsafe { CStr::from_ptr(level) }.to_str().unwrap();
    let file = unsafe { CStr::from_ptr(file) }.to_str().unwrap();

    let fields = unsafe { slice::from_raw_parts(fields, fields_len) };
    let fields = fields
        .iter()
        .map(|&p| unsafe { CStr::from_ptr(p) })
        .map(|cs| cs.to_str())
        .collect::<Result<Vec<_>, _>>()
        .unwrap();

    let level = level.parse().unwrap();

    // We immediately convert the new callsite into a pointer to erase lifetime
    // information. The caller MUST ensure that the callsite is stored statically.
    let site = Box::into_raw(Box::new(FfiCallsite::new(fields)));
    let site_ref = unsafe { &*site };

    let meta: Metadata<'static> = Metadata::new(
        name,
        target,
        level,
        Some(file),
        Some(line),
        None,
        FieldSet::new(&site_ref.fields, Identifier(site_ref)),
        if is_span { Kind::SPAN } else { Kind::EVENT },
    );
    unsafe { &mut *site }.meta = Some(meta);

    site_ref.register();
    site
}

macro_rules! repeat {
    (1, $val:expr) => {
        [$val]
    };
    (2, $val:expr) => {
        [$val, $val]
    };
    (3, $val:expr) => {
        [$val, $val, $val]
    };
    (4, $val:expr) => {
        [$val, $val, $val, $val]
    };
    (5, $val:expr) => {
        [$val, $val, $val, $val, $val]
    };
    (6, $val:expr) => {
        [$val, $val, $val, $val, $val, $val]
    };
    (7, $val:expr) => {
        [$val, $val, $val, $val, $val, $val, $val]
    };
    (8, $val:expr) => {
        [$val, $val, $val, $val, $val, $val, $val, $val]
    };
    (9, $val:expr) => {
        [$val, $val, $val, $val, $val, $val, $val, $val, $val]
    };
    (10, $val:expr) => {
        [$val, $val, $val, $val, $val, $val, $val, $val, $val, $val]
    };
    (11, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (12, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (13, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (14, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (15, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val,
        ]
    };
    (16, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val,
        ]
    };
    (17, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val,
        ]
    };
    (18, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val,
        ]
    };
    (19, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val,
        ]
    };
    (20, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val,
        ]
    };
    (21, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (22, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (23, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (24, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (25, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (26, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (27, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (28, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
        ]
    };
    (29, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val,
        ]
    };
    (30, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val,
        ]
    };
    (31, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val,
        ]
    };
    (32, $val:expr) => {
        [
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val, $val,
            $val, $val, $val, $val,
        ]
    };
}

#[no_mangle]
pub extern "C" fn tracing_span_create(callsite: *const FfiCallsite) -> *mut Span {
    let callsite = unsafe { &*callsite };

    let meta = callsite.metadata();
    assert!(meta.is_span());

    let span = if level_enabled!(*meta.level()) && callsite.is_enabled() {
        Span::new(meta, &meta.fields().value_set(&[]))
    } else {
        Span::none()
    };

    Box::into_raw(Box::new(span))
}

#[no_mangle]
pub extern "C" fn tracing_span_clone(span: *const Span) -> *mut Span {
    unsafe { span.as_ref() }
        .map(|span| Box::into_raw(Box::new(span.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn tracing_span_free(span: *mut Span) {
    if !span.is_null() {
        drop(unsafe { Box::from_raw(span) });
    }
}

#[no_mangle]
pub extern "C" fn tracing_span_enter(span: *const Span) -> *mut Entered<'static> {
    unsafe { span.as_ref() }
        .map(|span| Box::into_raw(Box::new(span.enter())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn tracing_span_exit(guard: *mut Entered) {
    if !guard.is_null() {
        drop(unsafe { Box::from_raw(guard) });
    }
}

#[no_mangle]
pub extern "C" fn tracing_log(
    callsite: *const FfiCallsite,
    field_values: *const *const c_char,
    fields_len: usize,
) {
    let callsite = unsafe { &*callsite };
    let field_values = unsafe { slice::from_raw_parts(field_values, fields_len) };

    let meta = callsite.metadata();
    assert!(meta.is_event());

    if level_enabled!(*meta.level()) && callsite.is_enabled() {
        let mut fi = meta.fields().iter();
        let mut vi = field_values
            .iter()
            .map(|&p| unsafe { CStr::from_ptr(p) })
            .map(|cs| cs.to_string_lossy());

        macro_rules! dispatch {
            ($n:tt) => {
                Event::dispatch(
                    meta,
                    &meta.fields().value_set(&repeat!(
                        $n,
                        (
                            &fi.next().unwrap(),
                            Some(&vi.next().unwrap().as_ref() as &dyn Value)
                        )
                    )),
                )
            };
        }

        // https://github.com/tokio-rs/tracing/issues/782 might help improve things here.
        match field_values.len() {
            1 => dispatch!(1),
            2 => dispatch!(2),
            3 => dispatch!(3),
            4 => dispatch!(4),
            5 => dispatch!(5),
            6 => dispatch!(6),
            7 => dispatch!(7),
            8 => dispatch!(8),
            9 => dispatch!(9),
            10 => dispatch!(10),
            11 => dispatch!(11),
            12 => dispatch!(12),
            13 => dispatch!(13),
            14 => dispatch!(14),
            15 => dispatch!(15),
            16 => dispatch!(16),
            17 => dispatch!(17),
            18 => dispatch!(18),
            19 => dispatch!(19),
            20 => dispatch!(20),
            21 => dispatch!(21),
            22 => dispatch!(22),
            23 => dispatch!(23),
            24 => dispatch!(24),
            25 => dispatch!(25),
            26 => dispatch!(26),
            27 => dispatch!(27),
            28 => dispatch!(28),
            29 => dispatch!(29),
            30 => dispatch!(30),
            31 => dispatch!(31),
            32 => dispatch!(32),
            _ => unimplemented!(),
        }
    }
}
