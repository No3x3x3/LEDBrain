// Simple test to verify JavaScript is executing
console.log("=== LEDBrain app.js loaded ===");
if (typeof document === "undefined") {
  console.error("FATAL: document is undefined!");
}

const state = {
  lang: "pl",
  config: null,
  info: null,
  notifyTimer: null,
  wledDevices: [],
  ledPins: [],
  ledState: { enabled: false, autostart: false },
  timers: { info: null, wled: null, uptime: null },
  lastInfoRefresh: 0,
  selectedFxSegment: null,
  selectedDeviceKey: null,
  latestRelease: null,
  otaChecking: false,
  globalBrightness: 255,
  brightnessTimer: null,
  refreshingInfo: false,
  loadingWled: false,
  fxFormAccordionState: { advanced: false, audioAdvanced: false },
  saveLedConfigTimer: null,
  // Store device states before global disable
  deviceStatesBeforeDisable: {
    wled: {}, // device_id -> enabled state
    physical: {}, // segment_id -> enabled state
    virtual: {}, // segment_id -> enabled state
  },
  // Store global state before toggleAll disable
  globalStateBeforeDisable: {
    effectsEnabled: false,
    audioEnabled: false,
  },
};

const T = {};

const INFO_REFRESH_MS = 5000;
const WLED_REFRESH_MS = 12000;
const EFFECT_CATALOG = [
  "Solid",
  "Blink",
  "Breathe",
  "Chase",
  "Colorloop",
  "Rainbow",
  "Rainbow Runner",
  "Rainbow Bands",
  "Rain",
  "Rain (Dual)",
  "Meteor",
  "Meteor Smooth",
  "Power+",
  "Power Cycle",
  "Energy Flow",
  "Energy Burst",
  "Energy Waves",
  "Candle Multi",
  "Candle",
  "Matrix",
  "Waves",
  "Plasma",
  "Ripple Flow",
  "Heartbeat",
  "Aura",
  "Hyperspace",
  "Ripple",
  "Pacifica",
  "Theater",
  "Scanner",
  "Scanner Dual",
  "Noise",
  "Sinelon",
  "Fire 2012",
  "Fireworks",
  "Beat Pulse",
  "Beat Bars",
  "Beat Scatter",
  "Beat Light",
  "Strobe",
];
const AUDIO_MODES = ["spectrum", "bass", "beat", "tempo", "energy", "vibes"];
const AUDIO_PROFILES = [
  { value: "default", label: "audio_profile_default" },
  { value: "wled_reactive", label: "audio_profile_wled_reactive" },
  { value: "wled_bass_boost", label: "audio_profile_wled_bass" },
  { value: "ledfx_energy", label: "audio_profile_ledfx_energy" },
  { value: "ledfx_tempo", label: "audio_profile_ledfx_tempo" },
];
const EFFECT_LIBRARY = [
  {
    category: "Ambient",
    effects: [
      { name: "Rain", desc: "Falling drops with cool tones", engine: "ledfx" },
      { name: "Rain (Dual)", desc: "Dual-layer rain flow", engine: "wled" },
      { name: "Waves", desc: "Smooth rolling gradients", engine: "ledfx" },
      { name: "Plasma", desc: "Animated gradients", engine: "ledfx" },
      { name: "Aura", desc: "Soft ambient glow", engine: "ledfx" },
      { name: "Ripple Flow", desc: "Ripples moving across LEDs", engine: "ledfx" },
      { name: "Matrix", desc: "Matrix-style falling glyphs", engine: "ledfx" },
    ],
  },
  {
    category: "Energy",
    effects: [
      { name: "Power+", desc: "Punchy energy beams", engine: "wled" },
      { name: "Power Cycle", desc: "Cycled energy pulses", engine: "wled" },
      { name: "Energy Flow", desc: "Directional energy trails", engine: "wled" },
      { name: "Energy Burst", desc: "Short bursts, beat friendly", engine: "wled" },
      { name: "Energy Waves", desc: "Layered waves, fast", engine: "wled" },
      { name: "Hyperspace", desc: "Deep-space warp lines", engine: "ledfx" },
    ],
  },
  {
    category: "Rhythm",
    effects: [
      { name: "Beat Pulse", desc: "Pulse on beat", engine: "wled" },
      { name: "Beat Bars", desc: "Bars scaled by beat", engine: "wled" },
      { name: "Beat Scatter", desc: "Beat-driven scatter", engine: "wled" },
      { name: "Beat Light", desc: "Simple beat flashes", engine: "wled" },
      { name: "Strobe", desc: "Hard strobe", engine: "wled" },
    ],
  },
  {
    category: "Classic",
    effects: [
      { name: "Solid", desc: "Solid color", engine: "wled" },
      { name: "Blink", desc: "Blinking effect", engine: "wled" },
      { name: "Breathe", desc: "Breathing pulse", engine: "wled" },
      { name: "Chase", desc: "Chasing lights", engine: "wled" },
      { name: "Colorloop", desc: "Color loop", engine: "wled" },
      { name: "Rainbow", desc: "Rainbow loop", engine: "wled" },
      { name: "Rainbow Runner", desc: "Fast rainbow runner", engine: "wled" },
      { name: "Rainbow Bands", desc: "Striped rainbow bands", engine: "wled" },
      { name: "Rain", desc: "Falling rain drops", engine: "wled" },
      { name: "Meteor", desc: "Meteor shower", engine: "wled" },
      { name: "Meteor Smooth", desc: "Smooth meteor shower", engine: "wled" },
      { name: "Candle", desc: "Candle flicker", engine: "wled" },
      { name: "Candle Multi", desc: "Multiple candle flickers", engine: "wled" },
      { name: "Scanner", desc: "Larson scanner", engine: "wled" },
      { name: "Scanner Dual", desc: "Dual-direction scanner", engine: "wled" },
      { name: "Theater", desc: "Theater chase", engine: "wled" },
      { name: "Noise", desc: "Perlin noise", engine: "wled" },
      { name: "Sinelon", desc: "Sinelon effect", engine: "wled" },
      { name: "Fireworks", desc: "Bursts and sparks", engine: "wled" },
      { name: "Fire 2012", desc: "Classic fire", engine: "wled" },
      { name: "Heartbeat", desc: "Heartbeat pulse", engine: "wled" },
      { name: "Ripple", desc: "Single ripple", engine: "wled" },
      { name: "Pacifica", desc: "Pacifica ocean waves", engine: "wled" },
    ],
  },
];

