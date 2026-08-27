// Axel '0vercl0k' Souchet - August 20 2026
use std::collections::HashMap;
use std::error::Error;
use std::fs::File;
use std::io::Write;
use std::ops::{Deref, DerefMut};
use std::path::{Path, PathBuf};
use std::sync::LazyLock;
use std::{env, io};

use serde_json::Value;
use zip::ZipArchive;

struct StaticLibInfo {
    /// Path to the static library file that needs to be extracted from the zip
    /// archive (like `release/bochscpu_ffi.lib`)..
    path_in_zip: &'static str,
    /// ..and renamed as `new_filename` (like `bochscpu_ffi_Windows_AMD64.lib`).
    new_filename: &'static str,
}

impl StaticLibInfo {
    const fn new(path_in_zip: &'static str, new_filename: &'static str) -> Self {
        Self {
            path_in_zip,
            new_filename,
        }
    }
}

/// This is the API that gives us the zip URLs for the latest releases made in
/// the `bochscpu-ffi` repo.
const BXCPUFFI_LATEST_RELEASE_LINK: &str =
    "https://api.github.com/repos/yrp604/bochscpu-ffi/releases/latest";

static BXCPUFFI_RELEASE_ZIP_FILENAMES: LazyLock<HashMap<&'static str, StaticLibInfo>> =
    LazyLock::new(|| {
        macro_rules! staticlibs {
            ($( $zip_name:literal => ($staticlib_path:literal, $filename:literal) )+) => {{
                let mut h = HashMap::new();

                $(
                    assert!(
                        h.insert($zip_name, StaticLibInfo::new($staticlib_path, $filename))
                        .is_none()
                    );
                )+

                h
            }}
        }

        staticlibs!(
            "bochscpu-ffi-windows-latest-x64-MT.zip" => (
                "target/release/bochscpu_ffi.lib",
                "bochscpu_ffi_Windows_AMD64.lib"
            )
            "bochscpu-ffi-ubuntu-latest-x64.zip" => (
                "target/release/libbochscpu_ffi.a",
                "libbochscpu_ffi_Linux_x86_64.a"
            )
            "bochscpu-ffi-ubuntu-24.04-arm-arm64.zip" => (
                "target/release/libbochscpu_ffi.a",
                "libbochscpu_ffi_Linux_aarch64.a"
            )
            "bochscpu-ffi-macos-latest-arm64.zip" => (
                "target/release/libbochscpu_ffi.a",
                "libbochscpu_ffi_Darwin_arm64.a"
            )
        )
    });

type Result<T> = std::result::Result<T, Box<dyn Error>>;

/// Get the URLs to the bochscpu-ffi zip files that contains the static library
/// `wtf` needs for linking.
fn fetch_bxcpuffi_zip_assets_urls() -> Result<(String, HashMap<String, String>)> {
    let res = ureq::get(BXCPUFFI_LATEST_RELEASE_LINK)
        .header("user-agent", "Mozilla/5.0")
        .call()?;
    let body = res.into_body().read_to_string()?;
    let js: Value = serde_json::from_str(&body)?;
    let assets = js["assets"].as_array().ok_or("no assets?")?;
    let tag_name = js["tag_name"].as_str().ok_or("no tag name?")?.to_string();

    let zip_urls_by_name = assets
        .iter()
        .map(|e| {
            (
                e["name"].as_str().unwrap().to_string(),
                e["browser_download_url"].as_str().unwrap().to_string(),
            )
        })
        .collect();

    Ok((tag_name, zip_urls_by_name))
}

struct TempFilePath(PathBuf);

impl Drop for TempFilePath {
    fn drop(&mut self) {
        println!("Cleaning up {}", self.0.display());
        // fs::remove_file(&self.0).unwrap();
    }
}

impl From<PathBuf> for TempFilePath {
    fn from(value: PathBuf) -> Self {
        Self(value)
    }
}

impl AsRef<Path> for TempFilePath {
    fn as_ref(&self) -> &Path {
        &self.0
    }
}

/// Create a file in the temp directory (w/ read & write access, truncated if
/// already exists), and clean it up on drop.
struct TempFile {
    file: File,
    _path: TempFilePath,
}

impl TempFile {
    fn new(filename: impl AsRef<Path>) -> Result<Self> {
        let path = env::temp_dir().join(filename.as_ref());
        let file = File::options()
            .truncate(true)
            .write(true)
            .read(true)
            .create(true)
            .open(&path)?;

        Ok(Self {
            file,
            _path: path.into(),
        })
    }
}

impl Deref for TempFile {
    type Target = File;

    fn deref(&self) -> &Self::Target {
        &self.file
    }
}

impl DerefMut for TempFile {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.file
    }
}

fn main() -> Result<()> {
    println!("Getting the latest bochscpu-ffi assets URLs..");
    let mut header_extracted = false;
    let (tag_name, mut zip_urls) = fetch_bxcpuffi_zip_assets_urls()?;
    for (zip_filename, staticlib_info) in BXCPUFFI_RELEASE_ZIP_FILENAMES.iter() {
        let zip_url = zip_urls.remove(*zip_filename).ok_or("entry is missing")?;
        let downloaded_zip_path = env::temp_dir().join(zip_filename);
        println!(
            "Downloading {zip_url} to {}..",
            downloaded_zip_path.display()
        );

        let mut downloaded_zip = TempFile::new(downloaded_zip_path)?;
        io::copy(
            &mut ureq::get(zip_url).call()?.into_body().as_reader(),
            &mut *downloaded_zip,
        )?;

        let staticlib_path = format!("../lib/{}", staticlib_info.new_filename);
        let mut staticlib_file = File::create(&staticlib_path)?;
        let mut zip = ZipArchive::new(&*downloaded_zip)?;
        println!(
            "Extracting {} into {staticlib_path}..",
            staticlib_info.path_in_zip
        );

        io::copy(
            &mut zip.by_name(staticlib_info.path_in_zip)?,
            &mut staticlib_file,
        )?;

        if !header_extracted {
            println!("Extracting boschcpu.hpp..");
            let mut header_file = File::create("../include/bochscpu.hpp")?;
            io::copy(&mut zip.by_name("bochscpu.hpp")?, &mut header_file)?;
            header_extracted = true;
        }
    }

    // The only zip we don't 'consume' the windows-MD zip.
    assert_eq!(zip_urls.len(), 1);
    println!("Extracted all the files!");

    let mut tag = File::create("../TAG")?;
    write!(tag, "{tag_name}")?;

    Ok(())
}
