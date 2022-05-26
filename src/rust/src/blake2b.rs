// Copyright (c) 2020-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#[cxx::bridge(namespace = "blake2b")]
mod ffi {
    extern "Rust" {
        type State;

        fn init(output_len: usize, personalization: &[u8]) -> Box<State>;
        fn box_clone(&self) -> Box<State>;
        fn update(&mut self, input: &[u8]);
        fn finalize(&self, output: &mut [u8]);
    }
}

#[derive(Clone)]
struct State(blake2b_simd::State);

fn init(output_len: usize, personalization: &[u8]) -> Box<State> {
    Box::new(State(
        blake2b_simd::Params::new()
            .hash_length(output_len)
            .personal(personalization)
            .to_state(),
    ))
}

impl State {
    fn box_clone(&self) -> Box<Self> {
        Box::new(self.clone())
    }

    fn update(&mut self, input: &[u8]) {
        self.0.update(input);
    }

    fn finalize(&self, output: &mut [u8]) {
        // Allow consuming only part of the output.
        let hash = self.0.finalize();
        assert!(output.len() <= hash.as_bytes().len());
        output.copy_from_slice(&hash.as_bytes()[..output.len()]);
    }
}