// Default values for each effect - matching WLED and LEDFx defaults
// Based on WLED source code and LEDFx documentation
const EFFECT_DEFAULTS = {
  // WLED Effects
  "Solid": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Blink": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Breathe": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Chase": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Colorloop": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Rainbow": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Rainbow Runner": { brightness: 255, intensity: 128, speed: 192, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Rainbow Bands": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Rain": { brightness: 255, intensity: 128, speed: 128, color1: "#0033ff", color2: "#0066ff", color3: "#0099ff" },
  "Rain (Dual)": { brightness: 255, intensity: 128, speed: 128, color1: "#0033ff", color2: "#0066ff", color3: "#0099ff" },
  "Meteor": { brightness: 255, intensity: 128, speed: 128, color1: "#ff6600", color2: "#ffaa00", color3: "#ffffff" },
  "Meteor Smooth": { brightness: 255, intensity: 128, speed: 128, color1: "#ff6600", color2: "#ffaa00", color3: "#ffffff" },
  "Power+": { brightness: 255, intensity: 128, speed: 128, color1: "#00ff00", color2: "#ffff00", color3: "#ff0000" },
  "Power Cycle": { brightness: 255, intensity: 128, speed: 128, color1: "#00ff00", color2: "#ffff00", color3: "#ff0000" },
  "Energy Flow": { brightness: 255, intensity: 128, speed: 128, color1: "#00ff00", color2: "#ffff00", color3: "#ff0000" },
  "Energy Burst": { brightness: 255, intensity: 128, speed: 192, color1: "#00ff00", color2: "#ffff00", color3: "#ff0000" },
  "Energy Waves": { brightness: 255, intensity: 128, speed: 192, color1: "#00ff00", color2: "#ffff00", color3: "#ff0000" },
  "Candle": { brightness: 255, intensity: 128, speed: 128, color1: "#ff6600", color2: "#ffaa00", color3: "#ffffff" },
  "Candle Multi": { brightness: 255, intensity: 128, speed: 128, color1: "#ff6600", color2: "#ffaa00", color3: "#ffffff" },
  "Matrix": { brightness: 255, intensity: 128, speed: 128, color1: "#00ff00", color2: "#00aa00", color3: "#003300" },
  "Waves": { brightness: 255, intensity: 128, speed: 128, color1: "#0033ff", color2: "#0066ff", color3: "#0099ff" },
  "Plasma": { brightness: 255, intensity: 128, speed: 128, color1: "#ff00ff", color2: "#00ffff", color3: "#ffff00" },
  "Ripple Flow": { brightness: 255, intensity: 128, speed: 128, color1: "#00ffff", color2: "#0099ff", color3: "#0033ff" },
  "Heartbeat": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#ff6600", color3: "#ffffff" },
  "Aura": { brightness: 255, intensity: 128, speed: 128, color1: "#ff00ff", color2: "#00ffff", color3: "#ffff00" },
  "Hyperspace": { brightness: 255, intensity: 128, speed: 192, color1: "#ff00ff", color2: "#00ffff", color3: "#0000ff" },
  "Ripple": { brightness: 255, intensity: 128, speed: 128, color1: "#00ffff", color2: "#0099ff", color3: "#0033ff" },
  "Pacifica": { brightness: 255, intensity: 128, speed: 128, color1: "#0033ff", color2: "#0066ff", color3: "#0099ff" },
  "Theater": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Scanner": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Scanner Dual": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Noise": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Sinelon": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Fire 2012": { brightness: 255, intensity: 128, speed: 128, color1: "#ff6600", color2: "#ffaa00", color3: "#ffffff" },
  "Fireworks": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#ffff00", color3: "#ffffff" },
  "Beat Pulse": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#ff6600", color3: "#ffffff" },
  "Beat Bars": { brightness: 255, intensity: 128, speed: 128, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Beat Scatter": { brightness: 255, intensity: 128, speed: 192, color1: "#ff0000", color2: "#00ff00", color3: "#0000ff" },
  "Beat Light": { brightness: 255, intensity: 128, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
  "Strobe": { brightness: 255, intensity: 255, speed: 128, color1: "#ffffff", color2: "#996600", color3: "#0033ff" },
};

// Function to get default values for an effect
function getEffectDefaults(effectName, engine) {
  if (!effectName) {
    // Return generic defaults
    return {
      brightness: 255,
      intensity: 128,
      speed: 128,
      color1: "#ffffff",
      color2: "#996600",
      color3: "#0033ff",
    };
  }
  
  // Check if we have specific defaults for this effect
  const defaults = EFFECT_DEFAULTS[effectName];
  if (defaults) {
    return { ...defaults };
  }
  
  // Fallback to generic defaults
  return {
    brightness: 255,
    intensity: 128,
    speed: 128,
    color1: "#ffffff",
    color2: "#996600",
    color3: "#0033ff",
  };
}

// Function to apply effect defaults to an assignment when effect is first selected
function applyEffectDefaults(assignment, effectName, engine, forceApply = false) {
  if (!assignment || !effectName) return;
  
  const defaults = getEffectDefaults(effectName, engine);
  
  // When forceApply is true (effect just selected), always apply defaults
  // Otherwise, only apply if values are at generic defaults (preserve user customizations)
  if (forceApply) {
    assignment.brightness = defaults.brightness;
    assignment.intensity = defaults.intensity;
    assignment.speed = defaults.speed;
    assignment.color1 = defaults.color1;
    assignment.color2 = defaults.color2;
    assignment.color3 = defaults.color3;
  } else {
    // Only update if values are at generic defaults
    if (typeof assignment.brightness !== "number" || assignment.brightness === 255) {
      assignment.brightness = defaults.brightness;
    }
    if (typeof assignment.intensity !== "number" || assignment.intensity === 128) {
      assignment.intensity = defaults.intensity;
    }
    if (typeof assignment.speed !== "number" || assignment.speed === 128) {
      assignment.speed = defaults.speed;
    }
    if (!assignment.color1 || assignment.color1 === "#ffffff") {
      assignment.color1 = defaults.color1;
    }
    if (!assignment.color2 || assignment.color2 === "#996600") {
      assignment.color2 = defaults.color2;
    }
    if (!assignment.color3 || assignment.color3 === "#0033ff") {
      assignment.color3 = defaults.color3;
    }
  }
}

const OTA_RELEASE_URL = "https://api.github.com/repos/No3x3x3/LEDBrain/releases/latest";
const MAX_BRIGHTNESS = 255;

const qs = (id) => document.getElementById(id);
const setText = (id, value) => {
  const el = qs(id);
  if (el) {
    el.textContent = value ?? "";
  }
};

function t(key) {
  return T[key] || key;
}

function notify(message, variant = "ok") {
  // Notifications disabled - function does nothing
  return;
}

async function loadLang(lang) {
  console.log("loadLang: Loading language", lang);
  try {
    const url = `/lang/${lang}.json?t=${Date.now()}`;
    console.log("loadLang: Fetching", url);
    const res = await fetch(url);
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}: ${res.statusText}`);
    }
    const data = await res.json();
    console.log("loadLang: Received data, keys:", Object.keys(data).length);
    Object.assign(T, data);
    state.lang = lang;
    console.log("loadLang: Rendering language");
    renderLang();
    console.log("loadLang: Language loaded successfully:", lang);
  } catch (err) {
    console.error("loadLang: Failed to load language:", lang, err);
    // Fallback to English if Polish fails
    if (lang === "pl") {
      console.log("loadLang: Falling back to English");
      await loadLang("en");
    } else {
      notify("Failed to load translations", "error");
    }
  }
}

function renderLang() {
  const map = {
    lblLang: "language",
    navOverview: "nav_lights",
    navNetwork: "nav_network",
    navMqtt: "nav_mqtt",
    navSystem: "nav_system",
    navLed: "nav_devices",
    tabConfigPower: "config_tab_power",
    tabConfigSegments: "config_tab_segments",
    tabConfigAudio: "config_tab_audio",
    tabConfigVirtual: "config_tab_virtual",
    tabConfigWled: "config_tab_wled",
    btnReboot: "btn_reboot",
    lblGlobalBrightness: "global_brightness",
    ledDriverTitle: "led_driver_title",
    ledDriverSubtitle: "led_driver_subtitle",
    lblDriver: "led_driver_label",
    lblMaxFps: "led_max_fps",
    lblCurrentLimit: "led_current_limit",
    lblPsuVoltage: "psu_voltage",
    lblPsuWatts: "psu_watts",
    lblAutoPowerLimit: "auto_power_limit",
    lblParallelOutputs: "led_parallel_outputs",
    lblEnableDma: "led_enable_dma",
    psuHint: "led_psu_hint",
    overviewTitle: "overview_title",
    overviewSubtitle: "overview_subtitle",
    statusCardTitle: "overview_status_card",
    networkCardTitle: "overview_network_card",
    mqttCardTitle: "overview_mqtt_card",
    lblDevice: "device_name",
    lblFirmware: "firmware",
    lblOta: "ota",
    lblUptime: "uptime",
    lblIp: "ip",
    lblGateway: "gw",
    lblDns: "dns",
    lblMqttServer: "mqtt_server",
    lblMqttUser: "mqtt_user",
    lblDdp: "ddp_target_label",
    btnOpenHA: "open_ha",
    btnMqttTest: "btn_mqtt_test",
    networkTitle: "network_title",
    networkSubtitle: "network_subtitle",
    networkAddressingTitle: "network_addressing_title",
    networkAddressingSubtitle: "network_addressing_subtitle",
    networkIdentityTitle: "network_identity_title",
    networkIdentitySubtitle: "network_identity_subtitle",
    dhcpLabel: "dhcp",
    lblHost: "hostname",
    lblStaticIp: "ip",
    lblNetmask: "mask",
    lblGatewayForm: "gw",
    lblDnsForm: "dns",
    networkHint: "network_hint",
    btnSaveNetwork: "save_reboot",
    btnNetworkApply: "save_apply",
    mqttTitle: "mqtt_title",
    mqttSubtitle: "mqtt_subtitle",
    mqttBrokerTitle: "mqtt_broker_title",
    mqttBrokerSubtitle: "mqtt_broker_subtitle",
    mqttDdpTitle: "mqtt_ddp_title",
    mqttDdpSubtitle: "mqtt_ddp_subtitle",
    mqttEnabledLabel: "mqtt_enabled",
    lblMqttHost: "mqtt_host",
    lblMqttPort: "mqtt_port",
    lblMqttUserField: "mqtt_user",
    lblMqttPass: "mqtt_pass",
    lblDdpTarget: "ddp_target",
    lblDdpPort: "ddp_port",
    mqttHint: "mqtt_hint",
    btnSaveMqtt: "save_reboot",
    btnMqttSync: "btn_mqtt_sync",
    systemTitle: "system_title",
    systemSubtitle: "system_subtitle",
    systemInfoTitle: "system_info_title",
    systemInfoSubtitle: "system_info_subtitle",
    lblSystemFw: "firmware",
    lblSystemIdf: "idf_version",
    lblSystemBuild: "build_time",
    otaTitle: "ota_title",
    otaSubtitle: "ota_subtitle",
    lblCurrentVersion: "current_version",
    lblAvailableVersion: "available_version",
    btnOtaCheck: "btn_ota_check",
    btnOtaApply: "btn_ota_apply",
    lblLangSystem: "language",
    lblAutostart: "autostart",
    systemHint: "system_hint",
    btnSaveSystem: "save",
    btnFactoryReset: "factory_reset",
    otaHint: "ota_hint",
    ledTitle: "led_title",
    ledSubtitle: "led_subtitle",
    ledSegmentsTitle: "led_segments_title",
    btnAddSegment: "btn_add_segment",
    colSegName: "col_seg_name",
    colSegLeds: "col_seg_leds",
    colSegStart: "col_seg_start",
    colSegOrder: "col_seg_order",
    colSegPin: "col_seg_pin",
    colSegRmt: "col_seg_rmt",
    colSegMatrix: "col_seg_matrix",
    colSegPower: "col_seg_power",
    colSegSource: "col_seg_source",
    colSegDirection: "col_seg_direction",
    colSegEnabled: "col_seg_enabled",
    ledSegmentsEmpty: "led_segments_empty",
    segmentHint: "segment_hint",
    ledPowerHint: "led_power_hint",
    audioTitle: "audio_title",
    audioSubtitle: "audio_subtitle",
    lblAudioSource: "audio_source",
    lblAudioSampleRate: "audio_sample_rate",
    lblAudioFft: "audio_fft",
    lblAudioFrame: "audio_frame",
    lblAudioSensitivity: "audio_sensitivity",
    lblAudioStereo: "audio_stereo",
    lblSnapHost: "snap_host",
    lblSnapPort: "snap_port",
    lblSnapLatency: "snap_latency",
    lblSnapUdp: "snap_udp",
    audioHint: "audio_hint",
    snapHint: "snap_hint",
    effectsTitle: "effects_title",
    effectsSubtitle: "effects_subtitle",
    lblDefaultEngine: "default_engine",
    colFxSegment: "col_fx_segment",
    colFxEngine: "col_fx_engine",
    colFxEffect: "col_fx_effect",
    colFxPreset: "col_fx_preset",
    colFxBrightness: "col_fx_brightness",
    colFxIntensity: "col_fx_intensity",
    colFxSpeed: "col_fx_speed",
    colFxAudioMode: "col_fx_audio_mode",
    colFxAudioProfile: "col_fx_audio_profile",
    colFxAudioLink: "col_fx_audio_link",
    colFxStereo: "col_fx_stereo",
    colFxGainL: "col_fx_gain_l",
    colFxGainR: "col_fx_gain_r",
    colFxSensitivity: "col_fx_sensitivity",
    colFxThreshold: "col_fx_threshold",
    colFxSmoothing: "col_fx_smoothing",
    ledFxEmpty: "led_fx_empty",
    btnSaveAudio: "btn_save_audio",
    btnSaveGlobal: "btn_save_global",
    btnSaveLed: "btn_save_led",
    btnSaveAudio: "btn_save_audio",
    btnSaveGlobal: "btn_save_global",
    lightsGroupWled: "lights_group_wled",
    lightsGroupPhysical: "lights_group_physical",
    lightsGroupVirtual: "lights_group_virtual",
    wledEmptyHint: "wled_empty_hint",
    fxSegmentsTitle: "fx_segments_title",
    fxSegmentsSubtitle: "fx_segments_subtitle",
    fxSettingsTitle: "fx_settings_title",
    fxSettingsSubtitle: "fx_settings_subtitle",
    fxAudioTitle: "fx_audio_title",
    fxAudioSubtitle: "fx_audio_subtitle",
    fxSceneTitle: "fx_scene_title",
    fxSceneSubtitle: "fx_scene_subtitle",
    lblFxEffectPicker: "fx_effect_picker",
    lblFxDirection: "fx_direction",
    lblFxScatter: "fx_scatter",
    lblFxFadeIn: "fx_fade_in",
    lblFxFadeOut: "fx_fade_out",
    lblFxColor1: "fx_color1",
    lblFxColor2: "fx_color2",
    lblFxColor3: "fx_color3",
    lblFxPalette: "fx_palette",
    lblFxGradient: "fx_gradient",
    lblFxBrightnessOverride: "fx_brightness_override",
    lblFxGammaColor: "fx_gamma_color",
    lblFxGammaBrightness: "fx_gamma_brightness",
    lblFxBlendMode: "fx_blend_mode",
    lblFxLayers: "fx_layers",
    lblFxReactiveMode: "fx_reactive_mode",
    lblFxSensBass: "fx_sens_bass",
    lblFxSensMids: "fx_sens_mids",
    lblFxSensTreble: "fx_sens_treble",
    lblFxAmplitude: "fx_amplitude_scale",
    lblFxBrightnessCompress: "fx_brightness_compress",
    lblFxBeatResponse: "fx_beat_response",
    lblFxAttack: "fx_attack",
    lblFxRelease: "fx_release",
    lblFxScenePreset: "fx_scene_preset",
    lblFxSceneSchedule: "fx_scene_schedule",
    lblFxBeatShuffle: "fx_beat_shuffle",
    wledTitle: "wled_title",
    wledSubtitle: "wled_subtitle",
    wledTableName: "wled_table_name",
    wledTableId: "wled_table_id",
    wledTableLeds: "wled_table_leds",
    wledTableSegments: "wled_table_segments",
    wledTableStatus: "wled_table_status",
    wledTableLastSeen: "wled_table_last_seen",
    btnWledRescan: "wled_btn_rescan",
    wledManualTitle: "wled_manual_title",
    wledManualSubtitle: "wled_manual_subtitle",
    wledManualNameLabel: "wled_manual_name",
    wledManualAddressLabel: "wled_manual_address",
    wledManualLedsLabel: "wled_manual_leds",
    wledManualSegmentsLabel: "wled_manual_segments",
    btnWledAdd: "btn_wled_add",
  };
  Object.entries(map).forEach(([id, key]) => {
    const el = qs(id);
    if (el) {
      el.textContent = t(key);
    }
  });
  setOtaStatus(t("ota_status_idle"));
  setText("valAvailableVersion", state.latestRelease?.tag || "-");
  setText("valCurrentVersion", state.info?.fw_version || state.info?.fw_name || "-");
  updatePowerSummary();
  updateDhcpUi();

  const placeholders = [
    ["host", "ph_host"],
    ["ip", "ph_static_ip"],
    ["mask", "ph_netmask"],
    ["gw", "ph_gateway"],
    ["dns", "ph_dns"],
    ["mqttHost", "ph_mqtt_host"],
    ["mqttUser", "ph_mqtt_user"],
    ["mqttPass", "ph_mqtt_pass"],
    ["ddpTarget", "ph_ddp_target"],
    ["wledManualName", "wled_manual_name"],
    ["wledManualAddress", "wled_manual_address"],
  ];
  placeholders.forEach(([id, key]) => {
    const el = qs(id);
    if (el) el.placeholder = t(key);
  });

  if (state.info) {
    updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
  }
  renderWledDevices();
  renderEffectCatalog("wled"); // Default to WLED
  renderLedConfig();
  renderLightsDashboard();
  updateControlButtons();
  const refreshBtn = qs("btnRefresh");
  if (refreshBtn) {
    refreshBtn.setAttribute("title", t("btn_refresh") + " / " + t("btn_reboot") + " (long press)");
    refreshBtn.setAttribute("aria-label", t("btn_refresh") + " / " + t("btn_reboot") + " (long press)");
  }
}

function renderEffectCatalog(engine) {
  const list = qs("effectCatalog");
  if (!list) {
    console.warn("renderEffectCatalog: effectCatalog datalist not found");
    return;
  }
  // Filter effects by engine - WLED and LEDFx have separate effect lists
  let effects = [];
  if (engine === "wled") {
    // WLED effects - get from EFFECT_LIBRARY where engine is "wled"
    for (const group of EFFECT_LIBRARY) {
      for (const eff of group.effects) {
        if (eff.engine === "wled" && !effects.includes(eff.name)) {
          effects.push(eff.name);
        }
      }
    }
    // Also include basic effects from EFFECT_CATALOG (these are WLED effects)
    for (const name of EFFECT_CATALOG) {
      if (!effects.includes(name)) {
        effects.push(name);
      }
    }
  } else if (engine === "ledfx") {
    // LEDFx effects - ONLY get from EFFECT_LIBRARY where engine is "ledfx"
    // DO NOT include EFFECT_CATALOG as those are WLED effects
    for (const group of EFFECT_LIBRARY) {
      for (const eff of group.effects) {
        if (eff.engine === "ledfx" && !effects.includes(eff.name)) {
          effects.push(eff.name);
        }
      }
    }
  } else {
    // Unknown engine - include all effects
    console.warn(`renderEffectCatalog: Unknown engine "${engine}", including all effects`);
    for (const group of EFFECT_LIBRARY) {
      for (const eff of group.effects) {
        if (!effects.includes(eff.name)) {
          effects.push(eff.name);
        }
      }
    }
    for (const name of EFFECT_CATALOG) {
      if (!effects.includes(name)) {
        effects.push(name);
      }
    }
  }
  // Sort effects alphabetically for better UX
  effects.sort();
  list.innerHTML = effects.map((name) => `<option value="${name}"></option>`).join("");
  console.log(`renderEffectCatalog(${engine}): ${effects.length} effects`, effects.slice(0, 10), "...");
}

function stopAutoRefreshLoops() {
  if (state.timers.info) {
    clearInterval(state.timers.info);
    state.timers.info = null;
  }
  if (state.timers.wled) {
    clearInterval(state.timers.wled);
    state.timers.wled = null;
  }
}

function startAutoRefreshLoops() {
  stopAutoRefreshLoops();
  // Throttle refreshInfo - nie wykonuj jeśli już trwa
  const throttledRefreshInfo = throttle(() => {
    if (!state.refreshingInfo) {
      state.refreshingInfo = true;
      refreshInfo().finally(() => {
        state.refreshingInfo = false;
      });
    }
  }, INFO_REFRESH_MS);
  
  // Throttle loadWledDevices - nie wykonuj jeśli już trwa
  const throttledLoadWled = throttle(() => {
    if (!state.loadingWled) {
      state.loadingWled = true;
      loadWledDevices().finally(() => {
        state.loadingWled = false;
      });
    }
  }, WLED_REFRESH_MS);
  
  state.timers.info = setInterval(throttledRefreshInfo, INFO_REFRESH_MS);
  state.timers.wled = setInterval(throttledLoadWled, WLED_REFRESH_MS);
}

function startUptimeTicker() {
  if (state.timers.uptime) {
    clearInterval(state.timers.uptime);
    state.timers.uptime = null;
  }
  state.timers.uptime = setInterval(() => {
    if (!state.info || typeof state.info.uptime_s !== "number") {
      return;
    }
    state.info.uptime_s += 1;
    const uptimeEl = qs("valUptime");
    if (uptimeEl) {
      uptimeEl.textContent = formatUptime(state.info.uptime_s);
    }
  }, 1000);
}

function updateControlButtons() {
  const effectsEnabled = state.ledState && state.ledState.enabled;
  const audioEnabled = state.info?.led_engine?.audio?.running ?? false;
  // All is enabled if anything is running (OR, not AND)
  const allEnabled = effectsEnabled || audioEnabled;
  
  const btnAll = qs("btnToggleAll");
  if (btnAll) {
    btnAll.dataset.state = allEnabled ? "playing" : "stopped";
    btnAll.disabled = false;
  }
  
  const btnEffects = qs("btnToggleEffects");
  if (btnEffects) {
    btnEffects.dataset.state = effectsEnabled ? "playing" : "stopped";
    btnEffects.disabled = false;
  }
  
  const btnAudio = qs("btnToggleAudio");
  if (btnAudio) {
    btnAudio.dataset.state = audioEnabled ? "playing" : "stopped";
    btnAudio.disabled = false;
  }
}

async function loadLedState() {
  try {
    const res = await fetch("/api/led/state");
    state.ledState = await res.json();
    if (typeof state.ledState.brightness === "number") {
      state.globalBrightness = clamp(state.ledState.brightness, 1, MAX_BRIGHTNESS);
      syncBrightnessUi();
    }
    updateControlButtons();
  } catch (err) {
    console.warn("led state", err);
  }
}

function scheduleBrightnessUpdate(value) {
  clearTimeout(state.brightnessTimer);
  state.globalBrightness = clamp(value, 1, MAX_BRIGHTNESS);
  syncBrightnessUi();
  state.brightnessTimer = setTimeout(() => {
    applyBrightness(state.globalBrightness);
  }, 250);
}

async function applyBrightness(value) {
  try {
    await fetch("/api/led/state", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ brightness: value, remember: true }),
    });
    state.ledState = state.ledState || {};
    state.ledState.brightness = value;
    if (state.info) {
      state.info.led_engine = state.info.led_engine || {};
      state.info.led_engine.brightness = value;
    }
  } catch (err) {
    console.error("brightness", err);
    notify(t("toast_save_failed"), "error");
  }
}

async function toggleAll() {
  const effectsEnabled = state.ledState && state.ledState.enabled;
  const audioEnabled = state.info?.led_engine?.audio?.running ?? false;
  const allEnabled = effectsEnabled || audioEnabled; // OR instead of AND - if anything is running, consider it enabled
  const next = !allEnabled;
  
  const btn = qs("btnToggleAll");
  if (btn) btn.disabled = true;
  try {
    // If disabling, save current global state (effects + audio) and device states
    if (!next) {
      state.globalStateBeforeDisable.effectsEnabled = effectsEnabled;
      state.globalStateBeforeDisable.audioEnabled = audioEnabled;
      saveDeviceStates();
    }
    
    // Toggle both effects and audio
    await Promise.all([
      fetch("/api/led/state", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ enabled: next }),
      }),
      fetch("/api/audio/state", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ enabled: next }),
      })
    ]);
    
    // If enabling, restore previous state
    if (next) {
      const saved = state.globalStateBeforeDisable;
      // Restore effects if they were enabled before
      if (saved.effectsEnabled) {
        await fetch("/api/led/state", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled: true }),
        });
        state.ledState = state.ledState || {};
        state.ledState.enabled = true;
      }
      // Restore audio if it was enabled before
      if (saved.audioEnabled) {
        await fetch("/api/audio/state", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled: true }),
        });
      }
      // Restore device states
      await restoreDeviceStates();
    } else {
      // Disabling - update state
      state.ledState = state.ledState || {};
      state.ledState.enabled = false;
    }
    
    if (state.info && state.info.led_engine) {
      if (next) {
        state.info.led_engine.enabled = state.globalStateBeforeDisable.effectsEnabled;
        if (state.info.led_engine.audio) {
          state.info.led_engine.audio.running = state.globalStateBeforeDisable.audioEnabled;
        }
      } else {
        state.info.led_engine.enabled = false;
        if (state.info.led_engine.audio) {
          state.info.led_engine.audio.running = false;
        }
      }
      updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    }
    updateControlButtons();
    // Refresh device preview to show current states (but individual buttons remain independent)
    renderDevicePreview();
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

// Save device states before global disable
function saveDeviceStates() {
  const saved = state.deviceStatesBeforeDisable;
  
  // Save WLED device states
  const fx = ensureWledEffectsConfig();
  saved.wled = {};
  fx.bindings.forEach(binding => {
    const deviceId = binding.device_id;
    if (deviceId) {
      saved.wled[deviceId] = binding.enabled !== false;
    }
  });
  
  // Save physical segment states
  const led = ensureLedEngineConfig();
  saved.physical = {};
  (led.segments || []).forEach(seg => {
    const segId = seg.id || seg.segment_id;
    if (segId) {
      saved.physical[segId] = seg.enabled !== false;
    }
  });
  
  // Save virtual segment states
  ensureVirtualSegments();
  saved.virtual = {};
  (state.config.virtual_segments || []).forEach(seg => {
    const segId = seg.id || seg.name;
    if (segId) {
      saved.virtual[segId] = seg.enabled !== false;
    }
  });
}

// Restore device states after global enable
async function restoreDeviceStates() {
  const saved = state.deviceStatesBeforeDisable;
  let needsSave = false;
  
  // Restore WLED device states
  const fx = ensureWledEffectsConfig();
  fx.bindings.forEach(binding => {
    const deviceId = binding.device_id;
    if (deviceId && saved.wled.hasOwnProperty(deviceId)) {
      const wasEnabled = saved.wled[deviceId];
      if (wasEnabled) {
        binding.enabled = true;
        needsSave = true;
      }
    }
  });
  
  // Restore physical segment states
  const led = ensureLedEngineConfig();
  (led.segments || []).forEach(seg => {
    const segId = seg.id || seg.segment_id;
    if (segId && saved.physical.hasOwnProperty(segId)) {
      const wasEnabled = saved.physical[segId];
      if (wasEnabled) {
        seg.enabled = true;
        needsSave = true;
      }
    }
  });
  
  // Restore virtual segment states
  ensureVirtualSegments();
  (state.config.virtual_segments || []).forEach(seg => {
    const segId = seg.id || seg.name;
    if (segId && saved.virtual.hasOwnProperty(segId)) {
      const wasEnabled = saved.virtual[segId];
      if (wasEnabled) {
        seg.enabled = true;
        needsSave = true;
      }
    }
  });
  
  // Save if any changes were made (saveLedConfig already saves wled_effects)
  if (needsSave) {
    await saveLedConfig();
  }
}

async function toggleEffects() {
  const next = !(state.ledState && state.ledState.enabled);
  const btn = qs("btnToggleEffects");
  if (btn) btn.disabled = true;
  try {
    // If disabling, save current device states
    if (!next) {
      saveDeviceStates();
    }
    
    await fetch("/api/led/state", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: next }),
    });
    state.ledState = state.ledState || {};
    state.ledState.enabled = next;
    
    // If enabling, restore device states
    if (next) {
      await restoreDeviceStates();
    }
    
    if (state.info && state.info.led_engine) {
      state.info.led_engine.enabled = next;
      updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    }
    updateControlButtons();
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function toggleAudio() {
  const audioEnabled = state.info?.led_engine?.audio?.running ?? false;
  const next = !audioEnabled;
  const btn = qs("btnToggleAudio");
  if (btn) btn.disabled = true;
  try {
    await fetch("/api/audio/state", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: next }),
    });
    if (state.info && state.info.led_engine && state.info.led_engine.audio) {
      state.info.led_engine.audio.running = next;
      updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    }
    updateControlButtons();
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

function syncLanguageSelects(lang) {
  const selectPrimary = qs("langSwitch");
  const selectMirror = qs("langMirror");
  if (selectPrimary) selectPrimary.value = lang;
  if (selectMirror) selectMirror.value = lang;
}

function setBadge(id, text, variant = "") {
  const el = qs(id);
  if (!el) return;
  el.textContent = text;
  el.classList.remove("ok", "warn", "error");
  if (variant) el.classList.add(variant);
}

function formatUptime(seconds) {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const secs = Math.floor(seconds % 60);
  const parts = [];
  if (days) parts.push(`${days}d`);
  if (hours || parts.length) parts.push(`${hours}h`);
  parts.push(`${minutes}m`);
  if (!days) {
    parts.push(`${secs}s`);
  }
  return parts.join(" ");
}

function formatOtaState(state, err) {
  const key = state ? `ota_state_${state}` : "ota_state_unknown";
  const text = t(key);
  if (state === "failed" && err) {
    return `${text} (${err})`;
  }
  return text;
}

function setOtaStatus(text) {
  setText("otaStatus", text);
}

function clamp(val, min, max) {
  return Math.min(Math.max(val, min), max);
}

// Debounce helper - opóźnia wykonanie funkcji
function debounce(func, wait) {
  let timeout;
  return function executedFunction(...args) {
    const later = () => {
      clearTimeout(timeout);
      func(...args);
    };
    clearTimeout(timeout);
    timeout = setTimeout(later, wait);
  };
}

// Throttle helper - ogranicza częstotliwość wykonania funkcji
function throttle(func, limit) {
  let inThrottle;
  return function(...args) {
    if (!inThrottle) {
      func.apply(this, args);
      inThrottle = true;
      setTimeout(() => inThrottle = false, limit);
    }
  };
}

function syncBrightnessUi() {
  const slider = qs("brightnessGlobal");
  const label = qs("brightnessValue");
  if (!slider || !label) {
    console.warn("syncBrightnessUi: elements not found", { slider: !!slider, label: !!label });
    return;
  }
  slider.value = state.globalBrightness;
  const pct = Math.round((state.globalBrightness / MAX_BRIGHTNESS) * 100);
  label.textContent = `${pct}%`;
}

function formatAge(ms) {
  if (!ms) return t('not_set');
  const seconds = Math.floor(ms / 1000);
  if (seconds < 60) return `${Math.max(seconds, 1)}s`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}h`;
  const days = Math.floor(hours / 24);
  return `${days}d`;
}

function createStatusPill(text, variant = "") {
  const span = document.createElement("span");
  span.className = "status-pill";
  if (variant) span.classList.add(variant);
  span.textContent = text;
  return span;
}

function formatLightsCount(count) {
  const template = t("lights_count_label");
  if (template && template.includes("{count}")) {
    return template.replace("{count}", count);
  }
  return `${count}`;
}

function formatAgeMs(ageMs) {
  if (!Number.isFinite(ageMs) || ageMs <= 0) {
    return t("not_set");
  }
  return formatAge(ageMs);
}

function createLightMetaRow(label, value) {
  const row = document.createElement("span");
  const labelEl = document.createElement("strong");
  labelEl.textContent = label;
  const valueEl = document.createElement("span");
  valueEl.textContent = value || t("not_set");
  row.append(labelEl, valueEl);
  return row;
}

function mergeWledDevices() {
  const map = new Map();
  // Helper function to generate a unique key for a device
  const getDeviceKey = (dev) => {
    // Prefer id, then address, then name
    return dev.id || dev.address || dev.ip || dev.name || "";
  };
  
  // Helper function to find existing device by matching id, address, or ip
  const findExistingKey = (dev, map) => {
    const key = getDeviceKey(dev);
    if (!key) return null;
    
    for (const [k, v] of map.entries()) {
      // Match by id
      if (dev.id && v.id && v.id === dev.id) return k;
      // Match by address
      if (dev.address && v.address && v.address === dev.address) return k;
      if (dev.ip && v.address && v.address === dev.ip) return k;
      if (dev.address && v.ip && v.ip === dev.address) return k;
      // Match by key if it's the same
      if (k === key) return k;
    }
    return null;
  };
  
  const stored = Array.isArray(state.config?.wled_devices) ? state.config.wled_devices : [];
  stored.forEach((dev, idx) => {
    const key = getDeviceKey(dev) || `stored-${idx}`;
    const existingKey = findExistingKey(dev, map);
    
    if (existingKey) {
      // Update existing entry
      const existing = map.get(existingKey);
      map.set(existingKey, {
        ...existing,
        id: dev.id || existing.id || existingKey,
        name: dev.name || existing.name || existingKey,
        address: dev.address || existing.address || "",
        ip: dev.address || existing.ip || "",
        leds: dev.leds || existing.leds || 0,
        segments: dev.segments || existing.segments || 1,
        manual: !dev.auto_discovered,
      });
    } else {
      map.set(key, {
        id: dev.id || key,
        name: dev.name || dev.address || dev.id || key,
        address: dev.address || "",
        ip: dev.address || "",
        leds: dev.leds || 0,
        segments: dev.segments || 1,
        online: false,
        version: "",
        age_ms: Number.POSITIVE_INFINITY,
        http_verified: false,
        manual: !dev.auto_discovered,
      });
    }
  });
  
  const discovered = Array.isArray(state.wledDevices) ? state.wledDevices : [];
  discovered.forEach((dev, idx) => {
    const existingKey = findExistingKey(dev, map);
    const key = existingKey || getDeviceKey(dev) || `discovered-${idx}`;
    const existing = map.get(key) || {};
    map.set(key, {
      ...existing,
      id: dev.id || existing.id || key,
      name: dev.name || existing.name || key,
      address: dev.address || dev.ip || existing.address || "",
      ip: dev.ip || dev.address || existing.ip || "",
      leds:
        typeof dev.leds === "number"
          ? dev.leds
          : typeof existing.leds === "number"
          ? existing.leds
          : 0,
      segments:
        typeof dev.segments === "number"
          ? dev.segments
          : typeof existing.segments === "number"
          ? existing.segments
          : 1,
      online: !!dev.online,
      version: dev.version || existing.version || "",
      age_ms: typeof dev.age_ms === "number" ? dev.age_ms : existing.age_ms,
      http_verified: dev.http_verified || existing.http_verified || false,
      manual: existing.manual ?? false,
    });
  });
  
  return Array.from(map.values()).sort((a, b) =>
    (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" })
  );
}

function buildPhysicalSegmentsSummary() {
  const led = ensureLedEngineConfig();
  if (!led) return [];
  const assignments = Array.isArray(led.effects?.assignments) ? led.effects.assignments : [];
  return (led.segments || [])
    .map((seg) => {
      const fx = assignments.find((a) => a.segment_id === seg.id) || null;
      return {
        id: seg.id,
        name: seg.name || seg.id,
        leds: seg.led_count || 0,
        gpio: seg.gpio,
        engine: fx?.engine || (() => {
          const meta = findEffectMeta(fx?.effect);
          return meta?.engine || led.effects?.default_engine || "wled";
        })(),
        effect: fx?.effect || "",
        audio_link: !!fx?.audio_link,
        enabled: seg.enabled !== false,
      };
    })
    .sort((a, b) => (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" }));
}

function buildVirtualSegmentsSummary() {
  ensureVirtualSegments();
  return (state.config.virtual_segments || [])
    .map((seg, idx) => ({
      id: seg.id || `virtual-${idx + 1}`,
      name: seg.name || seg.id || `Virtual ${idx + 1}`,
      engine: seg.effect?.engine || "ledfx",
      effect: seg.effect?.effect || "",
      audio_mode: seg.effect?.audio_mode || "",
      audio_link: !!seg.effect?.audio_link,
      enabled: seg.enabled !== false,
    }))
    .sort((a, b) => (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" }));
}

function buildUnifiedDevices() {
  const physical = buildPhysicalSegmentsSummary().map((seg) => ({
    key: `physical:${seg.id}`,
    type: "physical",
    name: seg.name || seg.id,
    leds: seg.leds,
    segments: 1,
    online: seg.enabled,
    meta: seg,
  }));
  ensureVirtualSegments();
  const virtual = (state.config.virtual_segments || []).map((seg, idx) => ({
    key: `virtual:${seg.id || idx}`,
    type: "virtual",
    name: seg.name || seg.id || `Virtual ${idx + 1}`,
    leds: 0,
    segments: seg.members?.length || 0,
    online: seg.enabled !== false,
    meta: seg,
  }));
  const wled = mergeWledDevices().map((dev) => ({
    key: `wled:${dev.id || dev.address || dev.name}`,
    type: "wled",
    name: dev.name || dev.id || dev.address || "WLED",
    leds: dev.leds,
    segments: dev.segments,
    online: dev.online,
    meta: dev,
  }));
  return [...physical, ...virtual, ...wled].sort((a, b) =>
    (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" })
  );
}

function renderWledLightCard(dev) {
  const card = document.createElement("article");
  card.className = "light-card";
  const header = document.createElement("header");
  const title = document.createElement("h4");
  title.textContent = dev.name || dev.id || dev.address || "WLED";
  header.appendChild(title);
  header.appendChild(createStatusPill(dev.online ? t("wled_status_online") : t("wled_status_offline"), dev.online ? "ok" : "warn"));
  card.appendChild(header);

  const meta = document.createElement("div");
  meta.className = "light-meta";
  meta.appendChild(createLightMetaRow(t("wled_table_leds"), dev.leds ? String(dev.leds) : t("not_set")));
  meta.appendChild(createLightMetaRow(t("wled_table_segments"), dev.segments ? String(dev.segments) : t("not_set")));
  meta.appendChild(createLightMetaRow(t("lights_label_host"), dev.address || dev.ip || t("not_set")));
  if (dev.version) {
    meta.appendChild(createLightMetaRow(t("lights_label_version"), dev.version));
  }
  if (Number.isFinite(dev.age_ms)) {
    meta.appendChild(createLightMetaRow(t("lights_label_last_seen"), formatAgeMs(dev.age_ms)));
  }
  card.appendChild(meta);
  return card;
}

function renderPhysicalLightCard(seg) {
  const card = document.createElement("article");
  card.className = "light-card";
  const header = document.createElement("header");
  const title = document.createElement("h4");
  title.textContent = seg.name || seg.id;
  header.appendChild(title);
  header.appendChild(createStatusPill(seg.enabled ? t("badge_enabled") : t("badge_disabled"), seg.enabled ? "ok" : "warn"));
  card.appendChild(header);

  const meta = document.createElement("div");
  meta.className = "light-meta";
  meta.appendChild(createLightMetaRow(t("col_seg_leds"), seg.leds ? String(seg.leds) : t("not_set")));
  meta.appendChild(
    createLightMetaRow(
      t("lights_label_gpio"),
      typeof seg.gpio === "number" && seg.gpio >= 0 ? `GPIO ${seg.gpio}` : t("not_set")
    )
  );
  if (seg.engine) {
    meta.appendChild(createLightMetaRow(t("lights_label_engine"), seg.engine));
  }
  if (seg.effect) {
    meta.appendChild(createLightMetaRow(t("lights_label_effect"), seg.effect));
  }
  meta.appendChild(createLightMetaRow(t("lights_label_audio"), seg.audio_link ? t("badge_enabled") : t("badge_disabled")));
  card.appendChild(meta);
  return card;
}

function renderVirtualLightCard(entry) {
  const card = document.createElement("article");
  card.className = "light-card";
  const header = document.createElement("header");
  const title = document.createElement("h4");
  title.textContent = entry.name || entry.id || t("not_set");
  header.appendChild(title);
  header.appendChild(createStatusPill((entry.engine || "LED").toUpperCase(), entry.engine === "wled" ? "ok" : ""));
  card.appendChild(header);

  const meta = document.createElement("div");
  meta.className = "light-meta";
  meta.appendChild(createLightMetaRow(t("lights_label_engine"), entry.engine || t("not_set")));
  meta.appendChild(createLightMetaRow(t("lights_label_members"), entry.id || t("not_set")));
  meta.appendChild(createLightMetaRow(t("lights_label_effect"), entry.effect || t("not_set")));
  meta.appendChild(createLightMetaRow(t("lights_label_audio_mode"), entry.audio_mode || t("not_set")));
  meta.appendChild(createLightMetaRow(t("lights_label_audio"), entry.audio_link ? t("badge_enabled") : t("badge_disabled")));
  card.appendChild(meta);
  return card;
}

function renderLightsDashboard() {
  const groups = [
    {
      gridId: "lightsGridWled",
      countId: "lightsGroupWledCount",
      emptyKey: "lights_empty_wled",
      items: mergeWledDevices(),
      renderer: renderWledLightCard,
    },
    {
      gridId: "lightsGridPhysical",
      countId: "lightsGroupPhysicalCount",
      emptyKey: "lights_empty_physical",
      items: buildPhysicalSegmentsSummary(),
      renderer: renderPhysicalLightCard,
    },
    {
      gridId: "lightsGridVirtual",
      countId: "lightsGroupVirtualCount",
      emptyKey: "lights_empty_virtual",
      items: buildVirtualSegmentsSummary(),
      renderer: renderVirtualLightCard,
    },
  ];
  groups.forEach((group) => {
    const grid = qs(group.gridId);
    if (!grid) {
      return;
    }
    grid.innerHTML = "";
    const countEl = qs(group.countId);
    if (countEl) {
      countEl.textContent = formatLightsCount(group.items.length);
    }
    if (!group.items.length) {
      const empty = document.createElement("div");
      empty.className = "lights-empty";
      empty.textContent = t(group.emptyKey);
      grid.appendChild(empty);
      return;
    }
    group.items.forEach((item) => {
      grid.appendChild(group.renderer(item));
    });
  });
  
  // Update device preview after rendering dashboard
  renderDevicePreview();
}

function renderDevicePreview() {
  const grid = qs("previewDevicesGrid");
  if (!grid) return;
  
  const previewEnabled = qs("previewEnabled")?.checked ?? true;
  if (!previewEnabled) {
    grid.innerHTML = '<div class="preview-empty"><p class="muted">Live preview disabled</p></div>';
    return;
  }
  
  // Collect all devices (WLED, physical, virtual)
  const allDevices = [];
  
  // WLED devices
  const wledDevices = mergeWledDevices();
  const fx = ensureWledEffectsConfig();
  wledDevices.forEach(dev => {
    // Get actual binding state for this device
    const binding = fx.bindings.find(b => 
      b.device_id === (dev.id || dev.address || dev.name)
    );
    const isEnabled = binding ? (binding.enabled !== false) : (dev.on !== false);
    // Get effect name from binding, not from device
    const effectName = binding?.effect?.effect || null;
    allDevices.push({
      id: dev.id || `wled-${dev.name}`,
      name: dev.name || "WLED Device",
      type: "wled",
      leds: dev.leds || 0,
      effect: effectName,
      enabled: isEnabled,
      data: dev,
      binding: binding  // Store binding for later use
    });
  });
  
  // Physical segments
  const physicalSegs = buildPhysicalSegmentsSummary();
  physicalSegs.forEach(seg => {
    // Get effect assignment for physical segment
    const assignment = state.config?.led?.segments?.find(s => s.id === seg.id)?.effect;
    allDevices.push({
      id: seg.id || `physical-${seg.name}`,
      name: seg.name || "Physical Segment",
      type: "physical",
      leds: seg.leds || 0,
      effect: seg.effect || assignment?.effect || null,
      enabled: seg.enabled !== false,
      binding: assignment,  // Store effect assignment for colors/brightness
      data: seg
    });
  });
  
  // Virtual segments
  const virtualSegs = buildVirtualSegmentsSummary();
  virtualSegs.forEach(seg => {
    // Get effect assignment for virtual segment
    const virtualSeg = state.config?.virtual_segments?.find(v => v.id === seg.id);
    const assignment = virtualSeg?.effect;
    allDevices.push({
      id: seg.id || `virtual-${seg.name}`,
      name: seg.name || "Virtual Segment",
      type: "virtual",
      leds: seg.leds || 0,
      effect: seg.effect || assignment?.effect || null,
      enabled: seg.enabled !== false,
      binding: assignment,  // Store effect assignment for colors/brightness
      data: seg
    });
  });
  
  grid.innerHTML = "";
  
  if (allDevices.length === 0) {
    grid.innerHTML = '<div class="preview-empty"><p class="muted">No devices configured</p><p class="small muted">Add devices or segments to see live preview</p></div>';
    return;
  }
  
  allDevices.forEach(device => {
    const card = document.createElement("div");
    card.className = "preview-device-card";
    card.dataset.deviceId = device.id;
    card.dataset.deviceType = device.type;
    
    const header = document.createElement("div");
    header.className = "preview-device-header";
    
    const title = document.createElement("h4");
    title.className = "preview-device-title";
    title.textContent = device.name;
    header.appendChild(title);
    
    const typeBadge = document.createElement("span");
    typeBadge.className = "preview-device-type";
    typeBadge.textContent = device.type.toUpperCase();
    header.appendChild(typeBadge);
    
    card.appendChild(header);
    
    // Canvas container
    const canvasContainer = document.createElement("div");
    canvasContainer.className = "preview-device-canvas-container";
    
    const canvas = document.createElement("canvas");
    canvas.className = "preview-device-canvas";
    canvas.width = 280;
    canvas.height = 120;
    canvas.dataset.deviceId = device.id;
    canvasContainer.appendChild(canvas);
    
    card.appendChild(canvasContainer);
    
    // Device info
    const info = document.createElement("div");
    info.className = "preview-device-info";
    
    if (device.leds > 0) {
      const ledsSpan = document.createElement("span");
      ledsSpan.innerHTML = `<strong>${device.leds}</strong> LEDs`;
      info.appendChild(ledsSpan);
    }
    
    if (device.effect) {
      const effectSpan = document.createElement("span");
      effectSpan.innerHTML = `<strong>${device.effect}</strong>`;
      info.appendChild(effectSpan);
    }
    
    card.appendChild(info);
    
    // Actions
    const actions = document.createElement("div");
    actions.className = "preview-device-actions";
    
    const toggleBtn = document.createElement("button");
    toggleBtn.className = "preview-device-toggle primary subtle icon-only device-toggle";
    toggleBtn.dataset.state = device.enabled ? "playing" : "stopped";
    toggleBtn.setAttribute("aria-label", device.enabled ? "Stop Effect" : "Start Effect");
    const playIconStyle = device.enabled ? '' : 'style="display: none;"';
    const pauseIconStyle = device.enabled ? 'style="display: none;"' : '';
    toggleBtn.innerHTML = `
      <svg viewBox="0 0 24 24" aria-hidden="true" focusable="false" style="width: 20px; height: 20px;">
        <path d="M8 5v14l11-7z" class="icon-play" ${playIconStyle}/>
        <path d="M6 6h4v12H6zm8 0h4v12h-4z" class="icon-pause" ${pauseIconStyle}/>
      </svg>
    `;
    toggleBtn.addEventListener("click", async (e) => {
      e.stopPropagation();
      e.preventDefault();
      // Toggle device enabled state - INDEPENDENT from global start/stop
      if (device.type === "wled") {
        // Toggle WLED device binding
        const binding = getWledBinding(device.data.id || device.data.address || device.data.name);
        if (binding) {
          const wasEnabled = binding.enabled !== false;
          binding.enabled = !wasEnabled;
          await saveWledEffects();
          
          // Update button state and icons immediately
          const newState = binding.enabled ? "playing" : "stopped";
          toggleBtn.dataset.state = newState;
          const playIcon = toggleBtn.querySelector(".icon-play");
          const pauseIcon = toggleBtn.querySelector(".icon-pause");
          if (playIcon) playIcon.style.display = binding.enabled ? "" : "none";
          if (pauseIcon) pauseIcon.style.display = binding.enabled ? "none" : "";
          
          // If disabling, backend will handle stopping DDP and restoring previous state
          // No need to send commands from frontend - backend does this automatically
          if (binding.enabled) {
            // If enabling, ensure DDP is enabled
            binding.ddp = true;
            await saveWledEffects();
          }
          
          // Re-render to update all device previews
          renderDevicePreview();
        }
      } else if (device.type === "physical") {
        // Toggle physical segment
        const led = ensureLedEngineConfig();
        if (led && device.data) {
          const seg = (led.segments || []).find(s => s.id === device.data.id || s.segment_id === device.data.segment_id);
          if (seg) {
            const wasEnabled = seg.enabled !== false;
            seg.enabled = !wasEnabled;
            await saveLedConfig();
            
            // Update button state and icons immediately
            const newState = seg.enabled ? "playing" : "stopped";
            toggleBtn.dataset.state = newState;
            const playIcon = toggleBtn.querySelector(".icon-play");
            const pauseIcon = toggleBtn.querySelector(".icon-pause");
            if (playIcon) playIcon.style.display = seg.enabled ? "" : "none";
            if (pauseIcon) pauseIcon.style.display = seg.enabled ? "none" : "";
            
            // Re-render to update all device previews
            renderDevicePreview();
          }
        }
      } else if (device.type === "virtual") {
        // Toggle virtual segment
        ensureVirtualSegments();
        const seg = (state.config.virtual_segments || []).find(s => s.id === device.data.id || s.name === device.data.name);
        if (seg) {
          const wasEnabled = seg.enabled !== false;
          seg.enabled = !wasEnabled;
          await saveLedConfig();
          
          // Update button state and icons immediately
          const newState = seg.enabled !== false ? "playing" : "stopped";
          toggleBtn.dataset.state = newState;
          const playIcon = toggleBtn.querySelector(".icon-play");
          const pauseIcon = toggleBtn.querySelector(".icon-pause");
          if (playIcon) playIcon.style.display = seg.enabled !== false ? "" : "none";
          if (pauseIcon) pauseIcon.style.display = seg.enabled !== false ? "none" : "";
          
          // Re-render to update all device previews
          renderDevicePreview();
        }
      }
    });
    actions.appendChild(toggleBtn);
    
    card.appendChild(actions);
    
    // Click handler to select device and open effect configuration
    card.addEventListener("click", () => {
      // Remove previous selection
      grid.querySelectorAll(".preview-device-card").forEach(c => c.classList.remove("selected"));
      card.classList.add("selected");
      
      // Switch to LED tab to show device configuration
      const navLedBtn = qs("navLed");
      if (navLedBtn) {
        navLedBtn.click();
      }
      
      // Show device effect configuration after a short delay to allow tab switch
      setTimeout(() => {
        if (device.type === "wled") {
          // Switch to WLED pane and show device effect form
          const wledTab = qs("tabConfigWled");
          if (wledTab) wledTab.click();
          state.selectedDeviceKey = `wled:${device.data.id || device.data.address || device.data.name}`;
          renderDeviceExplorer();
          // Open effect configuration form for this WLED device
          const deviceEntry = { type: "wled", meta: device.data };
          renderDeviceDetail(deviceEntry);
          // Scroll to effect form
          setTimeout(() => {
            const detailCard = qs("wledDeviceDetailCard");
            if (detailCard) {
              detailCard.scrollIntoView({ behavior: "smooth", block: "nearest" });
            }
          }, 200);
        } else if (device.type === "physical") {
          // Switch to Segments pane and show segment effect form
          const segTab = qs("tabConfigSegments");
          if (segTab) segTab.click();
          state.selectedDeviceKey = `physical:${device.data.id || device.data.segment_id}`;
          renderDeviceExplorer();
          const deviceEntry = { type: "physical", meta: device.data };
          renderDeviceDetail(deviceEntry);
          // Scroll to effect form
          setTimeout(() => {
            const detailCard = qs("segmentDeviceDetailCard");
            if (detailCard) {
              detailCard.scrollIntoView({ behavior: "smooth", block: "nearest" });
            }
          }, 200);
        } else if (device.type === "virtual") {
          // Switch to Virtual segments pane and show segment effect form
          const virtTab = qs("tabConfigVirtual");
          if (virtTab) virtTab.click();
          state.selectedDeviceKey = `virtual:${device.data.id || device.data.name}`;
          renderDeviceExplorer();
          const deviceEntry = { type: "virtual", meta: device.data };
          renderDeviceDetail(deviceEntry);
          // Scroll to effect form
          setTimeout(() => {
            const detailCard = qs("virtualDeviceDetailCard");
            if (detailCard) {
              detailCard.scrollIntoView({ behavior: "smooth", block: "nearest" });
            }
          }, 200);
        }
      }, 150);
    });
    
    grid.appendChild(card);
  });
  
  // Start preview animation
  startPreviewAnimation();
}

let previewAnimationFrame = null;
let previewFrameIndex = 0;
let previewStartTime = null;
const PREVIEW_FPS = 30; // Limit preview to 30 FPS for smoother animation

function startPreviewAnimation() {
  if (previewAnimationFrame) {
    cancelAnimationFrame(previewAnimationFrame);
  }
  
  const canvases = document.querySelectorAll(".preview-device-canvas");
  if (canvases.length === 0) {
    console.log("startPreviewAnimation: No canvases found, preview may not be initialized yet");
    return;
  }
  
  const previewEnabled = qs("previewEnabled")?.checked ?? true;
  if (!previewEnabled) {
    console.log("startPreviewAnimation: Preview disabled by user");
    return;
  }
  
  console.log(`startPreviewAnimation: Starting animation for ${canvases.length} canvas(es) at ${PREVIEW_FPS} FPS`);
  previewStartTime = performance.now();
  previewFrameIndex = 0;
  
  function animate(currentTime) {
    // Calculate frame index based on time (not requestAnimationFrame calls)
    // This ensures consistent animation speed regardless of browser FPS
    if (previewStartTime === null) {
      previewStartTime = currentTime;
    }
    const elapsedSeconds = (currentTime - previewStartTime) / 1000;
    const targetFrameIndex = Math.floor(elapsedSeconds * PREVIEW_FPS);
    
    // Only update if we need a new frame (throttle to PREVIEW_FPS)
    if (targetFrameIndex > previewFrameIndex) {
      previewFrameIndex = targetFrameIndex;
      
      let renderedCount = 0;
      canvases.forEach(canvas => {
        const deviceId = canvas.dataset.deviceId;
        const card = canvas.closest(".preview-device-card");
        if (!card) return;
        
        const deviceType = card.dataset.deviceType;
        const deviceData = getDeviceData(deviceId, deviceType);
        
        if (deviceData && deviceData.enabled !== false) {
          renderPreviewFrame(canvas, deviceData, previewFrameIndex);
          renderedCount++;
        } else {
          // Clear canvas if disabled
          const ctx = canvas.getContext("2d");
          ctx.fillStyle = "#0a0f18";
          ctx.fillRect(0, 0, canvas.width, canvas.height);
        }
      });
      
      // Log every 5 seconds
      if (previewFrameIndex % (PREVIEW_FPS * 5) === 0 && renderedCount > 0) {
        console.log(`Preview animation: rendering ${renderedCount} device(s), frame ${previewFrameIndex} (${elapsedSeconds.toFixed(1)}s)`);
      }
    }
    
    previewAnimationFrame = requestAnimationFrame(animate);
  }
  
  animate(performance.now());
}

function getDeviceData(deviceId, deviceType) {
  // Get device data from state
  if (deviceType === "wled") {
    // Find device in wledDevices
    const dev = state.wledDevices?.find(d => (d.id || `wled-${d.name}`) === deviceId);
    if (!dev) return null;
    
    // Get binding to access effect configuration
    const fx = ensureWledEffectsConfig();
    const binding = fx.bindings.find(b => 
      b.device_id === (dev.id || dev.address || dev.name)
    );
    
    // Return device data with effect from binding
    return {
      id: dev.id || `wled-${dev.name}`,
      name: dev.name || "WLED Device",
      type: "wled",
      leds: dev.leds || 0,
      effect: binding?.effect?.effect || null,
      enabled: binding ? (binding.enabled !== false) : (dev.on !== false),
      binding: binding,  // Store full binding for effect details (colors, brightness, etc.)
      data: dev
    };
  } else if (deviceType === "physical") {
    const segs = buildPhysicalSegmentsSummary();
    const seg = segs.find(s => (s.id || `physical-${s.name}`) === deviceId);
    if (!seg) return null;
    
    // Get effect assignment for physical segment
    const assignment = state.config?.led?.segments?.find(s => s.id === seg.id)?.effect;
    return {
      ...seg,
      effect: seg.effect || assignment?.effect || null,
      binding: assignment  // Store effect assignment for colors/brightness
    };
  } else if (deviceType === "virtual") {
    const segs = buildVirtualSegmentsSummary();
    const seg = segs.find(s => (s.id || `virtual-${s.name}`) === deviceId);
    if (!seg) return null;
    
    // Get effect assignment for virtual segment
    const virtualSeg = state.config?.virtual_segments?.find(v => v.id === seg.id);
    const assignment = virtualSeg?.effect;
    return {
      ...seg,
      effect: seg.effect || assignment?.effect || null,
      binding: assignment  // Store effect assignment for colors/brightness
    };
  }
  return null;
}

function renderPreviewFrame(canvas, device, frameIndex) {
  const ctx = canvas.getContext("2d");
  const width = canvas.width;
  const height = canvas.height;
  const leds = device.leds || 60;
  
  // Clear canvas
  ctx.fillStyle = "#0a0f18";
  ctx.fillRect(0, 0, width, height);
  
  // Get effect name and colors from device or binding
  const effect = device.effect || (device.binding?.effect?.effect) || "Solid";
  const binding = device.binding || device.data?.binding;
  const effectConfig = binding?.effect || {};
  
  // Parse colors from binding or use defaults
  const parseColor = (hex) => {
    if (!hex || typeof hex !== "string") return null;
    const clean = hex.replace("#", "");
    if (clean.length !== 6) return null;
    const r = parseInt(clean.substr(0, 2), 16);
    const g = parseInt(clean.substr(2, 2), 16);
    const b = parseInt(clean.substr(4, 2), 16);
    return { r, g, b };
  };
  
  const c1 = parseColor(effectConfig.color1) || { r: 255, g: 255, b: 255 };
  const c2 = parseColor(effectConfig.color2) || { r: 153, g: 102, b: 0 };
  const c3 = parseColor(effectConfig.color3) || { r: 0, g: 51, b: 255 };
  
  // Get brightness (0-1)
  const brightness = effectConfig.brightness ? Math.min(1, effectConfig.brightness / 255) : 1;
  const intensity = effectConfig.intensity ? effectConfig.intensity / 255 : 0.5;
  const speed = effectConfig.speed ? 0.02 + (effectConfig.speed / 255) * 0.25 : 0.1;
  
  const pixelWidth = width / leds;
  const effectLower = effect.toLowerCase();
  
  for (let i = 0; i < leds; i++) {
    let r = 0, g = 0, b = 0;
    const pos = i / leds;
    
    if (effectLower.includes("rainbow")) {
      const hue = ((pos * 360 + frameIndex * speed * 10) % 360) / 360;
      const rgb = hslToRgb(hue, 1, 0.5);
      r = rgb[0];
      g = rgb[1];
      b = rgb[2];
    } else if (effectLower.includes("solid")) {
      // Solid color with slight pulse
      const pulse = 0.8 + Math.sin(frameIndex * speed * 5) * 0.2;
      r = c1.r * pulse;
      g = c1.g * pulse;
      b = c1.b * pulse;
    } else if (effectLower.includes("fire")) {
      const intensity = Math.sin((pos * Math.PI * 2 + frameIndex * speed * 2) * 0.5) * 0.5 + 0.5;
      r = Math.floor(c1.r * intensity);
      g = Math.floor(c1.g * intensity * 0.4);
      b = Math.floor(c1.b * intensity * 0.1);
    } else if (effectLower.includes("wave") || effectLower.includes("flow")) {
      const wave = Math.sin((pos * Math.PI * 4 + frameIndex * speed * 3)) * 0.5 + 0.5;
      r = c1.r * (1 - wave) + c2.r * wave;
      g = c1.g * (1 - wave) + c2.g * wave;
      b = c1.b * (1 - wave) + c2.b * wave;
    } else if (effectLower.includes("chase") || effectLower.includes("theater")) {
      const move = (frameIndex * speed * 5) % 1;
      const dist = Math.abs(pos - move);
      const level = dist < 0.1 ? 1 : (dist < 0.2 ? 0.5 : 0);
      r = c1.r * level;
      g = c1.g * level;
      b = c1.b * level;
    } else if (effectLower.includes("meteor")) {
      const move = (frameIndex * speed * 3) % 1;
      const dist = Math.abs(pos - move);
      const level = dist < 0.15 ? (1 - dist / 0.15) : 0;
      r = c1.r * level;
      g = c1.g * level;
      b = c1.b * level;
    } else if (effectLower.includes("rain")) {
      const drop = Math.sin((pos * 10 + frameIndex * speed * 2) % Math.PI) * 0.5 + 0.5;
      r = c1.r * drop;
      g = c1.g * drop;
      b = c1.b * drop;
    } else {
      // Default: gradient flow
      const t = (pos + frameIndex * speed * 0.5) % 1;
      const mix = Math.sin(t * Math.PI * 2) * 0.5 + 0.5;
      r = c1.r * (1 - t) + c2.r * t;
      g = c1.g * (1 - t) + c2.g * t;
      b = c1.b * (1 - t) + c2.b * t;
      r = r * (1 - mix) + c3.r * mix;
      g = g * (1 - mix) + c3.g * mix;
      b = b * (1 - mix) + c3.b * mix;
    }
    
    // Apply brightness
    r = Math.floor(r * brightness);
    g = Math.floor(g * brightness);
    b = Math.floor(b * brightness);
    
    // Ensure minimum visibility
    if (r === 0 && g === 0 && b === 0 && brightness > 0) {
      r = Math.floor(c1.r * brightness * 0.1);
      g = Math.floor(c1.g * brightness * 0.1);
      b = Math.floor(c1.b * brightness * 0.1);
    }
    
    ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.fillRect(i * pixelWidth, 0, pixelWidth, height);
  }
}

function hslToRgb(h, s, l) {
  let r, g, b;
  if (s === 0) {
    r = g = b = l;
  } else {
    const hue2rgb = (p, q, t) => {
      if (t < 0) t += 1;
      if (t > 1) t -= 1;
      if (t < 1/6) return p + (q - p) * 6 * t;
      if (t < 1/2) return q;
      if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
      return p;
    };
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    r = hue2rgb(p, q, h + 1/3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1/3);
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

function renderWledDetail(dev) {
  const detail = qs("wledDeviceDetailBody");
  const card = qs("wledDeviceDetailCard");
  if (!detail || !card) return;
  
  // Show the detail card
  card.style.display = "block";
  if (!dev) {
    detail.innerHTML = `<div class="placeholder muted">${t("device_detail_placeholder") || "Select device"}</div>`;
    return;
  }
  const fx = ensureWledEffectsConfig();
  const binding = getWledBinding(dev.id || dev.address || dev.name || "wled");
  detail.innerHTML = `
    <div class="device-meta-grid">
      <div class="device-meta"><strong>${t("lights_label_host") || "Host"}</strong><span>${dev.address || dev.ip || "-"}</span></div>
      <div class="device-meta"><strong>${t("wled_table_leds") || "LEDs"}</strong><span>${dev.leds || t("not_set")}</span></div>
      <div class="device-meta"><strong>${t("wled_table_segments") || "Segments"}</strong><span>${dev.segments || t("not_set")}</span></div>
      <div class="device-meta"><strong>${t("wled_table_last_seen") || "Last seen"}</strong><span>${formatAgeMs(dev.age_ms)}</span></div>
      <div class="device-meta"><strong>${t("wled_table_status") || "Status"}</strong><span>${dev.online ? (t("wled_status_online") || "online") : (t("wled_status_offline") || "offline")}</span></div>
    </div>
    <div class="form-grid compact">
      <label>${t("target_fps") || "Target FPS"}
        <input type="number" min="1" max="240" id="wledFxFps" value="${fx.target_fps || 60}">
      </label>
      <label>${t("wled_segment_index") || "Segment"}
        <input type="number" min="0" max="${dev.segments || 1}" id="wledFxSegment" value="${binding.segment_index || 0}">
      </label>
      <label>${t("wled_audio_channel") || "Audio channel"}
        <select id="wledFxAudioChannel">
          <option value="mix" ${binding.audio_channel === "mix" ? "selected" : ""}>${t("audio_mix") || "Mix (L+R)"}</option>
          <option value="left" ${binding.audio_channel === "left" ? "selected" : ""}>${t("audio_left") || "Left"}</option>
          <option value="right" ${binding.audio_channel === "right" ? "selected" : ""}>${t("audio_right") || "Right"}</option>
        </select>
      </label>
      <label class="switch">
        <input type="checkbox" id="wledFxEnabled" ${binding.enabled !== false ? "checked" : ""}>
        <span></span> ${t("badge_enabled") || "Enabled"}
      </label>
      <label class="switch">
        <input type="checkbox" id="wledFxDdp" ${binding.ddp !== false ? "checked" : ""}>
        <span></span> ${t("wled_ddp") || "DDP stream"}
      </label>
    </div>
    <div class="form-hint" style="margin-top: 0.5rem; padding: 0.75rem; background: rgba(255, 140, 210, 0.1); border-radius: 8px; border: 1px solid rgba(255, 140, 210, 0.2);">
      <strong>⚠️ WLED Device Setup:</strong><br>
      <small>1. Enable DDP in WLED: Settings → Sync Settings → Receive via DDP<br>
      2. Ensure WLED device is on the same network (Ethernet or WiFi)<br>
      3. Check that device IP address is correct<br>
      4. Device must be in DDP receive mode (not running local effects)</small>
    </div>
    <div id="wledEffectForm"></div>
    <div class="actions" style="display: flex; gap: 0.5rem; align-items: center;">
      <button class="primary subtle icon-only device-toggle" id="btnWledToggle" data-state="${binding.enabled !== false ? "playing" : "stopped"}" title="" aria-label="Start/Stop Effect">
        <svg viewBox="0 0 24 24" aria-hidden="true" focusable="false" style="width: 20px; height: 20px;">
          <path d="M8 5v14l11-7z" class="icon-play" ${binding.enabled === false ? 'style="display: none;"' : ''}/>
          <path d="M6 6h4v12H6zm8 0h4v12h-4z" class="icon-pause" ${binding.enabled === false ? '' : 'style="display: none;"'}/>
        </svg>
      </button>
      <button class="primary" id="btnSaveWledFx">${t("btn_save_led") || "Save"}</button>
    </div>
  `;

  const formContainer = qs("wledEffectForm");
  const fakeSegment = { name: dev.name || dev.id || "WLED", led_count: dev.leds || 0, audio: defaultSegmentAudio() };
  if (formContainer) {
    // Pass binding as additional context for effect change handler
    renderEffectDetailForm(formContainer, fakeSegment, binding.effect, true, binding);  // true = isWled, binding for context
  }

  const fpsInput = qs("wledFxFps");
  fpsInput?.addEventListener("change", (e) => {
    const val = parseInt(e.target.value, 10);
    fx.target_fps = Math.max(1, Math.min(Number.isFinite(val) ? val : 60, 240));
    e.target.value = fx.target_fps;
  });
  qs("wledFxSegment")?.addEventListener("change", (ev) => {
    let val = parseInt(ev.target.value, 10);
    if (!Number.isFinite(val) || val < 0) val = 0;
    binding.segment_index = val;
    ev.target.value = val;
  });
  qs("wledFxEnabled")?.addEventListener("change", (ev) => {
    binding.enabled = ev.target.checked;
    // Update toggle button state and icons
    const toggleBtn = qs("btnWledToggle");
    if (toggleBtn) {
      toggleBtn.dataset.state = binding.enabled ? "playing" : "stopped";
      const playIcon = toggleBtn.querySelector(".icon-play");
      const pauseIcon = toggleBtn.querySelector(".icon-pause");
      if (playIcon) playIcon.style.display = binding.enabled ? "" : "none";
      if (pauseIcon) pauseIcon.style.display = binding.enabled ? "none" : "";
    }
    // Auto-save when enabled is toggled
    saveWledEffects();
  });
  
  // Remove any existing event listeners to prevent duplicates
  const existingBtn = qs("btnWledToggle");
  if (existingBtn) {
    const newBtn = existingBtn.cloneNode(true);
    existingBtn.parentNode?.replaceChild(newBtn, existingBtn);
  }
  
  // Toggle button for play/pause - INDEPENDENT from global start/stop
  qs("btnWledToggle")?.addEventListener("click", async (ev) => {
    ev.preventDefault();
    ev.stopPropagation();
    const wasEnabled = binding.enabled !== false;
    binding.enabled = !wasEnabled;
    
    // Update UI immediately for visual feedback
    const toggleBtn = qs("btnWledToggle");
    if (toggleBtn) {
      const newState = binding.enabled ? "playing" : "stopped";
      toggleBtn.dataset.state = newState;
      const playIcon = toggleBtn.querySelector(".icon-play");
      const pauseIcon = toggleBtn.querySelector(".icon-pause");
      if (playIcon) playIcon.style.display = binding.enabled ? "" : "none";
      if (pauseIcon) pauseIcon.style.display = binding.enabled ? "none" : "";
    }
    
    const enabledCheckbox = qs("wledFxEnabled");
    if (enabledCheckbox) {
      enabledCheckbox.checked = binding.enabled;
    }
    
    // Save configuration
    await saveWledEffects();
    
    // If disabling, backend will handle stopping DDP and restoring previous state
    // No need to send commands from frontend - backend does this automatically
    if (binding.enabled) {
      // If enabling, ensure DDP is enabled
      binding.ddp = true;
      const ddpCheckbox = qs("wledFxDdp");
      if (ddpCheckbox) {
        ddpCheckbox.checked = true;
      }
      await saveWledEffects();
    }
    
    // Update device preview if visible
    renderDevicePreview();
  });
  qs("wledFxDdp")?.addEventListener("change", (ev) => {
    binding.ddp = ev.target.checked;
    // Auto-save when DDP is toggled
    saveWledEffects();
  });
  qs("wledFxAudioChannel")?.addEventListener("change", (ev) => {
    binding.audio_channel = ev.target.value || "mix";
  });
  qs("btnSaveWledFx")?.addEventListener("click", (ev) => {
    ev.preventDefault();
    saveWledEffects();
  });
}

function renderEffectDetailForm(target, segment, assignment, isWled = false, wledBinding = null, virtualSegment = null) {
  if (!target) return;
  const saveFn = isWled ? saveWledEffects : autoSaveLedConfig;
  // Replace all autoSaveLedConfig() calls with saveFn() in this function scope
  const name = segment?.name || assignment?.segment_id || t("not_set");
  const audio = segment?.audio || defaultSegmentAudio();
  // Remember accordion state before re-render
  const advancedDetails = target.querySelector('.fx-section-advanced');
  if (advancedDetails) {
    state.fxFormAccordionState.advanced = advancedDetails.hasAttribute('open');
  }
  const audioAdvancedDetails = target.querySelectorAll('.fx-section-category');
  if (audioAdvancedDetails.length > 0) {
    // Find Audio Processing section (last fx-section-category)
    const audioProcessing = Array.from(audioAdvancedDetails).find(el => 
      el.querySelector('summary')?.textContent?.includes('Audio Processing') ||
      el.querySelector('summary')?.textContent?.includes('Audio Advanced')
    );
    if (audioProcessing) {
      state.fxFormAccordionState.audioAdvanced = audioProcessing.hasAttribute('open');
    }
  }
  target.innerHTML = `
    <div class="device-meta-grid">
      <div class="device-meta"><strong>${t("colSegName") || "Name"}</strong><span>${name}</span></div>
      <div class="device-meta"><strong>${t("colSegLeds") || "LEDs"}</strong><span>${segment?.led_count ?? "-"}</span></div>
      <div class="device-meta"><strong>${t("fx_engine_label") || "Effect Engine"}</strong><span>${assignment.engine || "wled"}</span></div>
    </div>
    <details class="fx-section-category" open>
      <summary class="fx-section-header">${t("fx_basic_title") || "Basic Settings"}</summary>
      <div class="form-grid" style="margin-top: 1rem;">
        <label>${t("fx_engine_label") || "Effect Engine"}
          <select id="devFxEngine">
            <option value="wled" ${assignment.engine === "wled" ? "selected" : ""}>WLED (time-based, no audio)</option>
            <option value="ledfx" ${assignment.engine === "ledfx" ? "selected" : ""}>LEDFx (audio-reactive)</option>
          </select>
          <small class="muted">${t("fx_engine_desc") || "WLED = time-based animations, LEDFx = audio-reactive effects"}</small>
        </label>
        <label style="grid-column: span 2;">${t("fx_effect_label") || "Effect Name"}
          <select id="devFxEffect">
            <option value="">Loading...</option>
          </select>
          <small class="muted" id="fxEffectDescription" style="display: block; margin-top: 0.25rem;"></small>
        </label>
        <label>${t("fx_preset_label") || "Preset Name (Optional)"}
          <input id="devFxPreset" value="${assignment.preset || ''}" placeholder="Optional preset name">
          <small class="muted">${t("fx_preset_desc") || "Optional name to save/load this effect configuration"}</small>
        </label>
      </div>
    </details>
    <details class="fx-section-category" open>
      <summary class="fx-section-header">${t("fx_colors_title") || "Colors & Appearance"}</summary>
      <div class="form-grid" style="margin-top: 1rem;">
        <label>${t("fx_color1_label") || "Primary Color (Main)"}
          <div>
            <input type="color" id="devFxColor1" value="${assignment.color1 || '#ffffff'}">
            <input type="text" id="devFxColor1Text" value="${assignment.color1 || '#ffffff'}" placeholder="#ffffff">
          </div>
          <small class="muted">${t("fx_color1_desc") || "Main color used by the effect"}</small>
        </label>
        <label>${t("fx_color2_label") || "Secondary Color (Accent)"}
          <div>
            <input type="color" id="devFxColor2" value="${assignment.color2 || '#ff6600'}">
            <input type="text" id="devFxColor2Text" value="${assignment.color2 || '#ff6600'}" placeholder="#ff6600">
          </div>
          <small class="muted">${t("fx_color2_desc") || "Secondary/accent color for effects that use multiple colors"}</small>
        </label>
        <label>${t("fx_color3_label") || "Tertiary Color (Third)"}
          <div>
            <input type="color" id="devFxColor3" value="${assignment.color3 || '#0033ff'}">
            <input type="text" id="devFxColor3Text" value="${assignment.color3 || '#0033ff'}" placeholder="#0033ff">
          </div>
          <small class="muted">${t("fx_color3_desc") || "Third color for effects that use three colors"}</small>
        </label>
        <label>${t("fx_palette_label") || "Color Palette (Predefined)"}
          <select id="devFxPalette">
            <option value="" ${!assignment.palette ? "selected" : ""}>None (use individual colors above)</option>
            <option value="sunset" ${assignment.palette === "sunset" ? "selected" : ""}>Sunset (warm orange-red)</option>
            <option value="fire" ${assignment.palette === "fire" ? "selected" : ""}>Fire (red-orange-yellow)</option>
            <option value="icy" ${assignment.palette === "icy" ? "selected" : ""}>Icy (blue-cyan-white)</option>
            <option value="forest" ${assignment.palette === "forest" ? "selected" : ""}>Forest (green-brown)</option>
            <option value="ocean" ${assignment.palette === "ocean" ? "selected" : ""}>Ocean (blue-cyan)</option>
            <option value="aqua" ${assignment.palette === "aqua" ? "selected" : ""}>Aqua (turquoise-blue)</option>
            <option value="party" ${assignment.palette === "party" ? "selected" : ""}>Party (multicolor rainbow)</option>
          </select>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_palette_desc") || "Predefined color palette that overrides individual colors if selected"}</small>
        </label>
        <label style="grid-column: span 2;">${t("fx_gradient_label") || "Custom Color Gradient"}
          <div style="display: flex; gap: 0.5rem; align-items: center;">
            <input id="devFxGradient" value="${assignment.gradient || ''}" placeholder="#000-#fff-#0ff" style="flex: 1;">
            <button type="button" class="ghost small" id="btnGradientHelper" title="${t("fx_gradient_helper") || "Gradient helper"}">?</button>
          </div>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_gradient_desc") || "Custom gradient: #color1-#color2-#color3 (separate colors with dash, comma, semicolon, or space)"}</small>
        </label>
      </div>
    </details>
    <details class="fx-section-category" open>
      <summary class="fx-section-header">${t("fx_animation_title") || "Animation & Movement"}</summary>
      <div class="form-grid" style="margin-top: 1rem;">
        <label>${t("fx_brightness_label") || "Effect Brightness"}
          <div class="slider-shell">
            <input type="range" min="0" max="255" id="devFxBrightness" value="${assignment.brightness ?? 255}">
            <span class="slider-value" id="devFxBrightnessValue">${assignment.brightness ?? 255}</span>
          </div>
          <small class="muted">${t("fx_brightness_desc") || "Overall brightness of the effect (0-255, 255 = maximum)"}</small>
        </label>
        <label>${t("fx_intensity_label") || "Effect Intensity / Strength"}
          <div class="slider-shell">
            <input type="range" min="0" max="255" id="devFxIntensity" value="${assignment.intensity ?? 128}">
            <span class="slider-value" id="devFxIntensityValue">${assignment.intensity ?? 128}</span>
          </div>
          <small class="muted">${t("fx_intensity_desc") || "How strong/prominent the effect is (0-255, higher = more visible)"}</small>
        </label>
        <label>${t("fx_speed_label") || "Animation Speed"}
          <div class="slider-shell">
            <input type="range" min="0" max="255" id="devFxSpeed" value="${assignment.speed ?? 128}">
            <span class="slider-value" id="devFxSpeedValue">${assignment.speed ?? 128}</span>
          </div>
          <small class="muted">${t("fx_speed_desc") || "How fast the effect animates (0-255, higher = faster movement)"}</small>
        </label>
        <label>${t("fx_direction_label") || "Animation Direction"}
          <select id="devFxDirection">
            <option value="forward" ${assignment.direction === "forward" ? "selected" : ""}>Forward (Start → End)</option>
            <option value="reverse" ${assignment.direction === "reverse" || assignment.direction === "backward" ? "selected" : ""}>Reverse (End → Start)</option>
            <option value="center_out" ${assignment.direction === "center_out" ? "selected" : ""}>Center Out (Middle → Edges)</option>
            <option value="edges_in" ${assignment.direction === "edges_in" ? "selected" : ""}>Edges In (Edges → Middle)</option>
          </select>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_direction_desc") || "Which direction the effect moves along the LED strip"}</small>
        </label>
        <label>${t("fx_scatter_label") || "Randomness / Particle Spread"}
          <input type="number" id="devFxScatter" min="0" max="100" step="1" value="${assignment.scatter ?? 0}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_scatter_desc") || "How random/spread out the effect particles are (0-100, 0 = uniform, 100 = very random)"}</small>
        </label>
        <label>${t("fx_fadein_label") || "Fade-in Time (when starting)"}
          <input type="number" id="devFxFadeIn" min="0" max="5000" value="${assignment.fade_in ?? 0}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_fadein_desc") || "Time in milliseconds for effect to gradually appear when starting (0 = instant)"}</small>
        </label>
        <label>${t("fx_fadeout_label") || "Fade-out Time (when stopping)"}
          <input type="number" id="devFxFadeOut" min="0" max="5000" value="${assignment.fade_out ?? 0}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_fadeout_desc") || "Time in milliseconds for effect to gradually disappear when stopping (0 = instant)"}</small>
        </label>
      </div>
    </details>
    <details class="fx-section-category" ${assignment.audio_link && assignment.engine !== "wled" ? "open" : ""}>
      <summary class="fx-section-header">${t("fx_audio_title") || "Audio Reactive"}</summary>
      <div class="form-grid" style="margin-top: 1rem;">
        <label class="switch">
          <input type="checkbox" id="devFxAudioLink" ${assignment.audio_link ? "checked" : ""} ${assignment.engine === "wled" ? "disabled" : ""}>
          <span></span> ${t("fx_audio_link_label") || "Enable Audio Reactivity"}
          <small class="muted" style="display: block; margin-top: 0.25rem;">${assignment.engine === "wled" ? (t("fx_audio_only_ledfx") || "Only available for LEDFx effects") : (t("fx_audio_link_desc") || "Make the effect respond to music/audio input")}</small>
        </label>
        ${assignment.audio_link && assignment.engine !== "wled" ? `
        <label>${t("fx_audio_profile_label") || "Audio Response Profile"}
          <select id="devFxAudioProfile" ${assignment.engine === "wled" ? "disabled" : ""}>
            ${renderAudioProfileOptions(assignment.audio_profile || "default")}
          </select>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${assignment.engine === "wled" ? (t("fx_audio_only_ledfx") || "Only for LEDFx effects") : (t("fx_audio_profile_desc") || "Predefined audio processing profile (default, energy, tempo, etc.)")}</small>
        </label>
        <label>${t("fx_audio_mode_label") || "Audio Analysis Mode"}
          <select id="devFxAudioMode" ${assignment.engine === "wled" ? "disabled" : ""}>
            ${renderAudioModeOptions(assignment.audio_mode || "spectrum")}
          </select>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${assignment.engine === "wled" ? (t("fx_audio_only_ledfx") || "Only for LEDFx effects") : (t("fx_audio_mode_desc") || "How audio is analyzed: Spectrum (frequency bands), Energy (overall level), Beat (rhythm detection)")}</small>
        </label>
        <label style="grid-column: 1 / -1;">${t("fx_audio_bands_label") || "Frequency Bands / Audio Metrics"}
          <div id="devFxBandsContainer" style="display: flex; flex-direction: column; gap: 0.5rem; margin-top: 0.5rem;">
            ${(() => {
              const bands = assignment.selected_bands || [];
              if (bands.length === 0) {
                // If no bands selected, show reactive_mode as fallback
                return `<div class="band-selector-row" data-idx="0">
                  <select class="band-select" data-idx="0" ${assignment.engine === "wled" ? "disabled" : ""}>
                    <option value="">${t("fx_audio_use_reactive_mode") || "Use reactive mode"}</option>
                    <option value="sub_bass">Sub Bass (20-60 Hz)</option>
                    <option value="bass_low">Bass Low (60-120 Hz)</option>
                    <option value="bass_high">Bass High (120-250 Hz)</option>
                    <option value="mid_low">Mid Low (250-500 Hz)</option>
                    <option value="mid_mid">Mid Mid (500-1000 Hz)</option>
                    <option value="mid_high">Mid High (1000-2000 Hz)</option>
                    <option value="treble_low">Treble Low (2000-4000 Hz)</option>
                    <option value="treble_mid">Treble Mid (4000-8000 Hz)</option>
                    <option value="treble_high">Treble High (8000-12000 Hz)</option>
                    <option value="bass">Bass (combined)</option>
                    <option value="mid">Mid (combined)</option>
                    <option value="treble">Treble (combined)</option>
                    <option value="energy">Energy</option>
                    <option value="beat">Beat</option>
                    <option value="tempo_bpm">Tempo (BPM)</option>
                  </select>
                  <button type="button" class="btn-icon band-add" title="${t("fx_audio_add_band") || "Add band"}" ${assignment.engine === "wled" ? "disabled" : ""}>+</button>
                </div>`;
              }
              return bands.map((band, idx) => `
                <div class="band-selector-row" data-idx="${idx}">
                  <select class="band-select" data-idx="${idx}" ${assignment.engine === "wled" ? "disabled" : ""}>
                    <option value="sub_bass" ${band === "sub_bass" ? "selected" : ""}>Sub Bass (20-60 Hz)</option>
                    <option value="bass_low" ${band === "bass_low" ? "selected" : ""}>Bass Low (60-120 Hz)</option>
                    <option value="bass_high" ${band === "bass_high" ? "selected" : ""}>Bass High (120-250 Hz)</option>
                    <option value="mid_low" ${band === "mid_low" ? "selected" : ""}>Mid Low (250-500 Hz)</option>
                    <option value="mid_mid" ${band === "mid_mid" ? "selected" : ""}>Mid Mid (500-1000 Hz)</option>
                    <option value="mid_high" ${band === "mid_high" ? "selected" : ""}>Mid High (1000-2000 Hz)</option>
                    <option value="treble_low" ${band === "treble_low" ? "selected" : ""}>Treble Low (2000-4000 Hz)</option>
                    <option value="treble_mid" ${band === "treble_mid" ? "selected" : ""}>Treble Mid (4000-8000 Hz)</option>
                    <option value="treble_high" ${band === "treble_high" ? "selected" : ""}>Treble High (8000-12000 Hz)</option>
                    <option value="bass" ${band === "bass" ? "selected" : ""}>Bass (combined)</option>
                    <option value="mid" ${band === "mid" ? "selected" : ""}>Mid (combined)</option>
                    <option value="treble" ${band === "treble" ? "selected" : ""}>Treble (combined)</option>
                    <option value="energy" ${band === "energy" ? "selected" : ""}>Energy</option>
                    <option value="beat" ${band === "beat" ? "selected" : ""}>Beat</option>
                    <option value="tempo_bpm" ${band === "tempo_bpm" ? "selected" : ""}>Tempo (BPM)</option>
                  </select>
                  <button type="button" class="btn-icon band-add" title="${t("fx_audio_add_band") || "Add band"}" ${assignment.engine === "wled" ? "disabled" : ""}>+</button>
                  ${idx > 0 ? '<button type="button" class="btn-icon band-remove" data-idx="' + idx + '" title="' + (t("fx_audio_remove_band") || "Remove band").replace(/"/g, "&quot;") + '">×</button>' : ""}
                </div>
              `).join("");
            })()}
          </div>
          <small class="muted" style="display: block; margin-top: 0.25rem;">${assignment.engine === "wled" ? (t("fx_audio_only_ledfx") || "Only for LEDFx effects") : (t("fx_audio_bands_hint") || "Select frequency bands or metrics. Empty = use reactive mode")}</small>
        </label>
        ` : ''}
      </div>
    </details>
    <details class="fx-section-advanced" ${state.fxFormAccordionState.advanced ? "open" : ""}>
      <summary class="fx-section-header">${t("fx_advanced_title") || "Advanced"}</summary>
      <div style="margin-top: 1rem;">
        <details class="fx-subsection" open>
          <summary class="fx-subsection-header">${t("fx_audio_sensitivity_title") || "Audio Sensitivity"}</summary>
          <div class="form-grid" style="margin-top: 0.75rem;">
            <label>${t("fx_sens_bass_label") || "Bass Frequency Sensitivity"}
              <input type="number" id="devFxSensBass" step="0.05" min="0" max="5" value="${assignment.band_gain_low ?? 1}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_sens_bass_desc") || "How much the effect responds to bass frequencies (low sounds). 1.0 = normal, >1 = more sensitive, <1 = less sensitive"}</small>
            </label>
            <label>${t("fx_sens_mids_label") || "Mid Frequency Sensitivity"}
              <input type="number" id="devFxSensMids" step="0.05" min="0" max="5" value="${assignment.band_gain_mid ?? 1}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_sens_mids_desc") || "How much the effect responds to mid frequencies (vocals, instruments). 1.0 = normal, >1 = more sensitive, <1 = less sensitive"}</small>
            </label>
            <label>${t("fx_sens_treble_label") || "Treble Frequency Sensitivity"}
              <input type="number" id="devFxSensTreble" step="0.05" min="0" max="5" value="${assignment.band_gain_high ?? 1}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_sens_treble_desc") || "How much the effect responds to treble frequencies (high sounds, cymbals). 1.0 = normal, >1 = more sensitive, <1 = less sensitive"}</small>
            </label>
            <label>${t("fx_amplitude_label") || "Overall Audio Volume Scaling"}
              <input type="number" id="devFxAmplitude" step="0.05" min="0" max="5" value="${assignment.amplitude_scale ?? 1}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_amplitude_desc") || "Scale all audio input levels (0-5, 1.0 = normal, >1 = louder response, <1 = quieter response)"}</small>
            </label>
            <label>${t("fx_brightness_compress_label") || "Brightness Dynamic Range Compression"}
              <input type="number" id="devFxBrightnessCompress" step="0.05" min="0" max="5" value="${assignment.brightness_compress ?? 0}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_brightness_compress_desc") || "Reduce brightness variation (0 = full range, higher = less variation, smoother)"}</small>
            </label>
            <label class="switch">
              <input type="checkbox" id="devFxBeatResponse" ${assignment.beat_response ? "checked" : ""}>
              <span></span> ${t("fx_beat_response_label") || "Enhanced Beat Detection"}
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_beat_response_desc") || "Make the effect react more strongly to detected beats in the music"}</small>
            </label>
            <label>${t("fx_attack_label") || "Response Speed (Attack Time)"}
              <input type="number" id="devFxAttack" min="0" max="2000" value="${assignment.attack_ms ?? 25}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_attack_desc") || "How quickly the effect responds when audio increases (0-2000ms, lower = faster response, higher = slower/smoother)"}</small>
            </label>
            <label>${t("fx_release_label") || "Fade Speed (Release Time)"}
              <input type="number" id="devFxRelease" min="0" max="3000" value="${assignment.release_ms ?? 120}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_release_desc") || "How quickly the effect fades when audio decreases (0-3000ms, lower = faster fade, higher = slower/smoother fade)"}</small>
            </label>
            <label class="switch">
              <input type="checkbox" id="devFxBeatShuffle" ${assignment.beat_shuffle ? "checked" : ""}>
              <span></span> ${t("fx_beat_shuffle_label") || "Randomize Beat Timing"}
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_beat_shuffle_desc") || "Add randomness to beat timing for more natural variation"}</small>
            </label>
          </div>
        </details>
        <details class="fx-subsection">
          <summary class="fx-subsection-header">${t("fx_rendering_title") || "Rendering"}</summary>
          <div class="form-grid" style="margin-top: 0.75rem;">
            <label>${t("fx_blend_mode_label") || "Effect Blending Mode"}
              <select id="devFxBlendMode">
                <option value="normal" ${assignment.blend_mode === "normal" ? "selected" : ""}>Normal (Replace)</option>
                <option value="add" ${assignment.blend_mode === "add" ? "selected" : ""}>Add (Brighten)</option>
                <option value="screen" ${assignment.blend_mode === "screen" ? "selected" : ""}>Screen (Lighten)</option>
                <option value="multiply" ${assignment.blend_mode === "multiply" ? "selected" : ""}>Multiply (Darken)</option>
              </select>
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_blend_mode_desc") || "How this effect combines with other effects: Normal = replace, Add = make brighter, Screen = make lighter, Multiply = make darker"}</small>
            </label>
            <label>${t("fx_layers_label") || "Effect Layers (Multiplicity)"}
              <input type="number" id="devFxLayers" min="1" max="8" value="${assignment.layers ?? 1}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_layers_desc") || "Render multiple copies of the effect simultaneously for richer visuals (1-8 layers)"}</small>
            </label>
            <label>${t("fx_gamma_color_label") || "Color Gamma Correction"}
              <input type="number" id="devFxGammaColor" step="0.1" min="0.5" max="4" value="${assignment.gamma_color ?? 2.2}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_gamma_color_desc") || "Adjust color accuracy for LED strips (0.5-4.0, 2.2 = standard, lower = more saturated, higher = less saturated)"}</small>
            </label>
            <label>${t("fx_gamma_brightness_label") || "Brightness Gamma Correction"}
              <input type="number" id="devFxGammaBrightness" step="0.1" min="0.5" max="4" value="${assignment.gamma_brightness ?? 2.2}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_gamma_brightness_desc") || "Adjust brightness perception for human eye (0.5-4.0, 2.2 = standard, lower = brighter appearance, higher = darker appearance)"}</small>
            </label>
          </div>
        </details>
        <details class="fx-subsection">
          <summary class="fx-subsection-header">${t("fx_brightness_title") || "Brightness & Overrides"}</summary>
          <div class="form-grid" style="margin-top: 0.75rem;">
            <label>${t("fx_brightness_override_label") || "Per-Effect Brightness Override"}
              <input type="number" id="devFxBrightnessOverride" min="0" max="255" value="${assignment.brightness_override ?? 0}">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_brightness_override_desc") || "Override the global brightness setting for this specific effect (0-255, 0 = use global brightness, 1-255 = custom brightness)"}</small>
            </label>
          </div>
        </details>
        <details class="fx-subsection">
          <summary class="fx-subsection-header">${t("fx_scene_title") || "Scene & Scheduling"}</summary>
          <div class="form-grid" style="margin-top: 0.75rem;">
            <label>${t("fx_scene_preset_label") || "Scene Preset Name"}
              <input id="devFxScenePreset" value="${assignment.scene_preset || ''}" placeholder="scene-name">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_scene_preset_desc") || "Name of scene preset to activate when this effect is selected"}</small>
            </label>
            <label>${t("fx_scene_schedule_label") || "Scene Schedule Pattern"}
              <input id="devFxSceneSchedule" value="${assignment.scene_schedule || ''}" placeholder="e.g. 09:00-17:00">
              <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_scene_schedule_desc") || "Time-based schedule pattern for automatic scene activation (e.g. '09:00-17:00' for 9am-5pm)"}</small>
            </label>
          </div>
        </details>
      </div>
    </details>
    <details class="fx-section-category" ${state.fxFormAccordionState.audioAdvanced ? "open" : ""}>
      <summary class="fx-section-header">${t("fx_audio_advanced_title") || "Audio Processing"}</summary>
      <div class="form-grid" style="margin-top: 1rem;">
        <label class="switch">
          <input type="checkbox" id="devAudioStereoSplit" ${audio.stereo_split ? "checked" : ""}>
          <span></span> ${t("fx_stereo_split_label") || "Stereo Split (Left/Right Separation)"}
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_stereo_split_desc") || "Split audio: left channel controls first half of LEDs, right channel controls second half"}</small>
        </label>
        <label>${t("fx_gain_left_label") || "Left Channel Audio Gain"}
          <input type="number" step="0.05" id="devAudioGainL" value="${audio.gain_left ?? 1}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_gain_l_desc") || "Volume multiplier for left audio channel (0-5, 1.0 = normal, >1 = louder, <1 = quieter)"}</small>
        </label>
        <label>${t("fx_gain_right_label") || "Right Channel Audio Gain"}
          <input type="number" step="0.05" id="devAudioGainR" value="${audio.gain_right ?? 1}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_gain_r_desc") || "Volume multiplier for right audio channel (0-5, 1.0 = normal, >1 = louder, <1 = quieter)"}</small>
        </label>
        <label>${t("fx_sensitivity_label") || "Overall Audio Sensitivity"}
          <input type="number" min="0" max="2" step="0.01" id="devAudioSensitivity" value="${audio.sensitivity ?? 0.85}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_sensitivity_desc") || "How sensitive the effect is to audio changes (0-2, 0.85 = default, higher = more reactive, lower = less reactive)"}</small>
        </label>
        <label>${t("fx_threshold_label") || "Audio Activation Threshold"}
          <input type="number" min="0" max="1" step="0.01" id="devAudioThreshold" value="${audio.threshold ?? 0.05}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_threshold_desc") || "Minimum audio level needed to activate the effect (0-1, 0.05 = default, higher = needs louder audio, lower = reacts to quieter sounds)"}</small>
        </label>
        <label>${t("fx_smoothing_label") || "Audio Smoothing / Anti-Jitter"}
          <input type="number" min="0" max="1" step="0.01" id="devAudioSmoothing" value="${audio.smoothing ?? 0.65}">
          <small class="muted" style="display: block; margin-top: 0.25rem;">${t("fx_smoothing_desc") || "Smooth out rapid audio changes to reduce flickering (0-1, 0.65 = default, higher = smoother but slower response, lower = faster but more jittery)"}</small>
        </label>
      </div>
    </details>
  `;

  // Update effect catalog for the current engine BEFORE setting up event listeners
  renderEffectCatalog(assignment.engine || "wled");
  
  // Load effects asynchronously and populate the select
  renderEffectOptions(assignment.engine || "wled", assignment.effect || "").then(options => {
    const select = qs("devFxEffect");
    if (select) {
      select.innerHTML = options;
    }
    // Update description
    const meta = findEffectMeta(assignment.effect);
    const desc = qs("fxEffectDescription");
    if (desc && meta) {
      desc.textContent = meta.desc || "";
    }
  }).catch(err => {
    console.error("Error loading effects:", err);
    const select = qs("devFxEffect");
    if (select) {
      select.innerHTML = `<option value="${assignment.effect || ""}">${assignment.effect || "Error loading effects"}</option>`;
    }
  });
  
  // Restore accordion state after render - use requestAnimationFrame to ensure DOM is ready
  requestAnimationFrame(() => {
    // Restore accordion state after render
    const advancedDetailsAfter = target.querySelector('.fx-section-advanced');
    if (advancedDetailsAfter && state.fxFormAccordionState.advanced) {
      advancedDetailsAfter.setAttribute('open', '');
    }
    const audioProcessingAfter = Array.from(target.querySelectorAll('.fx-section-category')).find(el => 
      el.querySelector('summary')?.textContent?.includes('Audio Processing') ||
      el.querySelector('summary')?.textContent?.includes('Audio Advanced')
    );
    if (audioProcessingAfter && state.fxFormAccordionState.audioAdvanced) {
      audioProcessingAfter.setAttribute('open', '');
    }
  });

  qs("devFxEngine")?.addEventListener("change", async (ev) => {
    assignment.engine = ev.target.value || assignment.engine;
    // Disable audio reactivity for WLED effects
    if (assignment.engine === "wled") {
      assignment.audio_link = false;
    }
    // Update effect catalog for the selected engine
    renderEffectCatalog(assignment.engine);
    // Update effect select dropdown
    const effectSelect = qs("devFxEffect");
    if (effectSelect) {
      effectSelect.innerHTML = await renderEffectOptions(assignment.engine, assignment.effect || "");
    }
    // Re-render form to update disabled states (only if not WLED - WLED uses different container)
    if (!isWled) {
      renderEffectDetailForm(qs("fxDetailForm"), null, assignment, false);
    }
    saveFn();
  });
  qs("devFxEffect")?.addEventListener("change", async (ev) => {
    const effectName = ev.target.value || "";
    assignment.effect = effectName;
    
    // For WLED devices, ensure binding.effect is properly structured
    if (isWled && wledBinding) {
      // Ensure binding.effect is an object
      if (!wledBinding.effect || typeof wledBinding.effect !== "object") {
        wledBinding.effect = {};
      }
      // Set the effect name
      wledBinding.effect.effect = effectName;
      // Ensure engine is set
      if (!wledBinding.effect.engine) {
        wledBinding.effect.engine = "wled";
      }
      
      // Apply effect-specific defaults (colors, brightness, etc.)
      if (effectName) {
        applyEffectDefaults(wledBinding.effect, effectName, "wled", true);
      }
      
      // Ensure default values are set (fallback if applyEffectDefaults didn't set them)
      // Brightness must be > 0 for visibility
      if (typeof wledBinding.effect.brightness !== "number" || wledBinding.effect.brightness <= 0) {
        wledBinding.effect.brightness = 255;
      }
      if (typeof wledBinding.effect.intensity !== "number" || wledBinding.effect.intensity <= 0) {
        wledBinding.effect.intensity = 128;
      }
      if (typeof wledBinding.effect.speed !== "number" || wledBinding.effect.speed <= 0) {
        wledBinding.effect.speed = 128;
      }
      // Ensure colors are set and not black
      if (!wledBinding.effect.color1 || wledBinding.effect.color1 === "#000000" || wledBinding.effect.color1 === "000000") {
        wledBinding.effect.color1 = "#ffffff";
      }
      if (!wledBinding.effect.color2 || wledBinding.effect.color2 === "#000000" || wledBinding.effect.color2 === "000000") {
        wledBinding.effect.color2 = "#ff6600";
      }
      if (!wledBinding.effect.color3 || wledBinding.effect.color3 === "#000000" || wledBinding.effect.color3 === "000000") {
        wledBinding.effect.color3 = "#0066ff";
      }
      
      console.log("WLED effect configured:", {
        effect: wledBinding.effect.effect,
        brightness: wledBinding.effect.brightness,
        intensity: wledBinding.effect.intensity,
        speed: wledBinding.effect.speed,
        color1: wledBinding.effect.color1,
        color2: wledBinding.effect.color2,
        color3: wledBinding.effect.color3,
        enabled: wledBinding.enabled,
        ddp: wledBinding.ddp
      });
      
      // Enable the device and DDP when effect is selected
      wledBinding.enabled = true;
      wledBinding.ddp = true;
      
      // Update UI toggle button
      const toggleBtn = qs("btnWledToggle");
      if (toggleBtn) {
        toggleBtn.dataset.state = "playing";
        const playIcon = toggleBtn.querySelector(".icon-play");
        const pauseIcon = toggleBtn.querySelector(".icon-pause");
        if (playIcon) playIcon.style.display = "";
        if (pauseIcon) pauseIcon.style.display = "none";
      }
      
      // Update enabled checkbox
      const enabledCheckbox = qs("wledFxEnabled");
      if (enabledCheckbox) {
        enabledCheckbox.checked = true;
      }
      
      // Update DDP checkbox
      const ddpCheckbox = qs("wledFxDdp");
      if (ddpCheckbox) {
        ddpCheckbox.checked = true;
      }
      
      console.log("WLED effect selected:", effectName, "Binding:", wledBinding);
      await saveFn();
    } else if (!isWled && effectName) {
      // Update description
      const meta = findEffectMeta(effectName);
      const desc = qs("fxEffectDescription");
      if (desc && meta) {
        desc.textContent = meta.desc || "";
      }
      // For physical/virtual segments, enable the segment when effect is selected
      if (virtualSegment) {
        // Virtual segment
        virtualSegment.enabled = true;
      } else if (segment) {
        // Physical segment
        segment.enabled = true;
      }
      saveFn();
    } else {
      // Update description
      const meta = findEffectMeta(effectName);
      const desc = qs("fxEffectDescription");
      if (desc && meta) {
        desc.textContent = meta.desc || "";
      }
      saveFn();
    }
  });
  qs("devFxPreset")?.addEventListener("change", (ev) => {
    assignment.preset = ev.target.value || "";
    saveFn();
  });
  // Color pickers with text inputs
  const color1Picker = qs("devFxColor1");
  const color1Text = qs("devFxColor1Text");
  if (color1Picker && color1Text) {
    color1Picker.addEventListener("change", (ev) => {
      assignment.color1 = ev.target.value || assignment.color1;
      color1Text.value = assignment.color1;
      saveFn();
    });
    color1Text.addEventListener("change", (ev) => {
      const val = ev.target.value.trim();
      if (/^#[0-9A-Fa-f]{6}$/.test(val)) {
        assignment.color1 = val;
        color1Picker.value = val;
        saveFn();
      }
    });
  }
  
  const color2Picker = qs("devFxColor2");
  const color2Text = qs("devFxColor2Text");
  if (color2Picker && color2Text) {
    color2Picker.addEventListener("change", (ev) => {
      assignment.color2 = ev.target.value || assignment.color2;
      color2Text.value = assignment.color2;
      saveFn();
    });
    color2Text.addEventListener("change", (ev) => {
      const val = ev.target.value.trim();
      if (/^#[0-9A-Fa-f]{6}$/.test(val)) {
        assignment.color2 = val;
        color2Picker.value = val;
        saveFn();
      }
    });
  }
  
  const color3Picker = qs("devFxColor3");
  const color3Text = qs("devFxColor3Text");
  if (color3Picker && color3Text) {
    color3Picker.addEventListener("change", (ev) => {
      assignment.color3 = ev.target.value || assignment.color3;
      color3Text.value = assignment.color3;
      saveFn();
    });
    color3Text.addEventListener("change", (ev) => {
      const val = ev.target.value.trim();
      if (/^#[0-9A-Fa-f]{6}$/.test(val)) {
        assignment.color3 = val;
        color3Picker.value = val;
        saveFn();
      }
    });
  }
  
  // Gradient helper button
  qs("btnGradientHelper")?.addEventListener("click", () => {
    const examples = [
      "#ff0000-#00ff00-#0000ff",
      "#000000-#ffffff",
      "#ff6600-#0033ff",
      "#ff00ff-#00ffff-#ffff00",
    ];
    const gradientInput = qs("devFxGradient");
    if (gradientInput) {
      const current = gradientInput.value.trim();
      if (!current) {
        gradientInput.value = examples[0];
        assignment.gradient = examples[0];
        saveFn();
      } else {
        alert(`Gradient format:\n${examples.join("\n")}\n\nSeparate colors with: comma, dash, semicolon, or space`);
      }
    }
  });
  // Brightness slider with value display
  const brightnessSlider = qs("devFxBrightness");
  const brightnessValue = qs("devFxBrightnessValue");
  if (brightnessSlider && brightnessValue) {
    brightnessSlider.addEventListener("input", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.brightness = Math.max(0, Math.min(val, 255));
      brightnessValue.textContent = assignment.brightness;
    });
    brightnessSlider.addEventListener("change", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.brightness = Math.max(0, Math.min(val, 255));
      brightnessValue.textContent = assignment.brightness;
      saveFn();
    });
  }
  
  // Intensity slider with value display
  const intensitySlider = qs("devFxIntensity");
  const intensityValue = qs("devFxIntensityValue");
  if (intensitySlider && intensityValue) {
    intensitySlider.addEventListener("input", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.intensity = Math.max(0, Math.min(val, 255));
      intensityValue.textContent = assignment.intensity;
    });
    intensitySlider.addEventListener("change", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.intensity = Math.max(0, Math.min(val, 255));
      intensityValue.textContent = assignment.intensity;
      saveFn();
    });
  }
  
  // Speed slider with value display
  const speedSlider = qs("devFxSpeed");
  const speedValue = qs("devFxSpeedValue");
  if (speedSlider && speedValue) {
    speedSlider.addEventListener("input", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.speed = Math.max(0, Math.min(val, 255));
      speedValue.textContent = assignment.speed;
    });
    speedSlider.addEventListener("change", (ev) => {
      const val = parseInt(ev.target.value, 10);
      assignment.speed = Math.max(0, Math.min(val, 255));
      speedValue.textContent = assignment.speed;
      saveFn();
    });
  }
  qs("devFxAudioProfile")?.addEventListener("change", (ev) => {
    assignment.audio_profile = ev.target.value || "default";
    saveFn();
  });
  qs("devFxAudioMode")?.addEventListener("change", (ev) => {
    assignment.audio_mode = ev.target.value || "spectrum";
    saveFn();
  });
  qs("devFxAudioLink")?.addEventListener("change", (ev) => {
    // Only allow audio reactivity for LEDFx effects
    if (assignment.engine === "wled") {
      ev.target.checked = false;
      assignment.audio_link = false;
      notify("Audio reactivity is only available for LEDFx effects", "warn");
      return;
    }
    assignment.audio_link = ev.target.checked;
    // Re-render form to show/hide audio reactive section (only if not WLED)
    if (!isWled) {
      renderEffectDetailForm(qs("fxDetailForm"), segment, assignment, false);
    }
    saveFn();
  });
  // Handle band selection changes
  const bandsContainer = qs("devFxBandsContainer");
  if (bandsContainer) {
    // Delegate events for dynamic band selectors
    bandsContainer.addEventListener("change", (ev) => {
      if (ev.target.classList.contains("band-select")) {
        const idx = parseInt(ev.target.dataset.idx, 10);
        if (!assignment.selected_bands) assignment.selected_bands = [];
        if (ev.target.value) {
          assignment.selected_bands[idx] = ev.target.value;
        } else {
          assignment.selected_bands.splice(idx, 1);
        }
        // Remove empty entries
        assignment.selected_bands = assignment.selected_bands.filter(b => b);
        // Re-render if needed (only if not WLED)
        if (!isWled && assignment.selected_bands.length === 0) {
          renderEffectDetailForm(qs("effectDetailForm"), segment, assignment, false);
        }
        saveFn();
      }
    });
    
    bandsContainer.addEventListener("click", (ev) => {
      if (ev.target.classList.contains("band-add")) {
        if (!assignment.selected_bands) assignment.selected_bands = [];
        assignment.selected_bands.push("");
        if (!isWled) {
          renderEffectDetailForm(qs("effectDetailForm"), segment, assignment, false);
        }
      } else if (ev.target.classList.contains("band-remove")) {
        const idx = parseInt(ev.target.dataset.idx, 10);
        if (!assignment.selected_bands) assignment.selected_bands = [];
        assignment.selected_bands.splice(idx, 1);
        if (!isWled) {
          renderEffectDetailForm(qs("effectDetailForm"), segment, assignment, false);
        }
      }
    });
  }
  
  qs("devFxReactiveMode")?.addEventListener("change", (ev) => {
    assignment.reactive_mode = ev.target.value || "full";
    saveFn();
  });
  qs("devFxDirection")?.addEventListener("change", (ev) => {
    assignment.direction = ev.target.value || "forward";
    saveFn();
  });
  // Scatter slider with value display
  const scatterSlider = qs("devFxScatter");
  const scatterValue = qs("devFxScatterValue");
  if (scatterSlider && scatterValue) {
    scatterSlider.addEventListener("input", (ev) => {
      const val = parseFloat(ev.target.value);
      assignment.scatter = Number.isFinite(val) ? Math.max(0, Math.min(val, 100)) : assignment.scatter;
      scatterValue.textContent = Math.round(assignment.scatter);
    });
  }
  qs("devFxFadeIn")?.addEventListener("input", (ev) => {
    assignment.fade_in = Math.max(0, parseInt(ev.target.value, 10) || 0);
  });
  qs("devFxFadeIn")?.addEventListener("change", (ev) => {
    assignment.fade_in = Math.max(0, parseInt(ev.target.value, 10) || 0);
    saveFn();
  });
  qs("devFxFadeOut")?.addEventListener("input", (ev) => {
    assignment.fade_out = Math.max(0, parseInt(ev.target.value, 10) || 0);
  });
  qs("devFxFadeOut")?.addEventListener("change", (ev) => {
    assignment.fade_out = Math.max(0, parseInt(ev.target.value, 10) || 0);
    saveFn();
  });
  qs("devFxPalette")?.addEventListener("change", (ev) => {
    assignment.palette = ev.target.value || "";
    saveFn();
  });
  qs("devFxGradient")?.addEventListener("change", (ev) => {
    assignment.gradient = ev.target.value || "";
    saveFn();
  });
  qs("devFxBrightnessOverride")?.addEventListener("change", (ev) => {
    let val = parseInt(ev.target.value, 10);
    if (!Number.isFinite(val)) val = 0;
    assignment.brightness_override = Math.max(0, Math.min(val, 255));
    ev.target.value = assignment.brightness_override;
    saveFn();
  });
  qs("devFxGammaColor")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.gamma_color = Number.isFinite(val) ? val : assignment.gamma_color;
    saveFn();
  });
  qs("devFxGammaBrightness")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.gamma_brightness = Number.isFinite(val) ? val : assignment.gamma_brightness;
    saveFn();
  });
  qs("devFxBlendMode")?.addEventListener("change", (ev) => {
    assignment.blend_mode = ev.target.value || "normal";
    saveFn();
  });
  qs("devFxLayers")?.addEventListener("change", (ev) => {
    let val = parseInt(ev.target.value, 10);
    if (!Number.isFinite(val)) val = 1;
    assignment.layers = Math.max(1, Math.min(val, 8));
    ev.target.value = assignment.layers;
    saveFn();
  });
  qs("devFxSensBass")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.band_gain_low = Number.isFinite(val) ? val : assignment.band_gain_low;
    saveFn();
  });
  qs("devFxSensMids")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.band_gain_mid = Number.isFinite(val) ? val : assignment.band_gain_mid;
    saveFn();
  });
  qs("devFxSensTreble")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.band_gain_high = Number.isFinite(val) ? val : assignment.band_gain_high;
    saveFn();
  });
  qs("devFxAmplitude")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.amplitude_scale = Number.isFinite(val) ? val : assignment.amplitude_scale;
    saveFn();
  });
  qs("devFxBrightnessCompress")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    assignment.brightness_compress = Number.isFinite(val) ? val : assignment.brightness_compress;
    saveFn();
  });
  qs("devFxBeatResponse")?.addEventListener("change", (ev) => {
    assignment.beat_response = ev.target.checked;
    saveFn();
  });
  qs("devFxAttack")?.addEventListener("change", (ev) => {
    let val = parseInt(ev.target.value, 10);
    if (!Number.isFinite(val)) val = 0;
    assignment.attack_ms = Math.max(0, val);
    ev.target.value = assignment.attack_ms;
    saveFn();
  });
  qs("devFxRelease")?.addEventListener("change", (ev) => {
    let val = parseInt(ev.target.value, 10);
    if (!Number.isFinite(val)) val = 0;
    assignment.release_ms = Math.max(0, val);
    ev.target.value = assignment.release_ms;
    saveFn();
  });
  qs("devFxScenePreset")?.addEventListener("change", (ev) => {
    assignment.scene_preset = ev.target.value || "";
    saveFn();
  });
  qs("devFxSceneSchedule")?.addEventListener("change", (ev) => {
    assignment.scene_schedule = ev.target.value || "";
    saveFn();
  });
  qs("devFxBeatShuffle")?.addEventListener("change", (ev) => {
    assignment.beat_shuffle = ev.target.checked;
    saveFn();
  });
  qs("devAudioStereoSplit")?.addEventListener("change", (ev) => {
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.stereo_split = ev.target.checked;
    }
    saveFn();
  });
  qs("devAudioGainL")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.gain_left = Number.isFinite(val) ? val : segment.audio.gain_left;
    }
    saveFn();
  });
  qs("devAudioGainR")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.gain_right = Number.isFinite(val) ? val : segment.audio.gain_right;
    }
    saveFn();
  });
  qs("devAudioSensitivity")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.sensitivity = Number.isFinite(val) ? val : segment.audio.sensitivity;
    }
    saveFn();
  });
  qs("devAudioThreshold")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.threshold = Number.isFinite(val) ? val : segment.audio.threshold;
    }
    saveFn();
  });
  qs("devAudioSmoothing")?.addEventListener("change", (ev) => {
    const val = parseFloat(ev.target.value);
    if (segment) {
      segment.audio = segment.audio || defaultSegmentAudio();
      segment.audio.smoothing = Number.isFinite(val) ? val : segment.audio.smoothing;
    }
    saveFn();
  });
}

