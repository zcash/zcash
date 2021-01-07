// This is mostly code copied from metrics_exporter_prometheus. It is licensed under the
// same license as the zcash codebase.

use hyper::{
    server::{conn::AddrStream, Server},
    service::{make_service_fn, service_fn},
    {Body, Error as HyperError, Response, StatusCode},
};
use metrics::SetRecorderError;
use metrics_exporter_prometheus::{PrometheusBuilder, PrometheusRecorder};
use std::future::Future;
use std::io;
use std::net::SocketAddr;
use std::thread;
use thiserror::Error as ThisError;
use tokio::{pin, runtime, select};

/// Errors that could occur while installing a Prometheus recorder/exporter.
#[derive(Debug, ThisError)]
pub enum InstallError {
    /// Creating the networking event loop did not succeed.
    #[error("failed to spawn Tokio runtime for endpoint: {0}")]
    Io(#[from] io::Error),

    /// Binding/listening to the given address did not succeed.
    #[error("failed to bind to given listen address: {0}")]
    Hyper(#[from] HyperError),

    /// Installing the recorder did not succeed.
    #[error("failed to install exporter as global recorder: {0}")]
    Recorder(#[from] SetRecorderError),
}

/// A copy of `PrometheusBuilder::build_with_exporter` that adds support for an IP address
/// or subnet allowlist.
pub(super) fn build(
    bind_address: SocketAddr,
    builder: PrometheusBuilder,
    allow_ips: Vec<ipnet::IpNet>,
) -> Result<
    (
        PrometheusRecorder,
        impl Future<Output = Result<(), HyperError>> + Send + 'static,
    ),
    InstallError,
> {
    let recorder = builder.build();
    let handle = recorder.handle();

    let server = Server::try_bind(&bind_address)?;

    let exporter = async move {
        let make_svc = make_service_fn(move |socket: &AddrStream| {
            let remote_addr = socket.remote_addr().ip();
            let allowed = allow_ips.iter().any(|subnet| subnet.contains(&remote_addr));
            let handle = handle.clone();

            async move {
                Ok::<_, HyperError>(service_fn(move |_| {
                    let handle = handle.clone();

                    async move {
                        if allowed {
                            let output = handle.render();
                            Ok(Response::new(Body::from(output)))
                        } else {
                            Response::builder()
                                .status(StatusCode::FORBIDDEN)
                                .body(Body::empty())
                        }
                    }
                }))
            }
        });

        server.serve(make_svc).await
    };

    Ok((recorder, exporter))
}

/// A copy of `PrometheusBuilder::install` that adds support for an IP address or subnet
/// allowlist.
pub(super) fn install(
    bind_address: SocketAddr,
    builder: PrometheusBuilder,
    allow_ips: Vec<ipnet::IpNet>,
) -> Result<(), InstallError> {
    let runtime = runtime::Builder::new_current_thread()
        .enable_all()
        .build()?;

    let (recorder, exporter) = {
        let _guard = runtime.enter();
        build(bind_address, builder, allow_ips)?
    };
    metrics::set_boxed_recorder(Box::new(recorder))?;

    thread::Builder::new()
        .name("zcash-prometheus".to_string())
        .spawn(move || {
            runtime.block_on(async move {
                pin!(exporter);
                loop {
                    select! {
                        _ = &mut exporter => {}
                    }
                }
            });
        })?;

    Ok(())
}
