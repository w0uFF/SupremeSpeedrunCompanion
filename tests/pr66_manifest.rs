//! SSC-owned test, copied only into a private reviewed PR66 source tree.
use std::{collections::HashMap, fs, path::Path};
use supreme_hook_config::{ModConfig, ModsState, save_mods_state, dll::{load_dll_plan, validate_x86_dll}};
use tempfile::tempdir;

#[test]
fn ssc_manifest_and_native_plan() {
    let dir = tempdir().unwrap();
    let root = dir.path().join("ssc");
    fs::create_dir(&root).unwrap();
    let manifest = root.join("mod.json");
    fs::write(&manifest, include_bytes!("ssc_manifest.json")).unwrap();
    let (config, warnings) = ModConfig::load_from_path_with_warnings(&manifest).unwrap();
    assert!(warnings.is_empty(), "{warnings:?}");
    config.validate().unwrap();
    assert_eq!(config.parameters.len(), 21);
    assert!(config.rules.is_empty());
    let dll = std::env::var_os("SSC_TEST_DLL").expect("SSC_TEST_DLL absolute path");
    assert!(Path::new(&dll).is_absolute());
    let bytes = fs::read(&dll).unwrap();
    validate_x86_dll(&bytes).unwrap();
    fs::write(root.join("SSCNative.dll"), bytes).unwrap();
    let mut state = ModsState::default();
    state.active.order = vec!["ssc".into()];
    for enabled in [false, true] {
        state.active.enabled = if enabled { vec!["ssc".into()] } else { vec![] };
        save_mods_state(dir.path(), &state).unwrap();
        let (plan, errors) = load_dll_plan(dir.path()).unwrap();
        assert!(errors.is_empty() && plan.is_empty());
    }
    state.active.configuration.insert("ssc".into(), HashMap::from([("load_diagnostic".into(), true.into())]));
    save_mods_state(dir.path(), &state).unwrap();
    let (plan, errors) = load_dll_plan(dir.path()).unwrap();
    assert!(errors.is_empty());
    assert_eq!(plan.len(), 1);
    assert_eq!(plan[0].initialize.as_deref(), Some("SSCInitialize"));
    state.active.order.clear();
    save_mods_state(dir.path(), &state).unwrap();
    assert!(load_dll_plan(dir.path()).unwrap().0.is_empty());
}

// Uses the exact upstream serializer, not a handcrafted mods.json. The preview
// is a standalone SSC process and cannot enable any native game capabilities.
#[test]
fn ssc_native_preview_from_upstream_state() {
    let Some(preview) = std::env::var_os("SSC_CONFIG_PREVIEW") else {
        println!("NOT_RUN: no native configuration preview supplied");
        return;
    };
    let dir = tempdir().unwrap();
    let profile_dir = dir.path().join("profiles");
    let mods_dir = dir.path().join("mods");
    fs::create_dir(&profile_dir).unwrap();
    fs::create_dir(&mods_dir).unwrap();
    let manifest: serde_json::Value = serde_json::from_slice(include_bytes!("ssc_manifest.json")).unwrap();
    let mut values = manifest["configuration"].as_object().unwrap().clone();
    values.remove("load_diagnostic");
    values.remove("profile_name");
    values.remove("use_launcher_overrides");
    let book = serde_json::json!({"schema":1,"active":"Training","profiles":{"Training":values}});
    let profile_file = profile_dir.join("profiles.json");
    fs::write(&profile_file, serde_json::to_vec(&book).unwrap()).unwrap();
    let mut state = ModsState::default();
    state.active.order = vec!["another mod".into(), "ssc".into()];
    state.active.enabled = vec!["ssc".into()];
    state.active.configuration.insert("another mod".into(), HashMap::from([
        ("test".into(), serde_json::json!([null, -3.5, "épreuve"]))
    ]));
    state.active.configuration.insert("ssc".into(), HashMap::from([
        ("load_diagnostic".into(), true.into()),
        ("use_launcher_overrides".into(), true.into()),
        ("profile_name".into(), "Training".into()),
        ("fps".into(), "60".into()),
        ("bitrate_mbps".into(), serde_json::json!(12.0)),
        ("package_directory".into(), serde_json::json!("D:/Runs/épreuve/🏂"))
    ]));
    save_mods_state(&mods_dir, &state).unwrap();
    let state_file = mods_dir.join("mods.json");
    let before = fs::read(&state_file).unwrap();
    let profile_before = fs::read(&profile_file).unwrap();
    let output = std::process::Command::new(&preview)
        .arg(&profile_dir).arg(&mods_dir).output().unwrap();
    assert!(output.status.success(), "{}", String::from_utf8_lossy(&output.stderr));
    let result: serde_json::Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(result["native_capabilities"], false);
    assert_eq!(result["diagnostic_requested"], true);
    assert_eq!(result["profile"], "Training");
    assert_eq!(result["source"], "launcher");
    assert_eq!(result["settings"]["fps"], "60");
    assert_eq!(result["settings"]["bitrate_mbps"], 12);
    assert_eq!(result["settings"]["package_directory"], "D:\\Runs\\épreuve\\🏂");
    assert_eq!(fs::read(&state_file).unwrap(), before);
    assert_eq!(fs::read(&profile_file).unwrap(), profile_before);
    println!("PASS: actual PR66 serialized ModsState consumed by native SSC preview, source files unchanged");
}