function renderDeviceDetail(entry) {
  if (!entry) {
    // Hide all detail cards
    const wledCard = qs("wledDeviceDetailCard");
    const segCard = qs("segmentDeviceDetailCard");
    const virtCard = qs("virtualDeviceDetailCard");
    if (wledCard) wledCard.style.display = "none";
    if (segCard) segCard.style.display = "none";
    if (virtCard) virtCard.style.display = "none";
    return;
  }
  
  if (entry.type === "wled") {
    renderWledDetail(entry.meta);
    return;
  }
  
  const led = ensureLedEngineConfig();
  if (!led) return;
  
  if (entry.type === "virtual") {
    const detail = qs("virtualDeviceDetailBody");
    const card = qs("virtualDeviceDetailCard");
    if (!detail || !card) return;
    card.style.display = "block";
    const seg = entry.meta;
    seg.effect = seg.effect || { effect: "Solid", brightness: 255, intensity: 128, speed: 128, audio_profile: "default", engine: "ledfx" };
    const fakeSegment = { name: seg.name, led_count: seg.members?.length || 0, audio: defaultSegmentAudio(), enabled: seg.enabled !== false };
    renderEffectDetailForm(detail, fakeSegment, seg.effect, false, null, seg);  // Pass seg as virtual segment context
  } else {
    const detail = qs("segmentDeviceDetailBody");
    const card = qs("segmentDeviceDetailCard");
    if (!detail || !card) return;
    card.style.display = "block";
    const assignment = ensureEffectAssignment(entry.meta.id || entry.meta.segment_id);
    const segment = (led.segments || []).find((s) => s.id === assignment.segment_id);
    renderEffectDetailForm(detail, segment, assignment, false);
  }
}

