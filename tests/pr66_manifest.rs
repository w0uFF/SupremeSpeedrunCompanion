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
