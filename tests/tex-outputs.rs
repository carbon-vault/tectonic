// Copyright 2016-2018 the Tectonic Project
// Licensed under the MIT License.

#[macro_use] extern crate lazy_static;
extern crate tectonic;

use std::collections::HashSet;
use std::env;
use std::sync::Mutex;

use tectonic::errors::{DefinitelySame, ErrorKind, Result};
use tectonic::engines::NoopIoEventBackend;
use tectonic::engines::tex::TexResult;
use tectonic::io::{FilesystemIo, FilesystemPrimaryInputIo, IoStack, MemoryIo};
use tectonic::io::testing::SingleInputFileIo;
use tectonic::status::NoopStatusBackend;
use tectonic::{TexEngine, XdvipdfmxEngine};

mod util;
use util::{ExpectedInfo, ensure_plain_format, test_path};

lazy_static! {
    static ref LOCK: Mutex<u8> = Mutex::new(0u8);
}

struct TestCase {
    stem: String,
    expected_result: Result<TexResult>,
    check_synctex: bool,
    check_pdf: bool,
}


impl TestCase {
    fn new(stem: &str) -> Self {
        TestCase {
            stem: stem.to_owned(),
            expected_result: Ok(TexResult::Spotless),
            check_synctex: false,
            check_pdf: false,
        }
    }

    fn check_synctex(&mut self, check_synctex: bool) -> &mut Self {
        self.check_synctex = check_synctex;
        self
    }

    fn check_pdf(&mut self, check_pdf: bool) -> &mut Self {
        self.check_pdf = check_pdf;
        self
    }

    fn expect(&mut self, result: Result<TexResult>) -> &mut Self {
        self.expected_result = result;
        self
    }

    fn expect_msg(&mut self, msg: &str) -> &mut Self {
        self.expect(Err(ErrorKind::Msg(msg.to_owned()).into()))
    }

    fn go(&self) {
        let _guard = LOCK.lock().unwrap(); // until we're thread-safe ...

        let expect_xdv = self.expected_result.is_ok();

        let mut p = test_path(&[]);

        // IoProvider for the format file; with magic to generate the format
        // on-the-fly if needed.
        let mut fmt = SingleInputFileIo::new(&ensure_plain_format().expect("couldn't write format file"));

        // Set up some useful paths, and the IoProvider for the primary input file.
        p.push("tex-outputs");
        p.push(&self.stem);
        p.set_extension("tex");
        let texname = p.file_name().unwrap().to_str().unwrap().to_owned();
        let mut tex = FilesystemPrimaryInputIo::new(&p);

        p.set_extension("xdv");
        let xdvname = p.file_name().unwrap().to_str().unwrap().to_owned();

        p.set_extension("pdf");
        let pdfname = p.file_name().unwrap().to_str().unwrap().to_owned();

        // MemoryIo layer that will accept the outputs.
        let mut mem = MemoryIo::new(true);

        // We only need the assets when running xdvipdfmx, but due to how
        // ownership works with IoStacks, it's easier to just unconditionally
        // add this layer.
        let mut assets = FilesystemIo::new(&test_path(&["assets"]), false, false, HashSet::new());

        let expected_log = ExpectedInfo::read_with_extension(&mut p, "log");

        // Run the engine(s)!
        let res = {
            let mut io = IoStack::new(vec![&mut mem, &mut tex, &mut fmt, &mut assets]);
            let mut events = NoopIoEventBackend::new();
            let mut status = NoopStatusBackend::new();

            let tex_res = TexEngine::new()
                .process(&mut io, &mut events, &mut status, "plain.fmt", &texname);

            if self.check_pdf && tex_res.definitely_same(&Ok(TexResult::Spotless)) {
                // While the xdv and log output is deterministic without setting
                // SOURCE_DATE_EPOCH, xdvipdfmx uses the current date in various places.
                env::set_var("SOURCE_DATE_EPOCH", "1456304492"); // TODO: default to deterministic behaviour

                XdvipdfmxEngine::new()
                    .with_compression(false)
                    .with_deterministic_tags(true)
                    .process(&mut io, &mut events, &mut status, &xdvname, &pdfname)
                    .unwrap();
            }

            tex_res
        };

        if !res.definitely_same(&self.expected_result) {
            panic!(format!("expected TeX result {:?}, got {:?}", self.expected_result, res));
        }

        // Check that outputs match expectations.

        let files = mem.files.borrow();

        expected_log.test_from_collection(&files);

        if expect_xdv {
            ExpectedInfo::read_with_extension(&mut p, "xdv").test_from_collection(&files);
        }

        if self.check_synctex {
            ExpectedInfo::read_with_extension_gz(&mut p, "synctex.gz").test_from_collection(&files);
        }

        if self.check_pdf {
            ExpectedInfo::read_with_extension(&mut p, "pdf").test_from_collection(&files);
        }
    }
}


// Keep these alphabetized.

#[test]
fn md5_of_hello() { TestCase::new("md5_of_hello").check_pdf(true).go() }

#[test]
fn negative_roman_numeral() { TestCase::new("negative_roman_numeral").go() }

#[test]
fn tex_logo() { TestCase::new("tex_logo").go() }

#[test]
fn pdfoutput() { TestCase::new("pdfoutput").go() }

#[test]
fn synctex() { TestCase::new("synctex").check_synctex(true).go() }

#[test]
fn unicode_file_name() { TestCase::new("hallöchen 🐨 welt 🌍.tex").expect(Ok(TexResult::Warnings)).go() }

#[test]
fn tectoniccodatokens_errinside() {
    TestCase::new("tectoniccodatokens_errinside")
        .expect_msg("halted on potentially-recoverable error as specified")
        .go()
}

#[test]
fn tectoniccodatokens_noend() {
    TestCase::new("tectoniccodatokens_noend")
        .expect_msg("*** (job aborted, no legal \\end found)")
        .go()
}

#[test]
fn tectoniccodatokens_ok() { TestCase::new("tectoniccodatokens_ok").go() }

#[test]
fn the_letter_a() { TestCase::new("the_letter_a").check_pdf(true).go() }