function renderDeviceExplorer() {
  const list = qs("deviceList");
  if (!list) return;
  const devices = buildUnifiedDevices();
  list.innerHTML = "";
  if (!devices.length) {
    const empty = document.createElement("div");
    empty.className = "placeholder muted";
    empty.textContent = t("wled_empty_hint") || "No WLED devices";
    list.appendChild(empty);
    renderDeviceDetail(null);
    return;
  }
  if (!state.selectedDeviceKey && devices.length) {
    state.selectedDeviceKey = devices[0].key;
  }
  devices.forEach((dev) => {
    const btn = document.createElement("button");
    btn.className = `device-list-item ${state.selectedDeviceKey === dev.key ? "active" : ""}`;
    const name = document.createElement("strong");
    name.textContent = dev.name;
    btn.appendChild(name);
    const meta = document.createElement("div");
    meta.className = "device-list-meta";
    if (dev.type === "wled" && (dev.meta.address || dev.meta.ip)) {
      const span = document.createElement("span");
      span.textContent = dev.meta.address || dev.meta.ip;
      meta.appendChild(span);
    } else {
      const span = document.createElement("span");
      span.textContent = dev.type.toUpperCase();
      meta.appendChild(span);
    }
    if (dev.meta.version) {
      const span = document.createElement("span");
      span.textContent = dev.meta.version;
      meta.appendChild(span);
    }
    btn.appendChild(meta);
    const statusLabel =
      dev.type === "wled"
        ? dev.online
          ? t("wled_status_online") || "online"
          : t("wled_status_offline") || "offline"
        : dev.online
        ? t("badge_enabled") || "enabled"
        : t("badge_disabled") || "disabled";
    btn.appendChild(createStatusPill(statusLabel, dev.online ? "ok" : "warn"));
    btn.addEventListener("click", () => {
      state.selectedDeviceKey = dev.key;
      renderDeviceExplorer();
      renderDeviceDetail(dev);
    });
    list.appendChild(btn);
  });
  const selected = devices.find((d) => d.key === state.selectedDeviceKey);
  renderDeviceDetail(selected || devices[0]);
}

