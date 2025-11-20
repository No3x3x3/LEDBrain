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
  "Meteor",
  "Meteor Smooth",
  "Candle Multi",
  "Candle",
  "Ripple",
  "Pacifica",
  "Theater",
  "Scanner",
  "Noise",
  "Sinelon",
  "Fire 2012",
  "Fireworks",
];
const AUDIO_MODES = ["spectrum", "bass", "beat", "tempo", "energy", "vibes"];
const AUDIO_PROFILES = [
  { value: "default", label: "audio_profile_default" },
  { value: "wled_reactive", label: "audio_profile_wled_reactive" },
  { value: "wled_bass_boost", label: "audio_profile_wled_bass" },
  { value: "ledfx_energy", label: "audio_profile_ledfx_energy" },
  { value: "ledfx_tempo", label: "audio_profile_ledfx_tempo" },
];

const qs = (id) => document.getElementById(id);

function t(key) {
  return T[key] || key;
}

function notify(message, variant = "ok") {
  const bar = qs("statusBar");
  if (!bar) return;
  bar.textContent = message;
  bar.classList.remove("hidden", "ok", "error");
  if (variant) {
    bar.classList.add(variant);
  }
  clearTimeout(state.notifyTimer);
  state.notifyTimer = setTimeout(() => {
    bar.classList.add("hidden");
  }, 3200);
}

async function loadLang(lang) {
  try {
    const res = await fetch(`/lang/${lang}.json`);
    const data = await res.json();
    Object.assign(T, data);
    state.lang = lang;
    renderLang();
  } catch (err) {
    console.error("lang", err);
    notify("Failed to load translations", "error");
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
    btnRefresh: "btn_refresh",
    btnReboot: "btn_reboot",
    ledDriverTitle: "led_driver_title",
    ledDriverSubtitle: "led_driver_subtitle",
    lblDriver: "led_driver_label",
    lblMaxFps: "led_max_fps",
    lblCurrentLimit: "led_current_limit",
    lblPsuVoltage: "psu_voltage",
    lblPsuWatts: "psu_watts",
    lblAutoPowerLimit: "auto_power_limit",
    lblParallelOutputs: "led_parallel_outputs",
    lblDedicatedCore: "led_dedicated_core",
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
    lblLangSystem: "language",
    lblAutostart: "autostart",
    systemHint: "system_hint",
    btnSaveSystem: "save",
    btnFactoryReset: "factory_reset",
    btnOta: "btn_ota",
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
    btnSaveLed: "btn_save_led",
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
  updatePowerSummary();

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
  }
  renderWledDevices();
  renderEffectCatalog();
  renderLedConfig();
  renderLightsDashboard();
  updatePlaybackButton();
  const refreshBtn = qs("btnRefresh");
  if (refreshBtn) {
    refreshBtn.setAttribute("title", t("btn_refresh"));
    refreshBtn.setAttribute("aria-label", t("btn_refresh"));
  }
}

