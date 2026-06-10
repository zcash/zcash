// Copyright (c) 2026 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use std::io;

/// Prefix communicated across the cxx FFI boundary for lenient I/O parse failures.
pub(crate) const IO_PREFIX: &str = "zcash_parse_io: ";
/// Prefix communicated across the cxx FFI boundary for consensus-rule parse failures.
pub(crate) const CONSENSUS_PREFIX: &str = "zcash_parse_consensus: ";

/// Classifies an `io::Error` from bundle deserialization as either a lenient I/O
/// failure (truncated stream, oversize length field) or a consensus rule violation.
///
/// This mirrors the lenient cases handled explicitly in `ProcessMessages()` for
/// `std::ios_base::failure` ("end of data", "size too large").
pub(crate) fn is_io_parse_failure(err: &io::Error) -> bool {
    if err.kind() == io::ErrorKind::UnexpectedEof {
        return true;
    }

    let msg = err.to_string();
    msg.contains("end of data")
        || msg.contains("end of file")
        || msg.contains("size too large")
        || msg.contains("CompactSize too large")
}

/// Formats a bundle parse `io::Error` with a kind prefix for the C++ wrapper.
pub(crate) fn format_bundle_parse_error(err: io::Error, context: &str) -> String {
    let detail = format!("{}: {}", context, err);
    if is_io_parse_failure(&err) {
        format!("{}{}", IO_PREFIX, detail)
    } else {
        format!("{}{}", CONSENSUS_PREFIX, detail)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unexpected_eof_is_io_failure() {
        let err = io::Error::new(io::ErrorKind::UnexpectedEof, "early eof");
        assert!(is_io_parse_failure(&err));
    }

    #[test]
    fn end_of_data_from_cpp_stream_is_io_failure() {
        let err = io::Error::new(
            io::ErrorKind::Other,
            "CBaseDataStream::read(): end of data",
        );
        assert!(is_io_parse_failure(&err));
    }

    #[test]
    fn compact_size_too_large_is_io_failure() {
        let err = io::Error::new(io::ErrorKind::InvalidInput, "CompactSize too large");
        assert!(is_io_parse_failure(&err));
    }

    #[test]
    fn non_canonical_compact_size_is_consensus_failure() {
        let err = io::Error::new(io::ErrorKind::InvalidInput, "non-canonical CompactSize");
        assert!(!is_io_parse_failure(&err));
    }

    #[test]
    fn value_balance_out_of_range_is_consensus_failure() {
        let err = io::Error::new(io::ErrorKind::InvalidData, "valueBalance out of range");
        assert!(!is_io_parse_failure(&err));
    }

    #[test]
    fn format_uses_expected_prefixes() {
        let io_err = io::Error::new(io::ErrorKind::UnexpectedEof, "eof");
        let consensus_err = io::Error::new(io::ErrorKind::InvalidInput, "invalid cv");

        assert!(format_bundle_parse_error(io_err, "ctx").starts_with(IO_PREFIX));
        assert!(format_bundle_parse_error(consensus_err, "ctx").starts_with(CONSENSUS_PREFIX));
    }
}