function updateOverview() {
  const info = state.info || {};
  const cfg = state.config || {};

  const fwName = info.fw_name || info.fw || "LEDBrain";
  const fwVersion = info.fw_version || "";
  const fwIdf = info.idf ? ` (IDF ${info.idf})` : "";
  const fwLabel = [fwName, fwVersion].filter(Boolean).join(" ");
  const fw = `${fwLabel}${fwIdf}`.trim();
  const device = cfg.network?.hostname || "ledbrain";
  let ipText = `IP: ${info.ip || "-"}`;
  if (info.wifi_ap_active && !info.wifi_sta_connected) {
    ipText += " (WiFi AP)";
  } else if (info.wifi_sta_connected) {
    ipText += " (WiFi)";
  }
  setText("chipIp", ipText);
  setText("valDevice", device);
  setText("valFirmware", fw || "-");
  setText("valOta", formatOtaState(info.ota_state, info.ota_error));
  setText("valUptime", info.uptime_s ? formatUptime(info.uptime_s) : "-");
  const build = info.fw_build_date ? `${info.fw_build_date} ${info.fw_build_time || ""}`.trim() : "-";
  setText("valSystemFw", fwLabel || "-");
  setText("valSystemIdf", info.idf || "-");
  setText("valSystemBuild", build && build.trim() ? build : "-");
  setText("valCurrentVersion", fwVersion || fwLabel || "-");
  if (state.latestRelease) {
    setText("valAvailableVersion", state.latestRelease.tag || t("ota_no_version"));
  }
  syncBrightnessUi();


  qs("valIp").textContent = info.ip || "-";
  qs("valGateway").textContent = cfg.network?.gateway || "-";
  qs("valDns").textContent = cfg.network?.dns || "-";
  qs("valMqttServer").textContent =
    cfg.mqtt?.host && cfg.mqtt?.port
      ? `${cfg.mqtt.host}:${cfg.mqtt.port}`
      : cfg.mqtt?.host || "-";
  qs("valMqttUser").textContent = cfg.mqtt?.username || t("not_set");
  qs("valDdp").textContent =
    cfg.mqtt?.ddp_target && cfg.mqtt?.ddp_port
      ? `${cfg.mqtt.ddp_target}:${cfg.mqtt.ddp_port}`
      : cfg.mqtt?.ddp_target || "-";

  // Update diagnostic bar: network status
  updateNetworkStatus(info);

  // Update diagnostic bar: CPU temperature
  updateTemperatureIndicator(info);

  // Update diagnostic bar: CPU usage
  updateCpuUsageIndicator(info);
  
  const ledEnabled = info?.led_engine?.enabled ?? !!state.ledState.enabled;
  setBadge("badgeStatus", ledEnabled ? t("badge_running") : t("badge_stopped"), ledEnabled ? "ok" : "warn");
  if (cfg.network?.use_dhcp) {
    setBadge("badgeNetwork", t("badge_dhcp"), "ok");
  } else {
    setBadge("badgeNetwork", t("badge_static"), "warn");
  }
  if (cfg.mqtt?.configured) {
    setBadge("badgeMqtt", t("badge_enabled"), "ok");
  } else {
    setBadge("badgeMqtt", t("badge_disabled"), "warn");
  }
}

function updateNetworkStatus(info) {
  const networkStatus = qs("networkStatus");
  if (!networkStatus) return;
  
  // Determine network type (ethernet or wifi)
  // Ethernet takes priority if both are connected
  const ethConnected = info.eth_connected ?? false;
  const isWifi = (info.wifi_sta_connected || info.wifi_ap_active) && !ethConnected;
  networkStatus.setAttribute("data-type", isWifi ? "wifi" : "ethernet");
  
  // Update WiFi signal strength if WiFi is active
  if (isWifi) {
    const signalBars = networkStatus.querySelector(".wifi-signal-bars");
    if (signalBars) {
      // WiFi signal strength (0-3, default to 2 if not provided)
      const strength = info.wifi_rssi ? Math.min(3, Math.max(0, Math.floor((info.wifi_rssi + 100) / 25))) : 2;
      signalBars.setAttribute("data-strength", strength);
    }
  } else {
    // Hide WiFi signal bars for Ethernet
    const signalBars = networkStatus.querySelector(".wifi-signal-bars");
    if (signalBars) {
      signalBars.setAttribute("data-strength", "0");
    }
  }
  
  // Update network traffic (if available in info)
  const trafficTx = qs("trafficTx");
  const trafficRx = qs("trafficRx");
  if (trafficTx) {
    // Try multiple possible field names for TX rate
    const txRate = info.network_tx_rate ?? info.tx_rate ?? info.tx_bytes_per_sec ?? info.udp_tx_rate ?? 0;
    if (txRate > 0) {
      trafficTx.textContent = formatTrafficRate(txRate);
    } else {
      trafficTx.textContent = "0 KB/s";
    }
  }
  if (trafficRx) {
    // Try multiple possible field names for RX rate
    const rxRate = info.network_rx_rate ?? info.rx_rate ?? info.rx_bytes_per_sec ?? info.udp_rx_rate ?? 0;
    if (rxRate > 0) {
      trafficRx.textContent = formatTrafficRate(rxRate);
    } else {
      trafficRx.textContent = "0 KB/s";
    }
  }
}

function formatTrafficRate(bytesPerSecond) {
  if (!Number.isFinite(bytesPerSecond) || bytesPerSecond < 0) return "0 KB/s";
  if (bytesPerSecond < 1024) return `${Math.round(bytesPerSecond)} B/s`;
  if (bytesPerSecond < 1024 * 1024) return `${(bytesPerSecond / 1024).toFixed(1)} KB/s`;
  return `${(bytesPerSecond / (1024 * 1024)).toFixed(2)} MB/s`;
}

function updateCpuUsageIndicator(info) {
  const cpuIndicator = qs("cpuUsageIndicator");
  const core0Value = qs("cpuCore0Value");
  const core1Value = qs("cpuCore1Value");

  if (!cpuIndicator || !core0Value || !core1Value) {
    // Elements not found - might be called before DOM is ready
    return;
  }

  const cpuUsage = info?.cpu_usage;
  if (!cpuUsage || cpuUsage.core0 === undefined || cpuUsage.core1 === undefined) {
    core0Value.textContent = "--%";
    core1Value.textContent = "--%";
    cpuIndicator.removeAttribute("data-usage-level");
    return;
  }

  const core0 = cpuUsage.core0;
  const core1 = cpuUsage.core1;

  // Check if values are valid (not undefined, not null, >= 0)
  const core0Valid = core0 !== undefined && core0 !== null && core0 >= 0 && Number.isFinite(core0);
  const core1Valid = core1 !== undefined && core1 !== null && core1 >= 0 && Number.isFinite(core1);
  
  if (core0Valid && core1Valid) {
    core0Value.textContent = `${Math.round(core0)}%`;
    core1Value.textContent = `${Math.round(core1)}%`;
    
    // Update color based on usage level
    cpuIndicator.setAttribute("data-usage-level", 
      core0 > 80 || core1 > 80 ? "high" : 
      core0 > 50 || core1 > 50 ? "medium" : "low");
  } else {
    core0Value.textContent = "--%";
    core1Value.textContent = "--%";
    cpuIndicator.removeAttribute("data-usage-level");
  }
}

function updateTemperatureIndicator(info) {
  const tempIndicator = qs("temperatureIndicator");
  const tempValue = qs("temperatureValue");

  if (!tempIndicator || !tempValue) {
    // Elements not found - might be called before DOM is ready
    return;
  }

  const cpuTemp = info?.cpu_temp_celsius;
  
  // Check if temperature is unavailable (undefined, null, or -1)
  if (cpuTemp === undefined || cpuTemp === null || cpuTemp < 0 || !Number.isFinite(cpuTemp)) {
    tempValue.textContent = "--°C";
    tempIndicator.setAttribute("data-temp", "0");
    tempIndicator.removeAttribute("data-temp-level");
    return;
  }
  
  // Update temperature value
  tempValue.textContent = `${Math.round(cpuTemp)}°C`;
  tempIndicator.setAttribute("data-temp", cpuTemp.toFixed(1));
  
  // Determine color level based on strict test thresholds
  let tempLevel = "green";
  if (cpuTemp >= 100) {
    tempLevel = "red";      // > 100°C
  } else if (cpuTemp >= 85) {
    tempLevel = "orange";    // 85°C - 100°C
  } else if (cpuTemp >= 66) {
    tempLevel = "yellow";   // 66°C - 85°C
  } else {
    tempLevel = "green";    // < 66°C
  }
  
  tempIndicator.setAttribute("data-temp-level", tempLevel);
}
function updateDhcpUi() {
  const dhcpOn = qs("dhcp").checked;
  const label = qs("dhcpLabel");
  if (label) {
    label.textContent = dhcpOn ? t("network_dhcp_on") || "DHCP (automatic)" : t("network_dhcp_off") || "Static IP (manual)";
  }
  const disabled = dhcpOn;
  ["ip", "mask", "gw", "dns"].forEach((id) => {
    const el = qs(id);
    if (el) el.disabled = disabled;
  });
}

function updateMqttUi() {
  const enabled = qs("mqttEnabled").checked;
  ["mqttHost", "mqttPort", "mqttUser", "mqttPass", "ddpTarget", "ddpPort"].forEach((id) => {
    const el = qs(id);
    if (el) el.disabled = !enabled;
  });
}

function renderWledDevices() {
  const tbody = qs("wledTableBody");
  if (!tbody) return;
  tbody.innerHTML = "";
  const devices = mergeWledDevices();
  const fallback = t("not_set");
  if (!devices.length) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 7;
    cell.textContent = t("wled_empty");
    row.appendChild(cell);
    tbody.appendChild(row);
    return;
  }
  devices.forEach((dev) => {
    const row = document.createElement("tr");

    const nameCell = document.createElement("td");
    const title = document.createElement("div");
    title.className = "cell-title";
    title.textContent = dev.name || dev.id || fallback;
    nameCell.appendChild(title);
    if (dev.version) {
      const sub = document.createElement("div");
      sub.className = "muted small";
      sub.textContent = dev.version;
      nameCell.appendChild(sub);
    }

    const idCell = document.createElement("td");
    idCell.textContent = dev.id || fallback;
    const endpoint = dev.ip || dev.address;
    if (endpoint) {
      const sub = document.createElement("div");
      sub.className = "muted small";
      sub.textContent = endpoint;
      idCell.appendChild(sub);
    }

    const ledsCell = document.createElement("td");
    ledsCell.textContent = dev.leds || fallback;

    const segCell = document.createElement("td");
    segCell.textContent = dev.segments || fallback;

    const statusCell = document.createElement("td");
    const statusText = dev.online ? t("wled_status_online") : t("wled_status_offline");
    statusCell.appendChild(createStatusPill(statusText, dev.online ? "ok" : "warn"));

    const lastSeenCell = document.createElement("td");
    const age = dev.age_ms || 0;
    lastSeenCell.textContent = Number.isFinite(age) && age > 0 ? formatAge(age) : t("not_set");

    const actionsCell = document.createElement("td");
    // Only show delete button for manually added devices (not auto-discovered)
    if (dev.manual !== false) {
      const deleteBtn = document.createElement("button");
      deleteBtn.className = "ghost small danger";
      deleteBtn.textContent = "Delete";
      deleteBtn.title = "Remove device from configuration";
      deleteBtn.addEventListener("click", async (e) => {
        e.stopPropagation();
        if (!confirm(`Remove device "${dev.name || dev.id || dev.address}"?`)) {
          return;
        }
        try {
          const payload = {};
          if (dev.id) payload.id = dev.id;
          if (dev.address) payload.address = dev.address;
          const res = await fetch("/api/wled/delete", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
          });
          if (res.ok) {
            notify("Device removed", "ok");
            await loadConfig();
            renderWledDevices();
            renderLightsDashboard();
            renderDeviceExplorer();
          } else {
            const text = await res.text();
            notify(`Failed to remove device: ${text}`, "error");
          }
        } catch (err) {
          console.error(err);
          notify("Failed to remove device", "error");
        }
      });
      actionsCell.appendChild(deleteBtn);
    }

    [nameCell, idCell, ledsCell, segCell, statusCell, lastSeenCell, actionsCell].forEach((cell) => row.appendChild(cell));
    tbody.appendChild(row);
  });
}