function renderEffectCatalog() {
  const list = qs("effectCatalog");
  if (!list) return;
  list.innerHTML = EFFECT_CATALOG.map((name) => `<option value="${name}"></option>`).join("");
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
  state.timers.info = setInterval(() => refreshInfo(), INFO_REFRESH_MS);
  state.timers.wled = setInterval(() => loadWledDevices(), WLED_REFRESH_MS);
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

function updatePlaybackButton() {
  const btn = qs("btnPlayback");
  if (!btn) return;
  const enabled = !!(state.ledState && state.ledState.enabled);
  const labelKey = enabled ? "btn_stop_effects" : "btn_start_effects";
  const fallback = enabled ? "Stop output" : "Start output";
  const text = T[labelKey] ? t(labelKey) : fallback;
  btn.textContent = text;
  btn.dataset.state = enabled ? "running" : "stopped";
  btn.disabled = false;
}

async function loadLedState() {
  try {
    const res = await fetch("/api/led/state");
    state.ledState = await res.json();
    updatePlaybackButton();
  } catch (err) {
    console.warn("led state", err);
  }
}

async function togglePlayback() {
  const next = !(state.ledState && state.ledState.enabled);
  const btn = qs("btnPlayback");
  if (btn) btn.disabled = true;
  try {
    await fetch("/api/led/state", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: next }),
    });
    state.ledState = state.ledState || {};
    state.ledState.enabled = next;
    if (state.info && state.info.led_engine) {
      state.info.led_engine.enabled = next;
      updateOverview();
    }
    updatePlaybackButton();
    notify(next ? t("toast_led_started") : t("toast_led_stopped"), "ok");
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
  const stored = Array.isArray(state.config?.wled_devices) ? state.config.wled_devices : [];
  stored.forEach((dev, idx) => {
    const key = dev.id || dev.address || dev.name || `stored-${idx}`;
    map.set(key, {
      id: dev.id || key,
      name: dev.name || dev.address || dev.id || key,
      address: dev.address || "",
      leds: dev.leds || 0,
      segments: dev.segments || 1,
      online: false,
      version: "",
      ip: "",
      age_ms: Number.POSITIVE_INFINITY,
      http_verified: false,
      manual: !dev.auto_discovered,
    });
  });
  const discovered = Array.isArray(state.wledDevices) ? state.wledDevices : [];
  discovered.forEach((dev, idx) => {
    const key = dev.id || dev.address || dev.ip || dev.name || `discovered-${idx}`;
    const existing = map.get(key) || {};
    map.set(key, {
      ...existing,
      id: dev.id || existing.id || key,
      name: dev.name || existing.name || key,
      address: dev.address || dev.ip || existing.address || "",
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
      ip: dev.ip || existing.ip || "",
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
        engine: fx?.engine || led.effects?.default_engine || "",
        effect: fx?.effect || "",
        audio_link: !!fx?.audio_link,
        enabled: seg.enabled !== false,
      };
    })
    .sort((a, b) => (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" }));
}

function buildVirtualSegmentsSummary() {
  const led = ensureLedEngineConfig();
  if (!led) return [];
  const segments = new Map((led.segments || []).map((seg) => [seg.id, seg]));
  return (Array.isArray(led.effects?.assignments) ? led.effects.assignments : [])
    .map((fx, idx) => {
      const segment = segments.get(fx.segment_id);
      return {
        id: fx.segment_id || `virtual-${idx}`,
        name: segment?.name || fx.segment_id || `Virtual ${idx + 1}`,
        engine: fx.engine || led.effects?.default_engine || "",
        effect: fx.effect || "",
        audio_mode: fx.audio_mode || "",
        audio_link: !!fx.audio_link,
      };
    })
    .sort((a, b) => (a.name || "").localeCompare(b.name || "", undefined, { sensitivity: "base" }));
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
}

function updateOverview() {
  const info = state.info || {};
  const cfg = state.config || {};

  const fwName = info.fw_name || info.fw || "LEDBrain";
  const fwVersion = info.fw_version ? ` ${info.fw_version}` : "";
  const fwIdf = info.idf ? ` · IDF ${info.idf}` : "";
  const fw = `${fwName}${fwVersion}${fwIdf}`.trim();
  const device = cfg.network?.hostname || "ledbrain";
  qs("chipIp").textContent = `IP: ${info.ip || "-"}`;
  qs("chipVersion").textContent = fw;
  qs("valDevice").textContent = device;
  qs("valFirmware").textContent = fw;
  qs("valOta").textContent = formatOtaState(info.ota_state, info.ota_error);
  qs("valUptime").textContent = info.uptime_s ? formatUptime(info.uptime_s) : "-";

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
function updateDhcpUi() {
  const disabled = qs("dhcp").checked;
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
    cell.colSpan = 6;
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

    [nameCell, idCell, ledsCell, segCell, statusCell, lastSeenCell].forEach((cell) => row.appendChild(cell));
    tbody.appendChild(row);
  });
}

async function refreshInfo(showToast = false) {
  try {
    const info = await (await fetch("/api/info")).json();
    state.info = info;
     state.lastInfoRefresh = Date.now();
     if (info?.led_engine && typeof info.led_engine.enabled === "boolean") {
       state.ledState.enabled = info.led_engine.enabled;
       updatePlaybackButton();
     }
    updateOverview();
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
    renderWledDevices();
    renderLightsDashboard();
    if (showToast) {
      notify(t("toast_wled_updated"), "ok");
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
      dedicate_core: true,
      enable_dma: true,
      segments: [],
      audio: defaultAudioConfig(),
      effects: { default_engine: "wled", assignments: [] },
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
      default_engine: "wled",
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
  if (typeof led.dedicate_core !== "boolean") {
    led.dedicate_core = true;
  }
  if (typeof led.enable_dma !== "boolean") {
    led.enable_dma = true;
  }
  led.effects.assignments = Array.isArray(led.effects.assignments) ? led.effects.assignments : [];
  return led;
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

function ensureEffectAssignment(segmentId) {
  const led = ensureLedEngineConfig();
  if (!led) return null;
  let assignment = led.effects.assignments.find((a) => a.segment_id === segmentId);
  if (!assignment) {
    assignment = {
      segment_id: segmentId,
      engine: led.effects.default_engine || "wled",
      effect: "Solid",
      preset: "",
      audio_link: false,
      audio_profile: "default",
      brightness: 255,
      intensity: 128,
      speed: 128,
      audio_mode: "spectrum",
      direction: "forward",
      scatter: 0,
      fade_in: 0,
      fade_out: 0,
      color1: "#ffffff",
      color2: "#ff6600",
      color3: "#0033ff",
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
  assignment.color1 = assignment.color1 || "#ffffff";
  assignment.color2 = assignment.color2 || "#ff6600";
  assignment.color3 = assignment.color3 || "#0033ff";
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
  return assignment;
}

function renderLedConfig() {
  const led = ensureLedEngineConfig();
  if (!led) return;
  renderHardwareForm(led);
  renderSegmentTable(led);
  renderEffectRows(led);
  populateAudioForm(led);
  const defEngine = qs("defaultEffectEngine");
  if (defEngine) {
    defEngine.value = led.effects.default_engine || "wled";
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
    ["dedicatedCore", led.dedicate_core ?? true],
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
    body.innerHTML = `<tr class="placeholder"><td colspan="9">${t("led_segments_empty")}</td></tr>`;
    return;
  }
  const rows = led.segments
    .map((seg, idx) => {
      const pinOptions = renderPinOptions(seg.gpio);
      const matrix = seg.matrix || {};
      return `<tr data-idx="${idx}">
        <td><input class="segment-field" data-field="name" data-idx="${idx}" value="${seg.name || ""}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="led_count" data-idx="${idx}" value="${seg.led_count ?? 0}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="start_index" data-idx="${idx}" value="${seg.start_index ?? 0}"></td>
        <td><input type="number" min="0" class="segment-field" data-field="render_order" data-idx="${idx}" value="${seg.render_order ?? idx}"></td>
        <td>
          <select class="segment-pin" data-idx="${idx}">
            ${pinOptions}
          </select>
        </td>
        <td><input type="number" min="0" max="7" class="segment-field" data-field="rmt_channel" data-idx="${idx}" value="${seg.rmt_channel ?? 0}"></td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="segment-field" data-field="matrix_enabled" data-idx="${idx}" ${seg.matrix_enabled ? "checked" : ""}>
            <span></span>
          </label>
          <div class="matrix-fields">
            <input type="number" min="0" class="segment-field" data-field="matrix.width" data-idx="${idx}" value="${matrix.width ?? 0}">
            <span>&times;</span>
            <input type="number" min="0" class="segment-field" data-field="matrix.height" data-idx="${idx}" value="${matrix.height ?? 0}">
          </div>
        </td>
        <td><input type="number" min="0" class="segment-field" data-field="power_limit_ma" data-idx="${idx}" value="${seg.power_limit_ma ?? 0}"></td>
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
            <input type="checkbox" class="segment-field" data-field="enabled" data-idx="${idx}" ${seg.enabled ? "checked" : ""}>
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
  if (!led.segments.length) {
    body.innerHTML = `<tr class="placeholder"><td colspan="16">${t("led_fx_empty")}</td></tr>`;
    return;
  }
  const rows = led.segments
    .map((seg) => {
      const assign = ensureEffectAssignment(seg.id);
      const audio = seg.audio || defaultSegmentAudio();
      return `<tr data-segid="${seg.id}">
        <td>${seg.name || seg.id}</td>
        <td>
          <select class="effect-field" data-field="engine" data-segid="${seg.id}">
            <option value="wled" ${assign.engine === "wled" ? "selected" : ""}>WLED</option>
            <option value="ledfx" ${assign.engine === "ledfx" ? "selected" : ""}>LedFx</option>
          </select>
        </td>
        <td><input class="effect-field" list="effectCatalog" data-field="effect" data-segid="${seg.id}" value="${assign.effect || ""}"></td>
        <td><input class="effect-field" data-field="preset" data-segid="${seg.id}" value="${assign.preset || ""}"></td>
        <td><input type="number" min="0" max="255" class="effect-field" data-field="brightness" data-segid="${seg.id}" value="${assign.brightness ?? 255}"></td>
        <td><input type="number" min="0" max="255" class="effect-field" data-field="intensity" data-segid="${seg.id}" value="${assign.intensity ?? 128}"></td>
        <td><input type="number" min="0" max="255" class="effect-field" data-field="speed" data-segid="${seg.id}" value="${assign.speed ?? 128}"></td>
        <td>
          <select class="effect-field" data-field="audio_mode" data-segid="${seg.id}">
            ${renderAudioModeOptions(assign.audio_mode)}
          </select>
        </td>
        <td>
          <select class="effect-field" data-field="audio_profile" data-segid="${seg.id}">
            ${renderAudioProfileOptions(assign.audio_profile)}
          </select>
        </td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="effect-field" data-field="audio_link" data-segid="${seg.id}" ${assign.audio_link ? "checked" : ""}>
            <span></span>
          </label>
        </td>
        <td>
          <label class="switch compact">
            <input type="checkbox" class="audio-field" data-field="stereo_split" data-segid="${seg.id}" ${audio.stereo_split ? "checked" : ""}>
            <span></span>
          </label>
        </td>
        <td><input type="number" step="0.05" class="audio-field" data-field="gain_left" data-segid="${seg.id}" value="${audio.gain_left ?? 1}"></td>
        <td><input type="number" step="0.05" class="audio-field" data-field="gain_right" data-segid="${seg.id}" value="${audio.gain_right ?? 1}"></td>
        <td><input type="number" step="0.05" class="audio-field" data-field="sensitivity" data-segid="${seg.id}" value="${audio.sensitivity ?? 0.85}"></td>
        <td><input type="number" min="0" max="1" step="0.01" class="audio-field" data-field="threshold" data-segid="${seg.id}" value="${audio.threshold ?? 0.05}"></td>
        <td><input type="number" min="0" max="1" step="0.01" class="audio-field" data-field="smoothing" data-segid="${seg.id}" value="${audio.smoothing ?? 0.65}"></td>
      </tr>`;
    })
    .join("");
  body.innerHTML = rows;
  const segIds = led.segments.map((s) => s.id);
  if (!state.selectedFxSegment || !segIds.includes(state.selectedFxSegment)) {
    state.selectedFxSegment = segIds[0] || null;
  }
  highlightEffectSelection();
  renderFxSegmentList();
  renderFxDetail();
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
    (dev) => `<div class="chip muted" title="${dev.address || ""}">${dev.name || dev.id} · ${dev.segments || 1} seg</div>`
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
  if (output) {
    output.textContent = `${seg.name || seg.id} • ${seg.led_count || 0} LED`;
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
    dedicatedCore: "dedicate_core",
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
  }
  if (field.startsWith("matrix.")) {
    const [, key] = field.split(".");
    seg.matrix = seg.matrix || { width: 0, height: 0, serpentine: true, vertical: false };
    let numeric = parseInt(value, 10);
    if (Number.isNaN(numeric)) numeric = 0;
    seg.matrix[key] = numeric;
    if (key === "width" || key === "height") {
      seg.matrix_enabled = seg.matrix_enabled || (seg.matrix.width > 0 && seg.matrix.height > 0);
    }
  } else {
    seg[field] = value;
  }
  if (field === "name") {
    renderEffectRows(led);
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
  handleAudioFormChange();
  try {
    await fetch("/api/led/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ led_engine: state.config.led_engine }),
    });
    notify(t("toast_led_saved"), "ok");
    refreshInfo();
  } catch (err) {
    console.error(err);
    notify(t("toast_led_failed"), "error");
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
    setTimeout(() => loadWledDevices(true), 1200);
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
      active: true,
      auto_discovered: false,
    };
    if (existingIdx >= 0) {
      const current = state.config.wled_devices[existingIdx];
      state.config.wled_devices[existingIdx] = { ...current, ...newEntry };
    } else {
      state.config.wled_devices.push(newEntry);
    }
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
    const cfg = await (await fetch("/api/get_config")).json();
    state.config = cfg;
    syncLanguageSelects(cfg.lang || "pl");
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
    await loadLang(cfg.lang || "pl");
    await refreshInfo();
    await loadLedState();
    startAutoRefreshLoops();
  } catch (err) {
    console.error(err);
    notify(t("toast_load_failed"), "error");
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
    notify(t("toast_saved_reboot"), "ok");
    setTimeout(() => {
      fetch("/api/reboot", { method: "POST" });
    }, 600);
  } catch (err) {
    console.error(err);
    notify(t("toast_save_failed"), "error");
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

async function triggerOta() {
  try {
    notify(t("toast_ota_start"), "ok");
    const res = await fetch("/api/ota/update", { method: "POST" });
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }
    notify(t("toast_ota_running"), "ok");
    setTimeout(() => refreshInfo(), 1500);
  } catch (err) {
    console.error(err);
    notify(t("toast_ota_failed"), "error");
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
  const buttons = document.querySelectorAll(".sidebar nav button");
  const views = document.querySelectorAll(".view");
  buttons.forEach((btn) => {
    btn.addEventListener("click", () => {
      buttons.forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      views.forEach((section) => {
        section.classList.toggle("active", section.id === btn.dataset.tab);
      });
    });
  });
  bindLedTabs();
}

function initEvents() {
  qs("langSwitch").addEventListener("change", (e) => {
    syncLanguageSelects(e.target.value);
    loadLang(e.target.value);
  });
  qs("langMirror").addEventListener("change", (e) => {
    syncLanguageSelects(e.target.value);
    loadLang(e.target.value);
  });
  qs("dhcp").addEventListener("change", () => {
    updateDhcpUi();
  });
  qs("mqttEnabled").addEventListener("change", () => {
    updateMqttUi();
  });

  qs("btnRefresh").addEventListener("click", () => {
    refreshInfo(true);
    loadWledDevices(true);
  });
  qs("btnReboot").addEventListener("click", () => {
    notify(t("toast_rebooting"), "warn");
    fetch("/api/reboot", { method: "POST" });
  });

  qs("btnSaveNetwork").addEventListener("click", () => saveNetwork(true));
  qs("btnNetworkApply").addEventListener("click", () => saveNetwork(false));

  qs("btnSaveMqtt").addEventListener("click", saveMqtt);
  qs("btnMqttSync").addEventListener("click", () => notify(t("toast_todo"), "warn"));
  qs("btnMqttTest").addEventListener("click", () => notify(t("toast_todo"), "warn"));

  qs("btnOpenHA").addEventListener("click", () =>
    window.open("http://homeassistant.local", "_blank")
  );

  const btnPlayback = qs("btnPlayback");
  if (btnPlayback) {
    btnPlayback.addEventListener("click", togglePlayback);
  }

  qs("btnSaveSystem").addEventListener("click", saveSystem);
  qs("btnFactoryReset").addEventListener("click", factoryReset);
  const btnOta = qs("btnOta");
  if (btnOta) {
    btnOta.addEventListener("click", triggerOta);
  }
  const btnRescan = qs("btnWledRescan");
  if (btnRescan) {
    btnRescan.addEventListener("click", rescanWled);
  }
  const btnAddWled = qs("btnWledAdd");
  if (btnAddWled) {
    btnAddWled.addEventListener("click", addWledManual);
  }

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
  const btnAddSegment = qs("btnAddSegment");
  if (btnAddSegment) {
    btnAddSegment.addEventListener("click", addSegment);
  }
  const btnSaveLed = qs("btnSaveLed");
  if (btnSaveLed) {
    btnSaveLed.addEventListener("click", saveLedConfig);
  }
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
  ["ledDriver", "maxFps", "currentLimit", "psuVoltage", "psuWatts", "autoPowerLimit", "parallelOutputs", "dedicatedCore", "enableDma"].forEach((id) => {
    const el = qs(id);
    if (!el) return;
    const evt = el.type === "checkbox" ? "change" : "input";
    el.addEventListener(evt, handleHardwareFieldChange);
  });
}

window.addEventListener("DOMContentLoaded", () => {
  renderEffectCatalog();
  bindNavigation();
  initEvents();
  loadConfig();
  loadWledDevices();
  loadLedPins();
});