async function refreshInfo(showToast = false) {
  try {
    const info = await (await fetch("/api/info")).json();
    // Preserve audio running state if it was manually set (don't let refreshInfo override user's choice)
    const previousAudioRunning = state.info?.led_engine?.audio?.running;
    state.info = info;
    // If audio was manually disabled, don't let refreshInfo re-enable it
    if (previousAudioRunning === false && info?.led_engine?.audio?.running === true) {
      // User had disabled audio, but backend says it's running - this shouldn't happen if endpoint works
      // But we'll respect the backend state for now (the endpoint should handle this)
      console.log("Audio state mismatch: user disabled but backend reports running");
    }
    if (info?.led_engine && typeof info.led_engine.brightness === "number") {
      state.globalBrightness = clamp(info.led_engine.brightness, 1, MAX_BRIGHTNESS);
      syncBrightnessUi();
    }
    state.lastInfoRefresh = Date.now();
    if (info?.led_engine && typeof info.led_engine.enabled === "boolean") {
      state.ledState.enabled = info.led_engine.enabled;
      updateControlButtons();
    }
    
    // Update WiFi status in network tab
    const wifiStatus = qs("wifiStatus");
    if (wifiStatus) {
      if (info.wifi_ap_active && !info.wifi_sta_connected) {
        const apIp = info.ip || "192.168.4.1";
        wifiStatus.innerHTML = `<div style="color: var(--warn);">📡 <strong>WiFi AP Mode</strong><br>SSID: <strong>${info.hostname || "LEDBrain"}-Setup</strong><br>Password: <strong>ledbrain123</strong><br>IP: <strong>${apIp}</strong></div>`;
      } else if (info.wifi_sta_connected) {
        wifiStatus.innerHTML = `<div style="color: var(--accent);">✓ <strong>WiFi Connected</strong><br>IP: <strong>${info.ip || "-"}</strong></div>`;
      } else {
        wifiStatus.innerHTML = `<div class="muted">WiFi not configured. Connect via Ethernet or scan for networks.</div>`;
      }
    }
    
    updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    startUptimeTicker();
    if (showToast) {
      notify(t("toast_refreshed"), "ok");
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_info_failed"), "error");
  }
}

async function loadWledDevices(showToast = false) {
  try {
    const res = await fetch("/api/wled/list");
    const data = await res.json();
    state.wledDevices = data.devices || [];
    console.log("wled devices", state.wledDevices.length, state.wledDevices);
    renderWledDevices();
    renderLightsDashboard();
    renderDeviceExplorer();
    if (showToast) {
      const count = Array.isArray(state.wledDevices) ? state.wledDevices.length : 0;
      notify(`${t("toast_wled_updated") || "WLED updated"} (${count})`, "ok");
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_load_failed"), "error");
  }
}

function defaultSegmentAudio() {
  return {
    stereo_split: false,
    gain_left: 1,
    gain_right: 1,
    sensitivity: 0.85,
    threshold: 0.05,
    smoothing: 0.65,
    per_led_sensitivity: [],
  };
}

function defaultAudioConfig() {
  return {
    source: "none",
    sample_rate: 48000,
    fft_size: 1024,
    frame_ms: 20,
    stereo: true,
    sensitivity: 1,
    snapcast: {
      enabled: false,
      host: "snapcast.local",
      port: 1704,
      latency_ms: 60,
      prefer_udp: true,
    },
  };
}

function ensureWledEffectsConfig() {
  if (!state.config) {
    state.config = {};
  }
  if (!state.config.wled_effects) {
    state.config.wled_effects = { target_fps: 60, bindings: [] };
  }
  const fx = state.config.wled_effects;
  if (!Array.isArray(fx.bindings)) {
    fx.bindings = [];
  }
  if (!fx.target_fps || fx.target_fps < 1) {
    fx.target_fps = 60;
  }
  return fx;
}

function getWledBinding(deviceId) {
  const fx = ensureWledEffectsConfig();
  let binding = fx.bindings.find((b) => b.device_id === deviceId);
  if (!binding) {
    binding = {
      device_id: deviceId,
      segment_index: 0,
      enabled: true,
      ddp: true,  // DDP must be enabled for UDP streaming
      audio_channel: "stereo",
      effect: {
        engine: "ledfx",  // Default to LEDFx for audio-reactive effects
        effect: "Energy Flow",
        brightness: 255,
        intensity: 128,
        speed: 128,
        color1: "#ffffff",
        color2: "#ff6600",
        color3: "#0033ff",
        preset: "",
        audio_link: true,  // Enable audio reactivity by default
        audio_profile: "ledfx_energy",
      },
    };
    fx.bindings.push(binding);
  }
  // Ensure ddp is true by default (required for UDP streaming)
  if (binding.ddp === undefined || binding.ddp === null) {
    binding.ddp = true;
  }
  // Ensure enabled is true by default
  if (binding.enabled === undefined || binding.enabled === null) {
    binding.enabled = true;
  }
  if (!binding.effect) {
    binding.effect = { effect: "Solid", brightness: 255, intensity: 128, speed: 128, audio_profile: "default" };
  }
  // Ensure effect name is set (required for rendering)
  if (!binding.effect.effect || binding.effect.effect.trim() === "") {
    binding.effect.effect = "Solid";
  }
  // Ensure engine is set (default to wled for WLED effects)
  if (!binding.effect.engine) {
    binding.effect.engine = "wled";
  }
  if (!binding.audio_channel) {
    binding.audio_channel = "stereo";
  }
  return binding;
}

function computeAutoCurrentLimit(led) {
  if (!led) return 0;
  const voltage = Number(led.power_supply_voltage || 0);
  const watts = Number(led.power_supply_watts || 0);
  if (!Number.isFinite(voltage) || !Number.isFinite(watts) || voltage <= 0 || watts <= 0) {
    return 0;
  }
  return Math.max(0, Math.round((watts / voltage) * 1000));
}

function ensureLedEngineConfig() {
  if (!state.config) {
    state.config = {};
  }
  if (!state.config.led_engine) {
    state.config.led_engine = {
      driver: "esp_rmt",
      max_fps: 120,
      global_current_limit_ma: 6000,
      power_supply_voltage: 5,
      power_supply_watts: 0,
      auto_power_limit: false,
      parallel_outputs: 4,
      enable_dma: true,
      segments: [],
      audio: defaultAudioConfig(),
      effects: { default_engine: "ledfx", assignments: [] },
    };
  }
  const led = state.config.led_engine;
  led.segments = Array.isArray(led.segments) ? led.segments : [];
  led.segments.forEach((seg, idx) => {
    seg.id = seg.id || `segment-${idx + 1}`;
    seg.matrix = seg.matrix || { width: 0, height: 0, serpentine: true, vertical: false };
    seg.audio = seg.audio || defaultSegmentAudio();
    if (typeof seg.render_order !== "number" || Number.isNaN(seg.render_order)) {
      seg.render_order = idx;
    }
    seg.effect_source = seg.effect_source || "local";
  });
  led.audio = led.audio || defaultAudioConfig();
  led.audio.snapcast = led.audio.snapcast || defaultAudioConfig().snapcast;
  led.effects =
    led.effects ||
    {
      default_engine: "ledfx",
      assignments: [],
    };
  led.driver = led.driver || "esp_rmt";
  if (typeof led.max_fps !== "number" || Number.isNaN(led.max_fps)) {
    led.max_fps = 120;
  }
  if (typeof led.global_current_limit_ma !== "number" || Number.isNaN(led.global_current_limit_ma)) {
    led.global_current_limit_ma = 0;
  }
  if (typeof led.power_supply_voltage !== "number" || Number.isNaN(led.power_supply_voltage) || led.power_supply_voltage <= 0) {
    led.power_supply_voltage = 5;
  }
  if (typeof led.power_supply_watts !== "number" || Number.isNaN(led.power_supply_watts)) {
    led.power_supply_watts = 0;
  }
  if (led.power_supply_watts < 0) {
    led.power_supply_watts = 0;
  }
  if (typeof led.auto_power_limit !== "boolean") {
    led.auto_power_limit = false;
  }
  if (led.auto_power_limit) {
    const autoLimit = computeAutoCurrentLimit(led);
    if (autoLimit > 0) {
      led.global_current_limit_ma = autoLimit;
    }
  }
  if (typeof led.parallel_outputs !== "number" || Number.isNaN(led.parallel_outputs)) {
    led.parallel_outputs = 1;
  }
  if (typeof led.enable_dma !== "boolean") {
    led.enable_dma = true;
  }
  led.effects.assignments = Array.isArray(led.effects.assignments) ? led.effects.assignments : [];
  return led;
}

function ensureVirtualSegments() {
  if (!state.config) state.config = {};
  if (!Array.isArray(state.config.virtual_segments)) {
    state.config.virtual_segments = [];
  }
  state.config.virtual_segments.forEach((seg, idx) => {
    seg.id = seg.id || `vseg-${idx + 1}`;
    seg.name = seg.name || seg.id;
    seg.enabled = seg.enabled !== false;
    seg.members = Array.isArray(seg.members) ? seg.members : [];
    seg.members.forEach((m) => {
      m.type = m.type || "physical";
      m.id = m.id || "";
      m.segment_index = Number.isFinite(m.segment_index) ? m.segment_index : 0;
      m.start = Number.isFinite(m.start) ? m.start : 0;
      m.length = Number.isFinite(m.length) ? m.length : 0;
    });
    seg.effect =
      seg.effect ||
      {
        engine: "ledfx",
        effect: "Solid",
        brightness: 255,
        intensity: 128,
        speed: 128,
        audio_profile: "default",
        audio_link: false,
        audio_mode: "spectrum",
      };
  });
  return state.config.virtual_segments;
}

function createSegmentTemplate() {
  const led = ensureLedEngineConfig();
  const index = (led?.segments?.length || 0) + 1;
  return {
    id: `segment-${Date.now()}-${Math.floor(Math.random() * 1000)}`,
    name: `Segment ${index}`,
    start_index: 0,
    led_count: 60,
    gpio: -1,
    rmt_channel: Math.min(index - 1, 7),
    chipset: "ws2812b",
    color_order: "GRB",
    enabled: true,
    reverse: false,
    mirror: false,
    matrix_enabled: false,
    matrix: { width: 0, height: 0, serpentine: true, vertical: false },
    power_limit_ma: 0,
    render_order: index - 1,
    effect_source: "local",
    audio: defaultSegmentAudio(),
  };
}

function createVirtualSegmentTemplate() {
  const idx = (state.config?.virtual_segments?.length || 0) + 1;
  return {
    id: `vseg-${Date.now()}-${Math.floor(Math.random() * 1000)}`,
    name: `Virtual ${idx}`,
    enabled: true,
    members: [],
    effect: {
      engine: "ledfx",
      effect: "Energy Flow",  // Default to audio-reactive effect
      brightness: 255,
      intensity: 128,
      speed: 128,
      audio_profile: "ledfx_energy",
      audio_link: true,  // Enable audio reactivity by default
      audio_mode: "spectrum",
    },
  };
}

function ensureEffectAssignment(segmentId) {
  const led = ensureLedEngineConfig();
  if (!led) return null;
  let assignment = led.effects.assignments.find((a) => a.segment_id === segmentId);
  if (!assignment) {
    const defaultEffect = "Solid";
    const defaults = getEffectDefaults(defaultEffect, led.effects.default_engine || "wled");
    assignment = {
      segment_id: segmentId,
      engine: led.effects.default_engine || "wled",  // Default to WLED for physical segments (for visual consistency)
      effect: defaultEffect,  // Default to simple WLED effect, user can change to audio-reactive
      preset: "",
      audio_link: false,  // User can enable audio reactivity if needed
      audio_profile: "wled_reactive",
      brightness: defaults.brightness,
      intensity: defaults.intensity,
      speed: defaults.speed,
      audio_mode: "spectrum",
      direction: "forward",
      scatter: 0,
      fade_in: 0,
      fade_out: 0,
      color1: defaults.color1,
      color2: defaults.color2,
      color3: defaults.color3,
      palette: "",
      gradient: "",
      brightness_override: 0,
      gamma_color: 2.2,
      gamma_brightness: 2.2,
      blend_mode: "normal",
      layers: 1,
      reactive_mode: "full",
      band_gain_low: 1,
      band_gain_mid: 1,
      band_gain_high: 1,
      amplitude_scale: 1,
      brightness_compress: 0,
      beat_response: false,
      attack_ms: 25,
      release_ms: 120,
      scene_preset: "",
      scene_schedule: "",
      beat_shuffle: false,
      freq_min: 0,  // 0 = use reactive_mode
      freq_max: 0,  // 0 = use reactive_mode
      selected_bands: [],  // Selected frequency bands (empty = use reactive_mode)
    };
    led.effects.assignments.push(assignment);
  }
  assignment.brightness = typeof assignment.brightness === "number" ? assignment.brightness : 255;
  assignment.intensity = typeof assignment.intensity === "number" ? assignment.intensity : 128;
  assignment.speed = typeof assignment.speed === "number" ? assignment.speed : 128;
  assignment.audio_mode = assignment.audio_mode || "spectrum";
  assignment.direction = assignment.direction || "forward";
  assignment.scatter = typeof assignment.scatter === "number" ? assignment.scatter : 0;
  assignment.fade_in = typeof assignment.fade_in === "number" ? assignment.fade_in : 0;
  assignment.fade_out = typeof assignment.fade_out === "number" ? assignment.fade_out : 0;
  // Apply effect-specific defaults if effect is set and values are at generic defaults
  if (assignment.effect) {
    applyEffectDefaults(assignment, assignment.effect, assignment.engine || "wled", false);
  } else {
    // Fallback to generic defaults
    assignment.color1 = assignment.color1 || "#ffffff";
    assignment.color2 = assignment.color2 || "#996600";
    assignment.color3 = assignment.color3 || "#0033ff";
  }
  assignment.palette = assignment.palette || "";
  assignment.gradient = assignment.gradient || "";
  assignment.brightness_override =
    typeof assignment.brightness_override === "number" ? assignment.brightness_override : 0;
  assignment.gamma_color = Number.isFinite(assignment.gamma_color) ? assignment.gamma_color : 2.2;
  assignment.gamma_brightness = Number.isFinite(assignment.gamma_brightness)
    ? assignment.gamma_brightness
    : 2.2;
  assignment.blend_mode = assignment.blend_mode || "normal";
  assignment.layers = typeof assignment.layers === "number" ? assignment.layers : 1;
  assignment.reactive_mode = assignment.reactive_mode || "full";
  assignment.band_gain_low = Number.isFinite(assignment.band_gain_low) ? assignment.band_gain_low : 1;
  assignment.band_gain_mid = Number.isFinite(assignment.band_gain_mid) ? assignment.band_gain_mid : 1;
  assignment.band_gain_high = Number.isFinite(assignment.band_gain_high)
    ? assignment.band_gain_high
    : 1;
  assignment.amplitude_scale = Number.isFinite(assignment.amplitude_scale)
    ? assignment.amplitude_scale
    : 1;
  assignment.brightness_compress = Number.isFinite(assignment.brightness_compress)
    ? assignment.brightness_compress
    : 0;
  assignment.beat_response = Boolean(assignment.beat_response);
  assignment.attack_ms = Number.isFinite(assignment.attack_ms) ? assignment.attack_ms : 25;
  assignment.release_ms = Number.isFinite(assignment.release_ms) ? assignment.release_ms : 120;
  assignment.scene_preset = assignment.scene_preset || "";
  assignment.scene_schedule = assignment.scene_schedule || "";
  assignment.beat_shuffle = Boolean(assignment.beat_shuffle);
  assignment.freq_min = typeof assignment.freq_min === "number" && Number.isFinite(assignment.freq_min) ? Math.max(0, assignment.freq_min) : 0;
  assignment.freq_max = typeof assignment.freq_max === "number" && Number.isFinite(assignment.freq_max) ? Math.max(0, assignment.freq_max) : 0;
  return assignment;
}

function findEffectMeta(name) {
  if (!name) return null;
  const lookup = name.toLowerCase();
  
  // Check loaded effects cache
  for (const engine of ["wled", "ledfx"]) {
    if (effectsCache[engine]) {
      for (const group of effectsCache[engine]) {
        if (group.effects && Array.isArray(group.effects)) {
          for (const eff of group.effects) {
            if (eff.name && eff.name.toLowerCase() === lookup) {
              return eff;
            }
          }
        }
      }
    }
  }
  
  // Fallback to EFFECT_LIBRARY
  for (const group of EFFECT_LIBRARY) {
    for (const eff of group.effects) {
      if (eff.name && eff.name.toLowerCase() === lookup) {
        return eff;
      }
    }
  }
  return null;
}

// Cache for loaded effects
let effectsCache = {
  wled: null,
  ledfx: null
};

async function loadEffectsForEngine(engine) {
  // Return cached if available
  if (effectsCache[engine]) {
    return effectsCache[engine];
  }
  
  try {
    const filename = engine === "wled" ? "wled/effects.json" : "ledfx/effects.json";
    const response = await fetch(`/${filename}`);
    if (!response.ok) {
      console.warn(`Failed to load ${filename}, using fallback`);
      return null;
    }
    const data = await response.json();
    effectsCache[engine] = data;
    return data;
  } catch (err) {
    console.error(`Error loading effects for ${engine}:`, err);
    return null;
  }
}

async function renderEffectOptions(engine, current) {
  let effects = [];
  
  // Load effects from JSON file
  const effectsData = await loadEffectsForEngine(engine);
  
  if (effectsData && Array.isArray(effectsData)) {
    // Use effects from JSON file
    for (const group of effectsData) {
      if (group.effects && Array.isArray(group.effects)) {
        for (const eff of group.effects) {
          if (!effects.find(e => e.name === eff.name)) {
            effects.push({ name: eff.name, category: group.category });
          }
        }
      }
    }
  } else {
    // Fallback to EFFECT_LIBRARY if JSON files are not available
    if (engine === "wled") {
      for (const group of EFFECT_LIBRARY) {
        for (const eff of group.effects) {
          if (eff.engine === "wled" && !effects.find(e => e.name === eff.name)) {
            effects.push({ name: eff.name, category: group.category });
          }
        }
      }
    } else {
      for (const group of EFFECT_LIBRARY) {
        for (const eff of group.effects) {
          if (eff.engine === "ledfx" && !effects.find(e => e.name === eff.name)) {
            effects.push({ name: eff.name, category: group.category });
          }
        }
      }
    }
  }
  
  // Group by category
  const byCategory = {};
  for (const eff of effects) {
    if (!byCategory[eff.category]) {
      byCategory[eff.category] = [];
    }
    byCategory[eff.category].push(eff.name);
  }
  
  // Build options HTML
  let options = "";
  for (const [category, names] of Object.entries(byCategory)) {
    names.sort();
    options += `<optgroup label="${category}">`;
    for (const name of names) {
      options += `<option value="${name}" ${name === current ? "selected" : ""}>${name}</option>`;
    }
    options += `</optgroup>`;
  }
  return options;
}

function renderEffectPicker(current) {
  const select = qs("fxEffectPicker");
  if (!select) return;
  const assignment = ensureEffectAssignment(state.selectedFxSegment);
  const options = renderEffectOptions(assignment.engine || "wled", current);
  select.innerHTML = `<option value="">${t("pin_select")}</option>${options}`;
}

function renderLedConfig() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  renderHardwareForm(led);
  renderSegmentTable(led);
  renderEffectRows(led);
  renderVirtualSegments();
  populateAudioForm(led);
  const defEngine = qs("defaultEffectEngine");
  if (defEngine) {
    defEngine.value = led.effects.default_engine || "ledfx";
  }
}

function renderHardwareForm(led) {
  const driver = qs("ledDriver");
  if (!driver) return;
  driver.value = led.driver || "esp_rmt";
  const numericMap = [
    ["maxFps", led.max_fps ?? 120],
    ["parallelOutputs", led.parallel_outputs ?? 1],
  ];
  const currentLimitField = qs("currentLimit");
  if (currentLimitField) {
    currentLimitField.value = led.global_current_limit_ma ?? 0;
  }
  numericMap.forEach(([id, value]) => {
    const el = qs(id);
    if (el) {
      el.value = value;
    }
  });
  const floatMap = [
    ["psuVoltage", led.power_supply_voltage ?? 5],
    ["psuWatts", led.power_supply_watts ?? 0],
  ];
  floatMap.forEach(([id, value]) => {
    const el = qs(id);
    if (el) {
      el.value = value;
    }
  });
  const checkboxMap = [
    ["enableDma", led.enable_dma ?? true],
  ];
  checkboxMap.forEach(([id, checked]) => {
    const el = qs(id);
    if (el) {
      el.checked = Boolean(checked);
    }
  });
  const autoToggle = qs("autoPowerLimit");
  if (autoToggle) {
    autoToggle.checked = Boolean(led.auto_power_limit);
  }
  updatePowerSummary();
}

function updatePowerSummary() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  const auto = Boolean(led.auto_power_limit);
  const currentField = qs("currentLimit");
  const autoToggle = qs("autoPowerLimit");
  const summary = qs("powerSummary");
  if (autoToggle) {
    autoToggle.checked = auto;
  }
  if (currentField) {
    currentField.readOnly = auto;
    currentField.classList.toggle("readonly", auto);
  }
  let limit = Number(led.global_current_limit_ma || 0);
  const autoLimit = computeAutoCurrentLimit(led);
  if (auto) {
    limit = autoLimit;
    if (currentField && autoLimit > 0) {
      currentField.value = autoLimit;
    }
    if (autoLimit > 0) {
      led.global_current_limit_ma = autoLimit;
    }
  }
  if (!summary) {
    return;
  }
  const hasSupply = Number(led.power_supply_voltage || 0) > 0 && Number(led.power_supply_watts || 0) > 0;
  const formattedWatts = Number(led.power_supply_watts || 0).toFixed(1);
  const formattedVolts = Number(led.power_supply_voltage || 0).toFixed(2);
  const amps = limit > 0 ? (limit / 1000).toFixed(2) : "0.00";
  if (auto && hasSupply && limit > 0) {
    summary.textContent = t("power_summary_auto")
      .replace("{milliamps}", limit.toString())
      .replace("{amps}", amps)
      .replace("{watts}", formattedWatts)
      .replace("{volts}", formattedVolts);
  } else if (hasSupply && limit > 0) {
    summary.textContent = t("power_summary_manual")
      .replace("{milliamps}", limit.toString())
      .replace("{amps}", amps)
      .replace("{watts}", formattedWatts)
      .replace("{volts}", formattedVolts);
  } else if (limit > 0) {
    summary.textContent = t("power_summary_limit_only")
      .replace("{milliamps}", limit.toString())
      .replace("{amps}", amps);
  } else {
    summary.textContent = t("power_summary_unset");
  }
}

function renderSegmentTable(led) {
  const body = qs("segmentTableBody");
  if (!body) return;
  if (!led.segments.length) {
    body.innerHTML = `<tr class="placeholder"><td colspan="14">${t("led_segments_empty")}</td></tr>`;
    return;
  }
  const rows = led.segments
    .map((seg, idx) => {
      const pinOptions = renderPinOptions(seg.gpio);
      const matrix = seg.matrix || {};
      return `<tr data-idx="${idx}">
        <td><input class="segment-field" data-field="name" data-idx="${idx}" value="${seg.name || ''}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="led_count" data-idx="${idx}" value="${seg.led_count ?? 0}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="start_index" data-idx="${idx}" value="${seg.start_index ?? 0}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="render_order" data-idx="${idx}" value="${seg.render_order ?? idx}"></td>
        <td>
          <select class="segment-pin" data-idx="${idx}">
            ${pinOptions}
          </select>
        </td>
        <td>
          <select class="segment-field" data-field="chipset" data-idx="${idx}">
            <option value="ws2811" ${seg.chipset === "ws2811" ? "selected" : ""}>WS2811</option>
            <option value="ws2812b" ${seg.chipset === "ws2812b" || !seg.chipset ? "selected" : ""}>WS2812B</option>
            <option value="ws2812" ${seg.chipset === "ws2812" ? "selected" : ""}>WS2812</option>
            <option value="ws2813" ${seg.chipset === "ws2813" ? "selected" : ""}>WS2813</option>
            <option value="ws2815" ${seg.chipset === "ws2815" ? "selected" : ""}>WS2815</option>
            <option value="sk6812" ${seg.chipset === "sk6812" ? "selected" : ""}>SK6812 (RGB)</option>
            <option value="sk6812_rgbw" ${seg.chipset === "sk6812_rgbw" ? "selected" : ""}>SK6812 RGBW</option>
            <option value="tm1814" ${seg.chipset === "tm1814" ? "selected" : ""}>TM1814 (RGBW)</option>
            <option value="tm1829" ${seg.chipset === "tm1829" ? "selected" : ""}>TM1829 (RGBW)</option>
            <option value="tm1914" ${seg.chipset === "tm1914" ? "selected" : ""}>TM1914 (RGBW)</option>
            <option value="apa102" ${seg.chipset === "apa102" ? "selected" : ""}>APA102 (DotStar)</option>
            <option value="sk9822" ${seg.chipset === "sk9822" ? "selected" : ""}>SK9822</option>
          </select>
        </td>
        <td>
          <select class="segment-field" data-field="color_order" data-idx="${idx}">
            ${(() => {
              const isRgbw = seg.chipset && (seg.chipset.includes("rgbw") || seg.chipset === "tm1814" || seg.chipset === "tm1829" || seg.chipset === "tm1914");
              if (isRgbw) {
                return `
                  <option value="GRBW" ${seg.color_order === "GRBW" || !seg.color_order ? "selected" : ""}>GRBW</option>
                  <option value="RGBW" ${seg.color_order === "RGBW" ? "selected" : ""}>RGBW</option>
                  <option value="BRGW" ${seg.color_order === "BRGW" ? "selected" : ""}>BRGW</option>
                  <option value="RBGW" ${seg.color_order === "RBGW" ? "selected" : ""}>RBGW</option>
                  <option value="GBRW" ${seg.color_order === "GBRW" ? "selected" : ""}>GBRW</option>
                  <option value="BGRW" ${seg.color_order === "BGRW" ? "selected" : ""}>BGRW</option>
                  <option value="WRGB" ${seg.color_order === "WRGB" ? "selected" : ""}>WRGB</option>
                  <option value="WGRB" ${seg.color_order === "WGRB" ? "selected" : ""}>WGRB</option>
                `;
              } else {
                return `
                  <option value="GRB" ${seg.color_order === "GRB" || !seg.color_order ? "selected" : ""}>GRB (WS2812)</option>
                  <option value="RGB" ${seg.color_order === "RGB" ? "selected" : ""}>RGB</option>
                  <option value="BRG" ${seg.color_order === "BRG" ? "selected" : ""}>BRG</option>
                  <option value="RBG" ${seg.color_order === "RBG" ? "selected" : ""}>RBG</option>
                  <option value="GBR" ${seg.color_order === "GBR" ? "selected" : ""}>GBR</option>
                  <option value="BGR" ${seg.color_order === "BGR" ? "selected" : ""}>BGR</option>
                `;
              }
            })()}
          </select>
        </td>
        <td><input type="number" min="0" max="7" class="segment-field" data-field="rmt_channel" data-idx="${idx}" value="${seg.rmt_channel ?? 0}"></td>
        <td>
          <div style="display: flex; flex-direction: column; gap: 0.25rem;">
            <label class="switch compact" style="margin-bottom: 0.25rem;">
              <input type="checkbox" class="segment-field" data-field="matrix_enabled" data-idx="${idx}" ${seg.matrix_enabled ? "checked" : ""}>
              <span></span>
            </label>
            <div class="matrix-fields" style="display: flex; gap: 0.25rem; align-items: center;">
              <input type="number" min="0" class="segment-field" data-field="matrix.width" data-idx="${idx}" value="${matrix.width ?? 0}" placeholder="W" style="width: 50px;">
              <span>&times;</span>
              <input type="number" min="0" class="segment-field" data-field="matrix.height" data-idx="${idx}" value="${matrix.height ?? 0}" placeholder="H" style="width: 50px;">
            </div>
            ${seg.matrix_enabled ? `
            <div style="display: flex; gap: 0.5rem; margin-top: 0.25rem; font-size: 0.75rem;">
              <label style="display: flex; align-items: center; gap: 0.25rem;">
                <input type="checkbox" class="segment-field" data-field="matrix.serpentine" data-idx="${idx}" ${matrix.serpentine !== false ? "checked" : ""}>
                <span>Serpentine</span>
              </label>
              <label style="display: flex; align-items: center; gap: 0.25rem;">
                <input type="checkbox" class="segment-field" data-field="matrix.vertical" data-idx="${idx}" ${matrix.vertical ? "checked" : ""}>
                <span>Vertical</span>
              </label>
            </div>
            ` : ""}
          </div>
        </td>
        <td>
          <div style="display: flex; flex-direction: column; gap: 0.25rem;">
            <input type="number" min="0" max="20000" step="100" class="segment-field" data-field="power_limit_ma" data-idx="${idx}" value="${seg.power_limit_ma ?? 0}" placeholder="0=auto" style="width: 100%;">
            ${(() => {
              const led = ensureLedEngineConfig();
              if (!led) return "";
              const voltage = led.power_supply_voltage || 5;
              const leds = seg.led_count || 0;
              // Estimate: WS2812B ~60mA per LED at full white (5V), SK6812 similar
              // For higher voltage, current is lower for same power: P = V * I, so I = P/V
              // At 5V: 60mA per LED, at 12V: ~25mA per LED (same power ~300mW)
              const baseCurrentPerLed = 60; // mA at 5V
              const estimatedMa = Math.round(leds * baseCurrentPerLed * (5.0 / voltage));
              const limitMa = seg.power_limit_ma || 0;
              const globalLimit = led.global_current_limit_ma || 0;
              if (limitMa > 0) {
                return `<small class="muted" style="font-size: 0.7rem;">Limit: ${limitMa}mA</small>`;
              } else if (globalLimit > 0) {
                return `<small class="muted" style="font-size: 0.7rem;">Global: ${globalLimit}mA</small>`;
              } else {
                return `<small class="muted" style="font-size: 0.7rem;">Est: ~${estimatedMa}mA</small>`;
              }
            })()}
          </div>
        </td>
        <td>
          <select class="segment-field" data-field="effect_source" data-idx="${idx}">
            <option value="local" ${seg.effect_source === "local" ? "selected" : ""}>Local</option>
            <option value="wled" ${seg.effect_source === "wled" ? "selected" : ""}>WLED</option>
            <option value="ledfx" ${seg.effect_source === "ledfx" ? "selected" : ""}>LEDFx</option>
            <option value="global" ${seg.effect_source === "global" ? "selected" : ""}>Global</option>
          </select>
        </td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="segment-field" data-field="reverse" data-idx="${idx}" ${seg.reverse ? "checked" : ""}>
            <span></span>
          </label>
        </td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="segment-field" data-field="mirror" data-idx="${idx}" ${seg.mirror ? "checked" : ""}>
            <span></span>
          </label>
        </td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="segment-field" data-field="enabled" data-idx="${idx}" ${seg.enabled !== false ? "checked" : ""}>
            <span></span>
          </label>
        </td>
        <td>
          <button type="button" class="ghost small danger" data-action="remove-segment" data-idx="${idx}">&times;</button>
        </td>
      </tr>`;
    })
    .join("");
  body.innerHTML = rows;
}

function renderPinOptions(selected) {
  const pins = state.ledPins || [];
  const placeholder = `<option value="">${t("pin_select")}</option>`;
  if (!pins.length) {
    return placeholder;
  }
  const opts = pins
    .map((pin) => {
      const selectedAttr = Number(pin.gpio) === Number(selected) ? "selected" : "";
      const note = pin.note ? ` (${pin.note})` : "";
      const title = `${pin.function || ""}${note}`;
      const label = pin.label || `GPIO${pin.gpio}`;
      return `<option value="${pin.gpio}" data-allowed="${pin.allowed}" title="${title}" ${selectedAttr}>${label}</option>`;
    })
    .join("");
  return placeholder + opts;
}

function renderEffectRows(led) {
  const body = qs("effectsTableBody");
  if (!body) return;
  body.innerHTML = `<tr class="placeholder"><td colspan="16">${t("led_fx_empty") || "Configure effects from the device list."}</td></tr>`;
}

function renderVirtualSegments() {
  ensureVirtualSegments();
  const container = qs("virtualList");
  if (!container) return;
  const segments = state.config.virtual_segments || [];
  if (!segments.length) {
    container.innerHTML = `<div class="placeholder muted">${t("led_segments_empty") || "No virtual segments yet"}</div>`;
    return;
  }
  const physical = (state.config?.led_engine?.segments || []).map((s) => ({ id: s.id, name: s.name || s.id }));
  const wled = (state.config?.wled_devices || []).map((d) => ({ id: d.id || d.address, name: d.name || d.id || d.address }));
  // Helper: Get LED count for physical segment
  const getPhysicalLedCount = (segId) => {
    const seg = (state.config?.led_engine?.segments || []).find(s => s.id === segId);
    return seg ? (seg.led_count || 0) : 0;
  };
  
  // Helper: Get LED count for WLED device
  const getWledLedCount = (devId) => {
    const dev = (state.config?.wled_devices || []).find(d => (d.id || d.address) === devId);
    return dev ? (dev.leds || 0) : 0;
  };
  
  const renderMember = (m, segIdx, memberIdx) => {
    const isPhysical = m.type === "physical";
    const isWled = m.type === "wled";
    const targetId = m.id || "";
    const ledCount = isPhysical ? getPhysicalLedCount(targetId) : (isWled ? getWledLedCount(targetId) : 0);
    const maxStart = Math.max(0, ledCount - 1);
    const currentStart = m.start || 0;
    const effectiveLength = (m.length || 0) === 0 && ledCount > 0 ? ledCount - currentStart : (m.length || 0);
    const maxLength = ledCount > 0 ? ledCount - currentStart : 0;
    const rangeEnd = currentStart + effectiveLength;
    const isValid = !targetId || (ledCount > 0 && currentStart < ledCount && rangeEnd <= ledCount && effectiveLength > 0);
    
    const options =
      `<option value="">${t("virtual_select_target") || "Select device or segment..."}</option>` +
      (physical.length > 0 ? `<optgroup label="${t("virtual_physical_segments") || "Physical Segments"}">` +
      physical.map((p) => {
        const pLedCount = getPhysicalLedCount(p.id);
        return `<option value="physical:${p.id}" ${m.type === "physical" && m.id === p.id ? "selected" : ""}>${p.name} (${pLedCount} LED${pLedCount !== 1 ? 's' : ''})</option>`;
      }).join("") + `</optgroup>` : "") +
      (wled.length > 0 ? `<optgroup label="${t("virtual_wled_devices") || "WLED Devices"}">` +
      wled.map((d) => {
        const wLedCount = getWledLedCount(d.id);
        return `<option value="wled:${d.id}" ${m.type === "wled" && m.id === d.id ? "selected" : ""}>${d.name} (${wLedCount} LED${wLedCount !== 1 ? 's' : ''})</option>`;
      }).join("") + `</optgroup>` : "");
    
    return `
      <div class="virtual-member ${isValid ? '' : 'invalid'}" data-virtual="${segIdx}" data-member="${memberIdx}">
        <div class="virtual-member-header">
          <div class="virtual-member-type-badge ${isPhysical ? 'type-physical' : (isWled ? 'type-wled' : 'type-none')}">
            ${isPhysical ? '🔌' : (isWled ? '📡' : '❓')}
            <span>${isPhysical ? (t("virtual_type_physical") || "Physical") : (isWled ? (t("virtual_type_wled") || "WLED") : (t("virtual_type_none") || "Not selected"))}</span>
          </div>
          <button type="button" class="ghost small danger" data-action="remove-virtual-member" title="${t("virtual_remove_member") || "Remove member"}">&times;</button>
        </div>
        <div class="virtual-member-body">
          <label class="virtual-member-target-label">
            <span>${t("virtual_target_device") || "Target Device/Segment"}</span>
            <select class="virtual-member-target">
              ${options}
            </select>
            ${targetId && ledCount > 0 ? `<small class="muted">${ledCount} LED${ledCount !== 1 ? 's' : ''} available</small>` : ''}
          </label>
          ${isWled ? `
          <label class="virtual-member-seg-label">
            <span>${t("virtual_wled_segment") || "WLED Segment Index"}</span>
            <input type="number" min="0" class="virtual-member-seg" value="${m.segment_index ?? 0}" placeholder="0">
            <small class="muted">${t("virtual_wled_segment_hint") || "Segment number on WLED device (0 = first segment)"}</small>
          </label>
          ` : ''}
          <div class="virtual-member-range">
            <label class="virtual-member-start-label">
              <span>${t("virtual_start_index") || "Start LED"}</span>
              <input type="number" min="0" max="${maxStart}" class="virtual-member-start" value="${currentStart}" placeholder="0">
              <small class="muted">${t("virtual_start_hint") || "First LED index (0-based)"}</small>
            </label>
            <label class="virtual-member-length-label">
              <span>${t("virtual_length") || "LED Count"}</span>
              <input type="number" min="0" max="${maxLength}" class="virtual-member-length" value="${m.length || 0}" placeholder="${ledCount > 0 ? ledCount : '0'}">
              <small class="muted">${t("virtual_length_hint") || "Number of LEDs to use (0 = all remaining from start)"}</small>
            </label>
          </div>
          ${ledCount > 0 ? `
          <div class="virtual-member-preview">
            <div class="virtual-member-preview-label">${t("virtual_range_preview") || "Range:"}</div>
            <div class="virtual-member-preview-bar">
              <div class="virtual-member-preview-used" style="width: ${(rangeEnd / ledCount) * 100}%; left: ${(currentStart / ledCount) * 100}%;"></div>
              <div class="virtual-member-preview-text">LED ${currentStart} - ${Math.min(rangeEnd - 1, ledCount - 1)} (${effectiveLength} LED${effectiveLength !== 1 ? 's' : ''})</div>
            </div>
          </div>
          ` : ''}
          ${!isValid && targetId ? `<div class="virtual-member-error">⚠️ ${t("virtual_range_invalid") || "Range exceeds available LEDs"}</div>` : ''}
        </div>
      </div>
    `;
  };
  container.innerHTML = segments
    .map((seg, idx) => {
      const members = seg.members.map((m, mIdx) => renderMember(m, idx, mIdx)).join("");
      const totalMembers = seg.members?.length || 0;
      const enabledMembers = seg.members?.filter(m => m.id && (m.type === "physical" || m.type === "wled")).length || 0;
      return `
        <div class="virtual-card" data-virtual="${idx}">
          <div class="virtual-header">
            <div class="virtual-header-main">
              <input class="virtual-name" value="${seg.name || ''}" placeholder="${t("virtual_name_placeholder") || "Virtual Segment Name"}">
              <div class="virtual-header-meta">
                <span class="virtual-member-count">${enabledMembers} ${t("virtual_member_count") || "member" + (enabledMembers !== 1 ? 's' : '')}</span>
                <span class="virtual-id-display">ID: <code>${seg.id || 'auto'}</code></span>
              </div>
            </div>
            <div class="virtual-header-actions">
              <label class="switch compact" title="${t("virtual_enable_hint") || "Enable/disable this virtual segment"}">
                <input type="checkbox" class="virtual-enabled" ${seg.enabled !== false ? "checked" : ""}>
                <span></span>
                <span class="switch-label">${t("badge_enabled") || "Enabled"}</span>
              </label>
              <button type="button" class="ghost small danger" data-action="remove-virtual" title="${t("virtual_remove_segment") || "Remove virtual segment"}">&times;</button>
            </div>
          </div>
          <div class="virtual-body">
            <div class="virtual-members-header">
              <h4>${t("virtual_members_title") || "Members"}</h4>
              <p class="muted">${t("virtual_members_desc") || "Add devices or segments to combine them into one virtual group"}</p>
            </div>
            <div class="virtual-members">
              ${members || `<div class="placeholder muted">${t("virtual_no_members") || "No members yet. Click 'Add member' to start."}</div>`}
            </div>
            <div class="form-actions start">
              <button type="button" class="ghost small" data-action="add-virtual-member" data-virtual="${idx}">
                <span>+</span> ${t("virtual_add_member") || "Add member"}
              </button>
            </div>
          </div>
        </div>
      `;
    })
    .join("");
}

function addVirtualSegment() {
  ensureVirtualSegments();
  state.config.virtual_segments.push(createVirtualSegmentTemplate());
  renderVirtualSegments();
}

function addVirtualMember(segIdx) {
  ensureVirtualSegments();
  const seg = state.config.virtual_segments[segIdx];
  if (!seg) return;
  if (!seg.members) seg.members = [];
  seg.members.push({ type: "physical", id: "", segment_index: 0, start: 0, length: 0 });
  renderVirtualSegments();
  // Scroll to the new member
  setTimeout(() => {
    const newMember = document.querySelector(`.virtual-member[data-virtual="${segIdx}"][data-member="${seg.members.length - 1}"]`);
    if (newMember) {
      newMember.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      const select = newMember.querySelector('.virtual-member-target');
      if (select) select.focus();
    }
  }, 100);
}

function removeVirtualSegment(segIdx) {
  ensureVirtualSegments();
  state.config.virtual_segments.splice(segIdx, 1);
  renderVirtualSegments();
}

function removeVirtualMember(segIdx, memberIdx) {
  ensureVirtualSegments();
  const seg = state.config.virtual_segments[segIdx];
  if (!seg || !seg.members) return;
  seg.members.splice(memberIdx, 1);
  renderVirtualSegments();
}

function handleVirtualListInput(event) {
  const card = event.target.closest(".virtual-card");
  if (!card) return;
  const segIdx = parseInt(card.dataset.virtual, 10);
  ensureVirtualSegments();
  const seg = state.config.virtual_segments[segIdx];
  if (!seg) return;
  if (event.target.classList.contains("virtual-name")) {
    seg.name = event.target.value;
    return;
  }
  if (event.target.classList.contains("virtual-id")) {
    seg.id = event.target.value;
    return;
  }
  if (event.target.classList.contains("virtual-enabled")) {
    seg.enabled = event.target.checked;
    // Re-render to update checkbox visual state
    renderVirtualSegments();
    return;
  }
  if (event.target.classList.contains("virtual-member-target")) {
    const val = event.target.value || "";
    const [type, ref] = val.includes(":") ? val.split(":") : [seg.members?.type || "physical", val];
    const memberIdx = parseInt(event.target.closest(".virtual-member")?.dataset.member || "0", 10);
    const mem = seg.members[memberIdx];
    if (mem) {
      mem.type = type || "physical";
      mem.id = ref || "";
      // Reset start/length when target changes
      if (!val) {
        mem.start = 0;
        mem.length = 0;
      }
    }
    // Re-render to update LED count and validation
    renderVirtualSegments();
    return;
  }
  if (event.target.classList.contains("virtual-member-seg")) {
    const memberIdx = parseInt(event.target.closest(".virtual-member")?.dataset.member || "0", 10);
    const mem = seg.members[memberIdx];
    if (mem) {
      let val = parseInt(event.target.value, 10);
      if (!Number.isFinite(val) || val < 0) val = 0;
      mem.segment_index = val;
      event.target.value = val;
    }
    return;
  }
  if (event.target.classList.contains("virtual-member-start")) {
    const memberIdx = parseInt(event.target.closest(".virtual-member")?.dataset.member || "0", 10);
    const mem = seg.members[memberIdx];
    if (mem) {
      let val = parseInt(event.target.value, 10);
      if (!Number.isFinite(val) || val < 0) val = 0;
      mem.start = val;
      event.target.value = val;
      // Re-render to update preview and validation
      renderVirtualSegments();
    }
    return;
  }
  if (event.target.classList.contains("virtual-member-length")) {
    const memberIdx = parseInt(event.target.closest(".virtual-member")?.dataset.member || "0", 10);
    const mem = seg.members[memberIdx];
    if (mem) {
      let val = parseInt(event.target.value, 10);
      if (!Number.isFinite(val) || val < 0) val = 0;
      mem.length = val;
      event.target.value = val;
      // Re-render to update preview and validation
      renderVirtualSegments();
    }
    return;
  }
}

function handleVirtualListClick(event) {
  const addBtn = event.target.closest("[data-action='add-virtual-member']");
  if (addBtn) {
    const segIdx = parseInt(addBtn.dataset.virtual || "0", 10);
    addVirtualMember(segIdx);
    return;
  }
  const removeSeg = event.target.closest("[data-action='remove-virtual']");
  if (removeSeg) {
    const segIdx = parseInt(removeSeg.closest(".virtual-card")?.dataset.virtual || "0", 10);
    removeVirtualSegment(segIdx);
    return;
  }
  const removeMember = event.target.closest("[data-action='remove-virtual-member']");
  if (removeMember) {
    const wrapper = removeMember.closest(".virtual-member");
    const segIdx = parseInt(wrapper?.dataset.virtual || "0", 10);
    const memberIdx = parseInt(wrapper?.dataset.member || "0", 10);
    removeVirtualMember(segIdx, memberIdx);
    return;
  }
}

function setSelectedFxSegment(segId) {
  if (!segId) return;
  state.selectedFxSegment = segId;
  highlightEffectSelection();
  renderFxDetail();
}

function highlightEffectSelection() {
  const rows = document.querySelectorAll("#effectsTableBody tr[data-segid]");
  rows.forEach((row) => {
    row.classList.toggle("selected", row.dataset.segid === state.selectedFxSegment);
  });
  const chips = document.querySelectorAll("#fxSegmentList button[data-segid]");
  chips.forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.segid === state.selectedFxSegment);
  });
}

function renderFxSegmentList() {
  const container = qs("fxSegmentList");
  if (!container) return;
  const led = ensureLedEngineConfig();
  const physical = (led?.segments || []).map(
    (seg) => `<button class="chip ghost ${state.selectedFxSegment === seg.id ? "active" : ""}" data-segid="${seg.id}">
      ${seg.name || seg.id} <span class="muted">(${seg.led_count || 0} LED)</span>
    </button>`
  );
  const wled = (state.wledDevices || []).map(
    (dev) => `<div class="chip muted" title="${dev.address || ""}">${dev.name || dev.id} ┬Ě ${dev.segments || 1} seg</div>`
  );
  container.innerHTML = `
    <div class="fx-group">
      <div class="fx-group-title">${t("lights_group_physical") || "Physical"}</div>
      <div class="fx-group-body">${physical.join("") || `<span class="muted">${t("led_segments_empty")}</span>`}</div>
    </div>
    <div class="fx-group">
      <div class="fx-group-title">${t("lights_group_wled") || "WLED"}</div>
      <div class="fx-group-body">${wled.join("") || `<span class="muted">${t("wled_empty_hint") || "-"}</span>`}</div>
    </div>
  `;
}

function renderFxDetail() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  const seg = led.segments.find((s) => s.id === state.selectedFxSegment);
  const output = qs("fxSelectedLabel");
  if (!seg) {
    if (output) output.textContent = t("not_set");
    return;
  }
  const assign = ensureEffectAssignment(seg.id);
  renderEffectPicker(assign.effect);
  if (output) {
    output.textContent = `${seg.name || seg.id} ? ${seg.led_count || 0} LED`;
  }
  const picker = qs("fxEffectPicker");
  if (picker) {
    picker.value = assign.effect || "";
    const meta = findEffectMeta(assign.effect);
    const desc = qs("fxEffectDescription");
    if (desc) {
      desc.textContent = meta?.desc || t("effects_hint_pick") || "";
    }
  }
  const map = {
    fxDirection: assign.direction,
    fxScatter: assign.scatter,
    fxFadeIn: assign.fade_in,
    fxFadeOut: assign.fade_out,
    fxColor1: assign.color1,
    fxColor2: assign.color2,
    fxColor3: assign.color3,
    fxPalette: assign.palette,
    fxGradient: assign.gradient,
    fxBrightnessOverride: assign.brightness_override,
    fxGammaColor: assign.gamma_color,
    fxGammaBrightness: assign.gamma_brightness,
    fxBlendMode: assign.blend_mode,
    fxLayers: assign.layers,
    fxReactiveMode: assign.reactive_mode,
    fxSensBass: assign.band_gain_low,
    fxSensMids: assign.band_gain_mid,
    fxSensTreble: assign.band_gain_high,
    fxAmplitude: assign.amplitude_scale,
    fxBrightnessCompress: assign.brightness_compress,
    fxBeatResponse: assign.beat_response,
    fxAttack: assign.attack_ms,
    fxRelease: assign.release_ms,
    fxScenePreset: assign.scene_preset,
    fxSceneSchedule: assign.scene_schedule,
    fxBeatShuffle: assign.beat_shuffle,
  };
  Object.entries(map).forEach(([id, value]) => {
    const el = qs(id);
    if (!el) return;
    if (el.type === "checkbox") {
      el.checked = Boolean(value);
    } else {
      el.value = value ?? "";
    }
  });
}

function getSelectedFxAssignment() {
  if (!state.selectedFxSegment) return null;
  return ensureEffectAssignment(state.selectedFxSegment);
}

function handleEffectRowClick(event) {
  const row = event.target.closest("tr[data-segid]");
  if (!row) return;
  setSelectedFxSegment(row.dataset.segid);
}

function handleEffectPickerChange(event) {
  const assign = getSelectedFxAssignment();
  if (!assign) return;
  const oldEffect = assign.effect;
  const newEffect = event.target.value || "";
  
  // If effect changed, apply defaults for the new effect
  if (newEffect && newEffect !== oldEffect) {
    // Find the engine for this effect
    let engine = assign.engine || "wled";
    for (const group of EFFECT_LIBRARY) {
      for (const eff of group.effects) {
        if (eff.name === newEffect) {
          engine = eff.engine || engine;
          break;
        }
      }
    }
    
    // Update engine if needed
    assign.engine = engine;
    
    // Apply effect-specific defaults
    applyEffectDefaults(assign, newEffect, engine, true);
  }
  
  assign.effect = newEffect;
  const led = ensureLedEngineConfig();
  renderEffectRows(led);
  renderFxDetail();
  renderLightsDashboard();
}

function handleFxSegmentChipClick(event) {
  const btn = event.target.closest("button[data-segid]");
  if (!btn) return;
  setSelectedFxSegment(btn.dataset.segid);
}

function handleFxDetailChange(event) {
  const assign = getSelectedFxAssignment();
  if (!assign) return;
  const map = {
    fxDirection: "direction",
    fxScatter: "scatter",
    fxFadeIn: "fade_in",
    fxFadeOut: "fade_out",
    fxColor1: "color1",
    fxColor2: "color2",
    fxColor3: "color3",
    fxPalette: "palette",
    fxGradient: "gradient",
    fxBrightnessOverride: "brightness_override",
    fxGammaColor: "gamma_color",
    fxGammaBrightness: "gamma_brightness",
    fxBlendMode: "blend_mode",
    fxLayers: "layers",
    fxReactiveMode: "reactive_mode",
    fxSensBass: "band_gain_low",
    fxSensMids: "band_gain_mid",
    fxSensTreble: "band_gain_high",
    fxAmplitude: "amplitude_scale",
    fxBrightnessCompress: "brightness_compress",
    fxBeatResponse: "beat_response",
    fxAttack: "attack_ms",
    fxRelease: "release_ms",
    fxScenePreset: "scene_preset",
    fxSceneSchedule: "scene_schedule",
    fxBeatShuffle: "beat_shuffle",
    devFxFreqMin: "freq_min",
    devFxFreqMax: "freq_max",
  };
  const field = map[event.target.id];
  if (!field) return;
  let value = event.target.type === "checkbox" ? event.target.checked : event.target.value;
  const numericFields = [
    "scatter",
    "fade_in",
    "fade_out",
    "brightness_override",
    "gamma_color",
    "gamma_brightness",
    "layers",
    "band_gain_low",
    "band_gain_mid",
    "band_gain_high",
    "freq_min",
    "freq_max",
    "amplitude_scale",
    "brightness_compress",
    "attack_ms",
    "release_ms",
  ];
  if (numericFields.includes(field)) {
    const parsed = event.target.type === "number" ? parseFloat(value) : parseFloat(value);
    value = Number.isFinite(parsed) ? parsed : 0;
    if (["layers", "brightness_override"].includes(field)) {
      value = Math.max(0, Math.min(value, field === "layers" ? 8 : 255));
    }
  }
  assign[field] = value;
}

function renderAudioModeOptions(selected) {
  return AUDIO_MODES.map((mode) => {
    const key = `audio_mode_${mode}`;
    const label = T[key] ? t(key) : mode;
    const sel = mode === selected ? "selected" : "";
    return `<option value="${mode}" ${sel}>${label}</option>`;
  }).join("");
}

function renderAudioProfileOptions(selected) {
  const current = selected || "default";
  return AUDIO_PROFILES.map(({ value, label }) => {
    const optionLabel = t(label) || value;
    const sel = value === current ? "selected" : "";
    return `<option value="${value}" ${sel}>${optionLabel}</option>`;
  }).join("");
}

function populateAudioForm(led) {
  const audio = led.audio || defaultAudioConfig();
  const snap = audio.snapcast || defaultAudioConfig().snapcast;
  const mappings = [
    ["audioSource", audio.source || "none"],
    ["audioSampleRate", audio.sample_rate ?? 48000],
    ["audioFft", audio.fft_size ?? 1024],
    ["audioFrame", audio.frame_ms ?? 20],
    ["audioSensitivity", audio.sensitivity ?? 1],
    ["audioStereo", audio.stereo ?? true],
    ["snapHost", snap.host || ""],
    ["snapPort", snap.port ?? 1704],
    ["snapLatency", snap.latency_ms ?? 60],
    ["snapUdp", snap.prefer_udp ?? true],
  ];
  mappings.forEach(([id, value]) => {
    const el = qs(id);
    if (!el) return;
    if (el.type === "checkbox") {
      el.checked = Boolean(value);
    } else {
      el.value = value;
    }
  });
}

function handleHardwareFieldChange(event) {
  const led = ensureLedEngineConfig();
  if (!led) return;
  const map = {
    ledDriver: "driver",
    maxFps: "max_fps",
    currentLimit: "global_current_limit_ma",
    psuVoltage: "power_supply_voltage",
    psuWatts: "power_supply_watts",
    autoPowerLimit: "auto_power_limit",
    parallelOutputs: "parallel_outputs",
    enableDma: "enable_dma",
  };
  const field = map[event.target.id];
  if (!field) return;
  let value = event.target.type === "checkbox" ? event.target.checked : event.target.value;
  if (["max_fps", "global_current_limit_ma", "parallel_outputs"].includes(field)) {
    value = parseInt(value, 10);
    if (Number.isNaN(value)) value = 0;
    if (field === "max_fps") {
      value = Math.max(1, Math.min(value, 240));
    } else if (field === "global_current_limit_ma") {
      value = Math.max(0, value);
    } else if (field === "parallel_outputs") {
      value = Math.max(1, Math.min(value, 8));
    }
  } else if (["power_supply_voltage", "power_supply_watts"].includes(field)) {
    value = parseFloat(value);
    if (!Number.isFinite(value) || value < 0) value = 0;
  }
  if (field === "auto_power_limit") {
    value = Boolean(value);
  }
  if (field === "driver") {
    value = value || "esp_rmt";
  }
  led[field] = value;
  if (["auto_power_limit", "power_supply_voltage", "power_supply_watts", "global_current_limit_ma"].includes(field)) {
    updatePowerSummary();
  }
}

function handleSegmentInput(event) {
  const target = event.target;
  if (!target.classList.contains("segment-field")) return;
  const idx = Number(target.dataset.idx);
  const field = target.dataset.field;
  const led = ensureLedEngineConfig();
  if (!led || Number.isNaN(idx)) return;
  const seg = led.segments[idx];
  if (!seg || !field) return;
  let value = target.type === "checkbox" ? target.checked : target.value;
  if (["led_count", "start_index", "rmt_channel", "power_limit_ma", "render_order"].includes(field)) {
    value = parseInt(value, 10);
    if (Number.isNaN(value)) value = 0;
    if (field === "rmt_channel") {
      value = Math.max(0, Math.min(value, 7));
    }
    if (["led_count", "start_index", "power_limit_ma", "render_order"].includes(field)) {
      value = Math.max(0, value);
    }
    if (field === "power_limit_ma") {
      value = Math.min(value, 20000); // Max 20A per segment
    }
  }
  if (field.startsWith("matrix.")) {
    const [, key] = field.split(".");
    seg.matrix = seg.matrix || { width: 0, height: 0, serpentine: true, vertical: false };
    if (key === "serpentine" || key === "vertical") {
      // Boolean fields
      seg.matrix[key] = target.type === "checkbox" ? target.checked : Boolean(value);
    } else {
      // Numeric fields
      let numeric = parseInt(value, 10);
      if (Number.isNaN(numeric)) numeric = 0;
      seg.matrix[key] = numeric;
    }
    if (key === "width" || key === "height") {
      seg.matrix_enabled = seg.matrix_enabled || (seg.matrix.width > 0 && seg.matrix.height > 0);
      // Re-render to show/hide matrix options
      renderSegmentTable(led);
    }
  } else if (field === "chipset" || field === "color_order") {
    // String fields - chipset and color_order
    seg[field] = value || (field === "chipset" ? "ws2812b" : "GRB");
    // If chipset changed, update color_order options and re-render
    if (field === "chipset") {
      renderSegmentTable(led);
    }
  } else {
    seg[field] = value;
  }
  if (field === "name") {
    renderEffectRows(led);
  }
  if (field === "matrix_enabled") {
    // Re-render to show/hide matrix options
    renderSegmentTable(led);
  }
  if (field === "enabled") {
    // Re-render to update checkbox visual state
    renderSegmentTable(led);
  }
  // Re-render power estimate when LED count or power limit changes
  if (field === "led_count" || field === "power_limit_ma") {
    renderSegmentTable(led);
    if (led.auto_power_limit) {
      updatePowerSummary();
    }
  }
}

function handleSegmentPinChange(event) {
  const select = event.target;
  if (!select.classList.contains("segment-pin")) return;
  const idx = Number(select.dataset.idx);
  const led = ensureLedEngineConfig();
  if (!led || Number.isNaN(idx)) return;
  const seg = led.segments[idx];
  if (!seg) return;
  if (!select.value) {
    seg.gpio = -1;
    return;
  }
  const gpio = parseInt(select.value, 10);
  const pinInfo = state.ledPins.find((p) => Number(p.gpio) === gpio);
  if (!pinInfo || !pinInfo.allowed) {
    select.value = "";
    seg.gpio = -1;
    notify(t("toast_pin_forbidden"), "warn");
    return;
  }
  seg.gpio = gpio;
}

function handleSegmentClick(event) {
  const btn = event.target.closest("[data-action='remove-segment']");
  if (!btn) return;
  const idx = Number(btn.dataset.idx);
  const led = ensureLedEngineConfig();
  if (!led || Number.isNaN(idx)) return;
  const seg = led.segments[idx];
  led.segments.splice(idx, 1);
  if (seg) {
    led.effects.assignments = led.effects.assignments.filter((a) => a.segment_id !== seg.id);
  }
  renderLedConfig();
}

function addSegment() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  led.segments.push(createSegmentTemplate());
  renderLedConfig();
  const body = qs("segmentTableBody");
  if (body) {
    body.scrollIntoView({ behavior: "smooth", block: "center" });
  }
}

function handleEffectInput(event) {
  const target = event.target;
  if (target.classList.contains("effect-field")) {
    const segId = target.dataset.segid;
    const assignment = ensureEffectAssignment(segId);
    if (!assignment) return;
    const field = target.dataset.field;
    if (["brightness", "intensity", "speed"].includes(field)) {
      let value = parseInt(target.value, 10);
      if (Number.isNaN(value)) value = 0;
      value = Math.max(0, Math.min(value, 255));
      assignment[field] = value;
      target.value = value;
    } else {
      assignment[field] = target.type === "checkbox" ? target.checked : target.value;
    }
  } else if (target.classList.contains("audio-field")) {
    handleAudioFieldChange(target);
  }
}

function handleAudioFieldChange(target) {
  const segId = target.dataset.segid;
  const led = ensureLedEngineConfig();
  if (!led) return;
  const seg = led.segments.find((s) => s.id === segId);
  if (!seg) return;
  seg.audio = seg.audio || defaultSegmentAudio();
  const field = target.dataset.field;
  if (!target.classList.contains("audio-field")) return;
  let value;
  if (target.type === "checkbox") {
    value = target.checked;
  } else {
    value = parseFloat(target.value);
    if (!Number.isFinite(value)) {
      value = 0;
    }
    if (["threshold", "smoothing"].includes(field)) {
      value = Math.max(0, Math.min(value, 1));
    } else if (["gain_left", "gain_right", "sensitivity"].includes(field)) {
      value = Math.max(0, value);
    }
    target.value = value;
  }
  seg.audio[field] = value;
}

function handleAudioFormChange() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  const audio = led.audio;
  const snap = audio.snapcast || defaultAudioConfig().snapcast;
  audio.source = qs("audioSource")?.value || "none";
  audio.sample_rate = parseInt(qs("audioSampleRate")?.value, 10) || 48000;
  audio.fft_size = parseInt(qs("audioFft")?.value, 10) || 1024;
  audio.frame_ms = parseInt(qs("audioFrame")?.value, 10) || 20;
  audio.sensitivity = parseFloat(qs("audioSensitivity")?.value) || 1;
  audio.stereo = qs("audioStereo")?.checked ?? true;
  snap.host = qs("snapHost")?.value.trim() || "";
  snap.port = parseInt(qs("snapPort")?.value, 10) || 1704;
  snap.latency_ms = parseInt(qs("snapLatency")?.value, 10) || 60;
  snap.prefer_udp = qs("snapUdp")?.checked ?? true;
  snap.enabled = audio.source === "snapcast" && Boolean(snap.host);
  audio.snapcast = snap;
}

async function saveLedConfig() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  ensureVirtualSegments();
  handleAudioFormChange();
  try {
    await fetch("/api/led/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ led_engine: state.config.led_engine, virtual_segments: state.config.virtual_segments, wled_effects: state.config.wled_effects }),
    });
    notify(t("toast_led_saved"), "ok");
    refreshInfo();
    // Auto-start effects if they are configured and not already running
    const hasEffects = (led.segments && led.segments.length > 0) || 
                       (state.config.virtual_segments && state.config.virtual_segments.length > 0) ||
                       (state.config.wled_effects && state.config.wled_effects.bindings && state.config.wled_effects.bindings.length > 0);
    if (hasEffects && !state.ledState?.enabled) {
      // Check if any segment has an effect assigned
      const hasAssignedEffects = led.segments.some(seg => {
        const assign = ensureEffectAssignment(seg.id);
        return assign && assign.effect && assign.effect.trim() !== "";
      });
      if (hasAssignedEffects) {
        // Auto-start effects
        await fetch("/api/led/state", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled: true }),
        });
        state.ledState = state.ledState || {};
        state.ledState.enabled = true;
        if (state.info && state.info.led_engine) {
          state.info.led_engine.enabled = true;
        }
        updateControlButtons();
        notify(t("toast_led_started"), "ok");
      }
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_led_failed"), "error");
  }
}

// Auto-save with debouncing
function autoSaveLedConfig() {
  if (state.saveLedConfigTimer) {
    clearTimeout(state.saveLedConfigTimer);
  }
  state.saveLedConfigTimer = setTimeout(() => {
    saveLedConfig();
  }, 500); // 500ms debounce
}

async function saveWledEffects() {
  ensureWledEffectsConfig();
  try {
    await fetch("/api/wled/effects", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ wled_effects: state.config.wled_effects }),
    });
    notify(t("toast_led_saved") || "Saved", "ok");
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function loadLedPins() {
  try {
    const res = await fetch("/api/led/pins");
    state.ledPins = await res.json();
    renderLedConfig();
  } catch (err) {
    console.warn("led pins", err);
  }
}

async function rescanWled() {
  try {
    notify(t("toast_wled_rescan"), "warn");
    await fetch("/api/wled/rescan", { method: "POST" });
    setTimeout(() => loadWledDevices(true), 1400);
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function addWledManual(event) {
  if (event) {
    event.preventDefault();
  }
  const nameInput = qs("wledManualName");
  const addressInput = qs("wledManualAddress");
  const ledsInput = qs("wledManualLeds");
  const segmentsInput = qs("wledManualSegments");

  const name = nameInput?.value.trim() || "";
  const address = addressInput?.value.trim() || "";
  const ledsValue = parseInt(ledsInput?.value, 10);
  const segmentsValue = parseInt(segmentsInput?.value, 10);

  if (!address) {
    notify(t("toast_wled_add_failed"), "error");
    return;
  }

  const payload = { name, address };
  if (Number.isFinite(ledsValue) && ledsValue > 0) {
    payload.leds = ledsValue;
  }
  if (Number.isFinite(segmentsValue) && segmentsValue > 0) {
    payload.segments = segmentsValue;
  }

  try {
    const res = await fetch("/api/wled/add", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    if (!res.ok) {
      throw new Error(await res.text());
    }
    notify(t("toast_wled_added"), "ok");
    state.config = state.config || {};
    state.config.wled_devices = Array.isArray(state.config.wled_devices) ? state.config.wled_devices : [];
    const existingIdx = state.config.wled_devices.findIndex(
      (dev) => dev.id === payload.address || dev.address === payload.address || dev.name === payload.name
    );
    const newEntry = {
      id: payload.address || payload.name || `manual-${Date.now()}`,
      name: payload.name || payload.address,
      address: payload.address,
      leds: payload.leds || 0,
      segments: payload.segments || 1,
      active: true,  // Must be true for DDP to work
      auto_discovered: false,
    };
    if (existingIdx >= 0) {
      const current = state.config.wled_devices[existingIdx];
      state.config.wled_devices[existingIdx] = { ...current, ...newEntry };
      // Ensure active is true when updating
      state.config.wled_devices[existingIdx].active = true;
    } else {
      state.config.wled_devices.push(newEntry);
    }
    // Ensure device has a binding with effect when added/updated
    const deviceId = newEntry.id;
    const binding = getWledBinding(deviceId);
    // Ensure binding has a valid effect
    if (!binding.effect || !binding.effect.effect || binding.effect.effect.trim() === "") {
      binding.effect = binding.effect || {};
      binding.effect.effect = "Solid";
      binding.effect.engine = "wled";
      binding.effect.brightness = 255;
      binding.effect.intensity = 128;
      binding.effect.speed = 128;
    }
    // Ensure binding is enabled and DDP is enabled
    binding.enabled = true;
    binding.ddp = true;
    // Save binding immediately
    saveWledEffects();
    renderWledDevices();
    renderLightsDashboard();
    if (nameInput) nameInput.value = "";
    if (addressInput) addressInput.value = "";
    if (ledsInput) ledsInput.value = "";
    if (segmentsInput) segmentsInput.value = "1";
    await loadWledDevices(false);
  } catch (err) {
    console.error(err);
    notify(t("toast_wled_add_failed"), "error");
  }
}

async function loadConfig() {
  try {
    console.log("loadConfig: Fetching config");
    const res = await fetch("/api/get_config");
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}: ${res.statusText}`);
    }
    const cfg = await res.json();
    console.log("loadConfig: Config received, lang:", cfg.lang);
    state.config = cfg;
    ensureWledEffectsConfig();
    
    // Load language FIRST before setting UI values
    const langToLoad = cfg.lang || "pl";
    console.log("loadConfig: Loading language:", langToLoad);
    await loadLang(langToLoad);
    syncLanguageSelects(langToLoad);
    
    if (cfg.led_engine && typeof cfg.led_engine.global_brightness === "number") {
      state.globalBrightness = clamp(cfg.led_engine.global_brightness, 1, MAX_BRIGHTNESS);
      syncBrightnessUi();
    }
    qs("dhcp").checked = cfg.network?.use_dhcp ?? true;
    qs("host").value = cfg.network?.hostname || "ledbrain";
    qs("ip").value = cfg.network?.static_ip || "";
    qs("mask").value = cfg.network?.netmask || "";
    qs("gw").value = cfg.network?.gateway || "";
    qs("dns").value = cfg.network?.dns || "";

    qs("mqttEnabled").checked = cfg.mqtt?.configured ?? false;
    qs("mqttHost").value = cfg.mqtt?.host || "";
    qs("mqttPort").value = cfg.mqtt?.port ?? 1883;
    qs("mqttUser").value = cfg.mqtt?.username || "";
    qs("mqttPass").value = cfg.mqtt?.password || "";
    qs("ddpTarget").value = cfg.mqtt?.ddp_target || "wled.local";
    qs("ddpPort").value = cfg.mqtt?.ddp_port ?? 4048;

    qs("autostart").checked = cfg.autostart ?? false;

    updateDhcpUi();
    updateMqttUi();

    ensureLedEngineConfig();
    renderLightsDashboard();
    renderDeviceExplorer();
    await refreshInfo();
    await loadLedState();
    // Ensure brightness UI is synced after all data is loaded
    if (typeof state.globalBrightness === "number") {
      syncBrightnessUi();
    }
    // Refresh device preview after config and info are loaded
    renderDevicePreview();
    startAutoRefreshLoops();
  } catch (err) {
    console.error(err);
    notify(t("toast_load_failed"), "error");
  }
}

async function scanWifi() {
  const btnScan = qs("btnWifiScan");
  const statusDiv = qs("wifiStatus");
  const networksList = qs("wifiNetworksList");
  const networksDiv = qs("wifiNetworks");
  const ssidSelect = qs("wifiSsid");
  
  if (btnScan) btnScan.disabled = true;
  if (statusDiv) statusDiv.textContent = t("wifi_scanning") || "Scanning...";
  if (networksDiv) networksDiv.innerHTML = "";
  
  try {
    const res = await fetch("/api/wifi/scan");
    if (!res.ok) throw new Error(await res.text());
    const networks = await res.json();
    
    // Clear and populate select
    if (ssidSelect) {
      ssidSelect.innerHTML = '<option value="">Select network...</option>';
      networks.forEach(net => {
        const option = document.createElement("option");
        option.value = net.ssid;
        option.textContent = `${net.ssid} ${net.secure ? "🔒" : ""} (${net.rssi} dBm)`;
        ssidSelect.appendChild(option);
      });
    }
    
    // Populate networks list
    if (networksDiv && networks.length > 0) {
      networksDiv.innerHTML = networks.map(net => `
        <button type="button" class="ghost" style="text-align: left; padding: 0.75rem; border: 1px solid var(--border); border-radius: 6px;" 
                onclick="selectWifiNetwork('${net.ssid.replace(/'/g, "\\'")}', ${net.secure})">
          <div style="display: flex; justify-content: space-between; align-items: center;">
            <div>
              <strong>${net.ssid}</strong>
              ${net.secure ? " 🔒" : ""}
            </div>
            <div class="muted" style="font-size: 0.85rem;">
              ${net.rssi} dBm • Ch ${net.channel}
            </div>
          </div>
        </button>
      `).join("");
      networksList.style.display = "block";
    }
    
    if (statusDiv) statusDiv.textContent = t("wifi_scan_complete") || `Found ${networks.length} networks`;
  } catch (err) {
    console.error("WiFi scan error:", err);
    if (statusDiv) statusDiv.textContent = t("wifi_scan_failed") || "Scan failed: " + err.message;
    notify(t("wifi_scan_failed") || "WiFi scan failed", "error");
  } finally {
    if (btnScan) btnScan.disabled = false;
  }
}

function selectWifiNetwork(ssid, secure) {
  const ssidSelect = qs("wifiSsid");
  const passwordInput = qs("wifiPassword");
  if (ssidSelect) ssidSelect.value = ssid;
  if (passwordInput && secure) {
    passwordInput.focus();
    passwordInput.placeholder = "Enter password for " + ssid;
  }
}

async function connectWifi() {
  const ssidSelect = qs("wifiSsid");
  const passwordInput = qs("wifiPassword");
  const statusDiv = qs("wifiStatus");
  const btnConnect = qs("btnWifiConnect");
  
  if (!ssidSelect || !ssidSelect.value) {
    notify(t("wifi_ssid_required") || "Please select a network", "warn");
    return;
  }
  
  const ssid = ssidSelect.value;
  const password = passwordInput?.value || "";
  
  if (btnConnect) btnConnect.disabled = true;
  if (statusDiv) statusDiv.textContent = t("wifi_connecting") || "Connecting...";
  
  try {
    const res = await fetch("/api/wifi/connect", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid, password }),
    });
    
    if (!res.ok) {
      const errorText = await res.text();
      throw new Error(errorText);
    }
    
    if (statusDiv) statusDiv.textContent = t("wifi_connected") || "Connected successfully!";
    notify(t("wifi_connected") || "WiFi connected", "ok");
    
    // Wait a bit and refresh info to show new IP
    setTimeout(() => {
      refreshInfo();
    }, 2000);
  } catch (err) {
    console.error("WiFi connect error:", err);
    if (statusDiv) statusDiv.textContent = t("wifi_connect_failed") || "Connection failed: " + err.message;
    notify(t("wifi_connect_failed") || "WiFi connection failed", "error");
  } finally {
    if (btnConnect) btnConnect.disabled = false;
  }
}

async function saveNetwork(reboot = true) {
  const payload = {
    network: {
      use_dhcp: qs("dhcp").checked,
      static_ip: qs("ip").value.trim(),
      netmask: qs("mask").value.trim(),
      gateway: qs("gw").value.trim(),
      dns: qs("dns").value.trim(),
      hostname: qs("host").value.trim() || "ledbrain",
    },
  };

  try {
    await fetch("/api/save_network", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    state.config = state.config || {};
    state.config.network = payload.network;
    updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    notify(reboot ? t("toast_saved_reboot") : t("toast_saved"), "ok");
    if (reboot) {
      setTimeout(() => {
        fetch("/api/reboot", { method: "POST" });
      }, 600);
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function saveMqtt() {
  const enabled = qs("mqttEnabled").checked;
  const payload = {
    mqtt: {
      enabled,
      host: qs("mqttHost").value.trim(),
      port: parseInt(qs("mqttPort").value, 10) || 1883,
      username: qs("mqttUser").value.trim(),
      password: qs("mqttPass").value,
      ddp_target: qs("ddpTarget").value.trim(),
      ddp_port: parseInt(qs("ddpPort").value, 10) || 4048,
    },
  };

  try {
    await fetch("/api/save_mqtt", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    state.config = state.config || {};
    state.config.mqtt = {
      configured: enabled,
      ...payload.mqtt,
    };
    updateOverview();
    // Refresh device preview after info update
    renderDevicePreview();
    notify(t("toast_saved_reboot"), "ok");
    setTimeout(() => {
      fetch("/api/reboot", { method: "POST" });
    }, 600);
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function syncMqttTopics() {
  const btn = qs("btnMqttSync");
  if (btn) btn.disabled = true;
  try {
    const res = await fetch("/api/mqtt/sync", { method: "POST" });
    if (res.ok) {
      notify(t("toast_mqtt_sync_success") || "Topics synchronized", "ok");
    } else {
      const text = await res.text();
      notify(t("toast_mqtt_sync_failed") || `Sync failed: ${text}`, "error");
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_mqtt_sync_failed") || "Sync failed", "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function testMqttConnection() {
  const btn = qs("btnMqttTest");
  if (btn) btn.disabled = true;
  try {
    const res = await fetch("/api/mqtt/test", { method: "POST" });
    if (res.ok) {
      notify(t("toast_mqtt_test_success") || "Test message sent", "ok");
    } else {
      const text = await res.text();
      notify(t("toast_mqtt_test_failed") || `Test failed: ${text}`, "error");
    }
  } catch (err) {
    console.error(err);
    notify(t("toast_mqtt_test_failed") || "Test failed", "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function saveSystem() {
  const payload = {
    lang: qs("langMirror").value,
    autostart: qs("autostart").checked,
  };
  try {
    await fetch("/api/save_system", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    state.config = state.config || {};
    state.config.lang = payload.lang;
    state.config.autostart = payload.autostart;
    syncLanguageSelects(payload.lang);
    await loadLang(payload.lang);
    notify(t("toast_saved"), "ok");
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function factoryReset() {
  if (!window.confirm(t("confirm_factory_reset"))) return;
  try {
    await fetch("/api/factory_reset", { method: "POST" });
    notify(t("toast_factory_reset"), "ok");
    setTimeout(() => window.location.reload(), 1200);
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
  }
}

async function checkOta() {
  if (state.otaChecking) return;
  state.otaChecking = true;
  const applyBtn = qs("btnOtaApply");
  if (applyBtn) applyBtn.disabled = true;
  try {
    setOtaStatus(t("ota_status_checking"));
    const res = await fetch(OTA_RELEASE_URL, {
      headers: { Accept: "application/vnd.github+json" },
    });
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }
    const data = await res.json();
    const asset = Array.isArray(data.assets)
      ? data.assets.find((a) => (a.name || "").toLowerCase().endsWith(".bin"))
      : null;
    const tag = data.tag_name || data.name || "";
    const url = asset?.browser_download_url || null;
    state.latestRelease = { tag, url };
    setText("valAvailableVersion", tag || t("ota_no_version"));
    const current = (state.info?.fw_version || "").replace(/^v/i, "").trim();
    const latest = (tag || "").replace(/^v/i, "").trim();
    // Compare versions: if latest exists and is different from current, it's newer
    const isNewer = latest && (!current || latest !== current);
    const canApply = Boolean(url && isNewer);
    console.log("OTA check:", { current, latest, isNewer, hasUrl: !!url, assetName: asset?.name });
    if (applyBtn) applyBtn.disabled = !canApply;
    setOtaStatus(
      isNewer && tag
        ? t("ota_status_available").replace("{version}", tag)
        : t("ota_status_latest")
    );
  } catch (err) {
    console.error(err);
    state.latestRelease = null;
    setText("valAvailableVersion", "-");
    setOtaStatus(t("ota_status_error"));
    notify(t("ota_status_error"), "error");
  } finally {
    state.otaChecking = false;
  }
}

async function triggerOta() {
  const rel = state.latestRelease;
  if (!rel || !rel.url) {
    notify(t("toast_ota_failed") || "No update URL available", "error");
    setOtaStatus(t("ota_status_error") || "Error: No URL");
    return;
  }
  const target = (rel.tag || state.info?.fw_version || t("ota_unknown_version")).trim();
  const confirmMsg = t("confirm_ota_install").replace("{version}", target);
  if (!window.confirm(confirmMsg)) return;
  try {
    notify(t("toast_ota_start"), "ok");
    setOtaStatus(t("ota_status_installing").replace("{version}", target));
    console.log("Starting OTA update from URL:", rel.url);
    const res = await fetch("/api/ota/update", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ url: rel.url }),
    });
    if (!res.ok) {
      const errorText = await res.text();
      throw new Error(`HTTP ${res.status}: ${errorText}`);
    }
    const responseText = await res.text();
    console.log("OTA update response:", responseText);
    notify(t("toast_ota_running") || "OTA update started", "ok");
    setOtaStatus(t("ota_status_installing").replace("{version}", target) + " - " + responseText);
    // Wait a bit longer before refreshing to allow OTA to start
    setTimeout(() => {
      refreshInfo();
      // Check OTA status periodically
      let checkCount = 0;
      const statusInterval = setInterval(async () => {
        checkCount++;
        if (checkCount > 30) { // Stop after 30 checks (30 seconds)
          clearInterval(statusInterval);
          return;
        }
        try {
          await refreshInfo();
        } catch (err) {
          console.error("Error checking OTA status:", err);
        }
      }, 1000);
    }, 2000);
  } catch (err) {
    console.error("OTA update error:", err);
    notify(t("toast_ota_failed") || "OTA update failed: " + err.message, "error");
    setOtaStatus(t("ota_status_error") || "Error: " + err.message);
  }
}

async function uploadOtaFile() {
  const fileInput = qs("otaFileInput");
  if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
    notify("Please select a firmware file", "warn");
    return;
  }
  
  const file = fileInput.files[0];
  if (!file.name.toLowerCase().endsWith(".bin")) {
    notify("Please select a .bin firmware file", "warn");
    return;
  }
  
  const confirmMsg = `Install firmware from file "${file.name}"? The device will reboot after installation.`;
  if (!window.confirm(confirmMsg)) return;
  
  const formData = new FormData();
  formData.append("firmware", file);
  
  const progressBar = qs("otaUploadProgress");
  const progressFill = qs("otaProgressFill");
  const progressText = qs("otaProgressText");
  const uploadBtn = qs("btnOtaUpload");
  
  if (progressBar) progressBar.style.display = "block";
  if (uploadBtn) uploadBtn.disabled = true;
  
  try {
    setOtaStatus(`Uploading ${file.name}...`);
    notify("Starting firmware upload...", "ok");
    
    const xhr = new XMLHttpRequest();
    
    xhr.upload.addEventListener("progress", (e) => {
      if (e.lengthComputable) {
        const percent = Math.round((e.loaded / e.total) * 100);
        if (progressFill) progressFill.style.width = `${percent}%`;
        if (progressText) progressText.textContent = `${percent}% (${Math.round(e.loaded / 1024)} KB / ${Math.round(e.total / 1024)} KB)`;
      }
    });
    
    xhr.addEventListener("load", () => {
      if (xhr.status === 200) {
        notify("Firmware upload completed. Device will reboot...", "ok");
        setOtaStatus("Upload completed, rebooting...");
        setTimeout(() => {
          window.location.reload();
        }, 3000);
      } else {
        throw new Error(`HTTP ${xhr.status}: ${xhr.responseText}`);
      }
    });
    
    xhr.addEventListener("error", () => {
      throw new Error("Upload failed");
    });
    
    xhr.open("POST", "/api/ota/upload");
    xhr.send(formData);
    
  } catch (err) {
    console.error(err);
    notify(`Upload failed: ${err.message}`, "error");
    setOtaStatus("Upload failed");
    if (progressBar) progressBar.style.display = "none";
    if (uploadBtn) uploadBtn.disabled = false;
  }
}

function bindLedTabs() {
  const buttons = document.querySelectorAll("#ledSubnav button");
  const panes = document.querySelectorAll(".led-pane");
  if (!buttons.length || !panes.length) {
    return;
  }
  buttons.forEach((btn) => {
    btn.addEventListener("click", () => {
      buttons.forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      panes.forEach((pane) => {
        pane.classList.toggle("active", pane.id === btn.dataset.target);
      });
    });
  });
}

function bindNavigation() {
  console.log("bindNavigation: Starting");
  const buttons = document.querySelectorAll(".sidebar nav button");
  const views = document.querySelectorAll(".view");
  console.log("bindNavigation: Found", buttons.length, "buttons and", views.length, "views");
  
  // Debug: log all buttons
  buttons.forEach((btn, idx) => {
    console.log(`  Button ${idx}: id="${btn.id}", data-tab="${btn.dataset.tab}", text="${btn.textContent}"`);
  });
  
  // Debug: log all views
  views.forEach((view, idx) => {
    console.log(`  View ${idx}: id="${view.id}", classList="${view.classList}"`);
  });
  
  if (!buttons.length || !views.length) {
    console.error("bindNavigation: FATAL - Navigation elements not found!", { 
      buttons: buttons.length, 
      views: views.length,
      sidebar: !!document.querySelector(".sidebar"),
      nav: !!document.querySelector(".sidebar nav"),
    });
    return;
  }
  buttons.forEach((btn) => {
    console.log("bindNavigation: Adding click listener to button", btn.id);
    btn.addEventListener("click", (e) => {
      console.log("=== Navigation button clicked ===", btn.dataset.tab, btn);
      e.preventDefault();
      e.stopPropagation();
      const targetTab = btn.dataset.tab;
      if (!targetTab) {
        console.warn("Button missing data-tab attribute", btn);
        return;
      }
      // Update button states
      buttons.forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      console.log("bindNavigation: Button", btn.id, "is now active");
      // Update view visibility
      views.forEach((section) => {
        const isActive = section.id === targetTab;
        section.classList.toggle("active", isActive);
        console.log("bindNavigation: View", section.id, "isActive:", isActive, "classes:", section.classList);
      });
    });
  });
  bindLedTabs();
  console.log("bindNavigation: Complete -", buttons.length, "buttons bound");
}

function initEvents() {
  const langSwitch = qs("langSwitch");
  if (langSwitch) {
    langSwitch.addEventListener("change", (e) => {
      syncLanguageSelects(e.target.value);
      loadLang(e.target.value);
    });
  }
  const langMirror = qs("langMirror");
  if (langMirror) {
    langMirror.addEventListener("change", (e) => {
      syncLanguageSelects(e.target.value);
      loadLang(e.target.value);
    });
  }
  const dhcp = qs("dhcp");
  if (dhcp) {
    dhcp.addEventListener("change", () => {
      updateDhcpUi();
    });
  }
  const mqttEnabled = qs("mqttEnabled");
  if (mqttEnabled) {
    mqttEnabled.addEventListener("change", () => {
      updateMqttUi();
    });
  }

  const btnRefresh = qs("btnRefresh");
  if (btnRefresh) {
    let longPressTimer = null;
    let isLongPress = false;
    
    // Handle click (refresh)
    btnRefresh.addEventListener("click", debounce(() => {
      if (!isLongPress) {
        if (!state.refreshingInfo) {
          refreshInfo(true);
        }
        if (!state.loadingWled) {
          loadWledDevices(true);
        }
      }
      isLongPress = false;
    }, 500));
    
    // Handle long press (reboot) - 1 second
    btnRefresh.addEventListener("mousedown", () => {
      isLongPress = false;
      longPressTimer = setTimeout(() => {
        isLongPress = true;
        notify(t("toast_rebooting"), "warn");
        fetch("/api/reboot", { method: "POST" });
      }, 1000);
    });
    
    btnRefresh.addEventListener("mouseup", () => {
      if (longPressTimer) {
        clearTimeout(longPressTimer);
        longPressTimer = null;
      }
    });
    
    btnRefresh.addEventListener("mouseleave", () => {
      if (longPressTimer) {
        clearTimeout(longPressTimer);
        longPressTimer = null;
      }
    });
    
    // Touch events for mobile
    btnRefresh.addEventListener("touchstart", () => {
      isLongPress = false;
      longPressTimer = setTimeout(() => {
        isLongPress = true;
        notify(t("toast_rebooting"), "warn");
        fetch("/api/reboot", { method: "POST" });
      }, 1000);
    });
    
    btnRefresh.addEventListener("touchend", () => {
      if (longPressTimer) {
        clearTimeout(longPressTimer);
        longPressTimer = null;
      }
    });
  }

  const btnSaveNetwork = qs("btnSaveNetwork");
  if (btnSaveNetwork) {
    btnSaveNetwork.addEventListener("click", () => saveNetwork(true));
  }
  const btnNetworkApply = qs("btnNetworkApply");
  if (btnNetworkApply) {
    btnNetworkApply.addEventListener("click", () => saveNetwork(false));
  }
  
  // WiFi event handlers
  const btnWifiScan = qs("btnWifiScan");
  if (btnWifiScan) {
    btnWifiScan.addEventListener("click", scanWifi);
  }
  const btnWifiConnect = qs("btnWifiConnect");
  if (btnWifiConnect) {
    btnWifiConnect.addEventListener("click", connectWifi);
  }
  
  // Auto-scan WiFi when network tab is opened
  const navNetwork = qs("navNetwork");
  if (navNetwork) {
    navNetwork.addEventListener("click", () => {
      setTimeout(scanWifi, 500); // Delay to let tab switch complete
    });
  }

  const btnSaveMqtt = qs("btnSaveMqtt");
  if (btnSaveMqtt) {
    btnSaveMqtt.addEventListener("click", saveMqtt);
  }
  const btnMqttSync = qs("btnMqttSync");
  if (btnMqttSync) {
    btnMqttSync.addEventListener("click", syncMqttTopics);
  }
  const btnMqttTest = qs("btnMqttTest");
  if (btnMqttTest) {
    btnMqttTest.addEventListener("click", testMqttConnection);
  }

  const btnOpenHA = qs("btnOpenHA");
  if (btnOpenHA) {
    btnOpenHA.addEventListener("click", () =>
      window.open("http://homeassistant.local:8123", "_blank")
    );
  }
  const brightnessSlider = qs("brightnessGlobal");
  const brightnessValue = qs("brightnessValue");
  if (brightnessSlider && brightnessValue) {
    brightnessSlider.addEventListener("input", (e) => {
      const val = clamp(parseInt(e.target.value, 10) || 0, 1, MAX_BRIGHTNESS);
      // Update percentage immediately during drag
      const pct = Math.round((val / MAX_BRIGHTNESS) * 100);
      brightnessValue.textContent = `${pct}%`;
      scheduleBrightnessUpdate(val);
    });
    brightnessSlider.addEventListener("change", (e) => {
      const val = clamp(parseInt(e.target.value, 10) || 0, 1, MAX_BRIGHTNESS);
      // Update percentage on change
      const pct = Math.round((val / MAX_BRIGHTNESS) * 100);
      brightnessValue.textContent = `${pct}%`;
      scheduleBrightnessUpdate(val);
    });
  }

  // New control buttons: Toggle All, Toggle Effects, Toggle Audio
  const btnToggleAll = qs("btnToggleAll");
  if (btnToggleAll) {
    btnToggleAll.addEventListener("click", toggleAll);
  }
  
  const btnToggleEffects = qs("btnToggleEffects");
  if (btnToggleEffects) {
    btnToggleEffects.addEventListener("click", toggleEffects);
  }
  
  const btnToggleAudio = qs("btnToggleAudio");
  if (btnToggleAudio) {
    btnToggleAudio.addEventListener("click", toggleAudio);
  }

  const btnSaveSystem = qs("btnSaveSystem");
  if (btnSaveSystem) {
    btnSaveSystem.addEventListener("click", saveSystem);
  }
  const btnFactoryReset = qs("btnFactoryReset");
  if (btnFactoryReset) {
    btnFactoryReset.addEventListener("click", factoryReset);
  }
  qs("btnOtaCheck")?.addEventListener("click", checkOta);
  qs("btnOtaApply")?.addEventListener("click", triggerOta);
  qs("btnOtaSelectFile")?.addEventListener("click", () => {
    qs("otaFileInput")?.click();
  });
  qs("otaFileInput")?.addEventListener("change", (e) => {
    const file = e.target.files?.[0];
    const fileName = qs("otaFileName");
    const uploadBtn = qs("btnOtaUpload");
    if (file) {
      if (fileName) fileName.textContent = file.name;
      if (uploadBtn) uploadBtn.disabled = false;
    } else {
      if (fileName) fileName.textContent = "No file selected";
      if (uploadBtn) uploadBtn.disabled = true;
    }
  });
  qs("btnOtaUpload")?.addEventListener("click", uploadOtaFile);
  const btnRescan = qs("btnWledRescan");
  if (btnRescan) {
    btnRescan.addEventListener("click", rescanWled);
  }
  const btnAddWled = qs("btnWledAdd");
  if (btnAddWled) {
    btnAddWled.addEventListener("click", addWledManual);
  }
  
  // Device browser buttons
  const btnDeviceRescan = qs("btnDeviceRescan");
  if (btnDeviceRescan) {
    btnDeviceRescan.addEventListener("click", () => {
      loadWledDevices(true);
      refreshInfo(true);
    });
  }
  const btnDeviceAdd = qs("btnDeviceAdd");
  if (btnDeviceAdd) {
    btnDeviceAdd.addEventListener("click", () => {
      // Switch to WLED tab and focus on add form
      const wledTab = qs("tabConfigWled");
      if (wledTab) {
        wledTab.click();
        setTimeout(() => {
          const nameInput = qs("wledManualName");
          if (nameInput) nameInput.focus();
        }, 100);
      }
    });
  }
  
  // Preview toggle
  const previewToggle = qs("previewEnabled");
  if (previewToggle) {
    previewToggle.addEventListener("change", () => {
      renderDevicePreview();
    });
  }
  
  // Diagnostic bar control buttons
  // Old stop buttons removed - now using toggleAll, toggleEffects, toggleAudio

  const segBody = qs("segmentTableBody");
  if (segBody) {
    segBody.addEventListener("input", handleSegmentInput);
    segBody.addEventListener("change", (e) => {
      if (e.target.classList.contains("segment-pin")) {
        handleSegmentPinChange(e);
      } else {
        handleSegmentInput(e);
      }
    });
    segBody.addEventListener("click", handleSegmentClick);
  }
  const fxBody = qs("effectsTableBody");
  if (fxBody) {
    fxBody.addEventListener("input", handleEffectInput);
    fxBody.addEventListener("change", handleEffectInput);
    fxBody.addEventListener("click", handleEffectRowClick);
  }
  const fxPicker = qs("fxEffectPicker");
  if (fxPicker) {
    fxPicker.addEventListener("change", handleEffectPickerChange);
  }
  const btnAddSegment = qs("btnAddSegment");
  if (btnAddSegment) {
    btnAddSegment.addEventListener("click", addSegment);
  }
  const btnAddVirtual = qs("btnAddVirtual");
  if (btnAddVirtual) {
    btnAddVirtual.addEventListener("click", addVirtualSegment);
  }
  const btnSaveLed = qs("btnSaveLed");
  if (btnSaveLed) {
    btnSaveLed.addEventListener("click", saveLedConfig);
  }
  const btnSaveAudio = qs("btnSaveAudio");
  if (btnSaveAudio) {
    btnSaveAudio.addEventListener("click", saveLedConfig);
  }
  // btnSaveGlobal removed - no longer needed
  const defEngine = qs("defaultEffectEngine");
  if (defEngine) {
    defEngine.addEventListener("change", (e) => {
      const led = ensureLedEngineConfig();
      if (led) {
        led.effects.default_engine = e.target.value;
      }
    });
  }
  const fxSegmentList = qs("fxSegmentList");
  if (fxSegmentList) {
    fxSegmentList.addEventListener("click", handleFxSegmentChipClick);
  }
  [
    "fxDirection",
    "fxScatter",
    "fxFadeIn",
    "fxFadeOut",
    "fxColor1",
    "fxColor2",
    "fxColor3",
    "fxPalette",
    "fxGradient",
    "fxBrightnessOverride",
    "fxGammaColor",
    "fxGammaBrightness",
    "fxBlendMode",
    "fxLayers",
    "fxReactiveMode",
    "fxSensBass",
    "fxSensMids",
    "fxSensTreble",
    "fxAmplitude",
    "fxBrightnessCompress",
    "fxBeatResponse",
    "fxAttack",
    "fxRelease",
    "fxScenePreset",
    "fxSceneSchedule",
    "fxBeatShuffle",
  ].forEach((id) => {
    const el = qs(id);
    if (!el) return;
    const evt = el.type === "checkbox" || el.tagName === "SELECT" ? "change" : "input";
    el.addEventListener(evt, handleFxDetailChange);
  });
  [
    "audioSource",
    "audioSampleRate",
    "audioFft",
    "audioFrame",
    "audioSensitivity",
    "audioStereo",
    "snapHost",
    "snapPort",
    "snapLatency",
    "snapUdp",
  ].forEach((id) => {
    const el = qs(id);
    if (!el) return;
    const eventType = el.tagName === "SELECT" || el.type === "checkbox" ? "change" : "input";
    el.addEventListener(eventType, handleAudioFormChange);
  });
  ["ledDriver", "maxFps", "currentLimit", "psuVoltage", "psuWatts", "autoPowerLimit", "parallelOutputs", "enableDma"].forEach((id) => {
    const el = qs(id);
    if (!el) return;
    const evt = el.type === "checkbox" ? "change" : "input";
    el.addEventListener(evt, handleHardwareFieldChange);
  });
  const virtualList = qs("virtualList");
  if (virtualList) {
    virtualList.addEventListener("input", handleVirtualListInput);
    virtualList.addEventListener("change", handleVirtualListInput);
    virtualList.addEventListener("click", handleVirtualListClick);
  }
}

// Responsive scaling function
function applyResponsiveScaling() {
  const viewport = {
    width: window.innerWidth || document.documentElement.clientWidth,
    height: window.innerHeight || document.documentElement.clientHeight,
  };
  
  // Base reference: 1920x1080 (Full HD)
  const baseWidth = 1920;
  const baseHeight = 1080;
  
  // Calculate scale factor based on viewport size
  // For very small screens (< 640px), scale down more aggressively
  // For large screens (> 2560px), scale up slightly
  let scaleFactor = 1;
  
  if (viewport.width < 640) {
    // Mobile: scale based on width, minimum 0.6
    scaleFactor = Math.max(0.6, viewport.width / baseWidth * 1.2);
  } else if (viewport.width < 1024) {
    // Tablet: moderate scaling
    scaleFactor = Math.max(0.75, viewport.width / baseWidth * 1.1);
  } else if (viewport.width < 1920) {
    // Small desktop: slight scaling
    scaleFactor = Math.max(0.85, viewport.width / baseWidth);
  } else if (viewport.width > 2560) {
    // Large desktop: scale up slightly
    scaleFactor = Math.min(1.2, viewport.width / baseWidth * 0.9);
  } else {
    // Standard desktop (1920-2560): no scaling
    scaleFactor = 1;
  }
  
  // Apply scale factor to root element
  const root = document.documentElement;
  root.style.setProperty('--scale-base', scaleFactor);
  
  // Also apply transform scaling for very small screens if needed
  if (viewport.width < 480) {
    const bodyScale = Math.max(0.5, viewport.width / 480);
    document.body.style.transform = `scale(${bodyScale})`;
    document.body.style.transformOrigin = 'top left';
    document.body.style.width = `${100 / bodyScale}%`;
    document.body.style.height = `${100 / bodyScale}%`;
  } else {
    document.body.style.transform = '';
    document.body.style.transformOrigin = '';
    document.body.style.width = '';
    document.body.style.height = '';
  }
  
  console.log(`Responsive scaling applied: ${scaleFactor.toFixed(2)}x for ${viewport.width}x${viewport.height}`);
}

// Apply scaling on load and resize
window.addEventListener('resize', () => {
  clearTimeout(window.scaleTimeout);
  window.scaleTimeout = setTimeout(applyResponsiveScaling, 100);
});

// Test if script is executing before DOM is ready
console.log("Script executing, document.readyState:", document.readyState);

// Apply scaling immediately if DOM is already ready
if (document.readyState !== "loading") {
  applyResponsiveScaling();
}

// Also try immediate execution test
if (document.readyState === "loading") {
  console.log("Document still loading, waiting for DOMContentLoaded");
} else {
  console.log("Document already loaded, initializing immediately");
}

window.addEventListener("DOMContentLoaded", async () => {
  console.log("=== DOMContentLoaded: Starting initialization ===");
  
  // Apply responsive scaling immediately
  applyResponsiveScaling();
  console.log("Document ready, URL:", window.location.href);
  try {
    console.log("DOMContentLoaded: Step 1 - Rendering effect catalog");
    renderEffectCatalog("wled"); // Default to WLED
    console.log("DOMContentLoaded: Step 2 - Binding navigation");
    bindNavigation();
    console.log("DOMContentLoaded: Step 3 - Initializing events");
    initEvents();
    console.log("DOMContentLoaded: Step 4 - Loading config");
    // Load config first - it will load the correct language
    await loadConfig();
    console.log("DOMContentLoaded: Step 5 - Config loaded, loading WLED devices");
    loadWledDevices();
    console.log("DOMContentLoaded: Step 6 - Loading LED pins");
    loadLedPins();
    console.log("=== DOMContentLoaded: Initialization complete ===");
  } catch (err) {
    console.error("=== DOMContentLoaded: FATAL ERROR ===", err);
    console.error("Error stack:", err.stack);
    alert("Initialization error: " + err.message + "\n\nCheck console for details.");
  }
});
