// Danh mục thiết bị của ngôi nhà và cách đọc/ghi chúng trên Firebase Realtime Database.
// Đường dẫn tính từ nhánh /home, khớp đúng với firmware trong thư mục CODE.
// File này không phụ thuộc Next.js hay Firebase nên dùng được ở cả server lẫn trình duyệt.

export const DEVICES = {
  light1: { floor: 1, kind: 'light', label: 'Đèn tầng 1', path: 'floor_1/light_room_1', invert: true, source: 'cua_chinh.ino' },
  door1:  { floor: 1, kind: 'door', label: 'Cửa chính', path: 'floor_1/door_opened_1', readOnly: 'Cửa chính chỉ mở bằng bàn phím mật khẩu tại mạch.', source: 'cua_chinh.ino' },
  garage: { floor: 1, kind: 'garage', label: 'Cửa cuốn nhà xe', path: 'floor_1/garage/status', cmd: 'floor_1/garage/command', source: 'nha_xe_moi_nhat.ino' },
  wc1:    { floor: 1, kind: 'auto', label: 'Đèn WC 1', path: 'floor_1/wc1_light', readOnly: 'Đèn WC tự bật bằng cảm biến PIR.', source: 'den_wc.ino' },
  wc2:    { floor: 1, kind: 'auto', label: 'Đèn WC 2', path: 'floor_1/wc2_light', readOnly: 'Đèn WC tự bật bằng cảm biến PIR.', source: 'den_wc.ino' },
  alarm:  { floor: 1, kind: 'alarm', label: 'Báo trộm (PIR)', path: 'floor_1/alarm_enabled_1', trig: 'floor_1/alarm_triggered_1', source: 'bao_trom_va_do_am.ino' },
  climate:{ floor: 1, kind: 'sensor', label: 'Nhiệt độ, độ ẩm (DHT11)', path: 'floor_1/temperature_1', humidity: 'floor_1/humidity_1', readOnly: 'Cảm biến chỉ đọc.', source: 'bao_trom_va_do_am.ino' },
  light2: { floor: 2, kind: 'light', label: 'Đèn tầng 2', path: 'floor_2/light_room_2', source: 'baochay_denl2.ino' },
  smoke:  { floor: 2, kind: 'smoke', label: 'Báo khói', path: 'floor_2/smoke_level_2', detected: 'floor_2/smoke_detected_2', mute: 'floor_2/buzzer_mute_2', threshold: 120, source: 'baochay_denl2.ino' },
  light3: { floor: 3, kind: 'light', label: 'Đèn tầng 3', path: 'light_room_3', source: 'van_tay.ino' },
  door3:  { floor: 3, kind: 'door', label: 'Cửa vân tay', path: 'door_status', readOnly: 'Cửa tầng 3 chỉ mở bằng vân tay tại mạch.', source: 'van_tay.ino' },
};

export const ACTIONS = {
  light: ['on', 'off', 'toggle'],
  alarm: ['on', 'off', 'toggle'],
  smoke: ['mute', 'unmute', 'toggle'],
  garage: ['open', 'close', 'toggle'],
};

export class DeviceError extends Error {
  constructor(status, message) { super(message); this.status = status; }
}

export const getPath = (obj, path) => path.split('/').reduce((a, k) => (a == null ? undefined : a[k]), obj);

/** Trạng thái đã chuẩn hóa của một thiết bị, đọc từ nhánh /home. */
export function deviceState(home, id) {
  const d = DEVICES[id];
  if (!d) throw new DeviceError(404, `Không có thiết bị "${id}".`);
  const v = p => getPath(home || {}, p);
  switch (d.kind) {
    case 'light': case 'auto': {
      const raw = v(d.path);
      return { on: raw == null ? null : (d.invert ? !raw : !!raw) };
    }
    case 'door': return { open: v(d.path) == null ? null : v(d.path) === true };
    case 'garage': return { status: v(d.path) ?? null, command: v(d.cmd) ?? null };
    case 'alarm': return { enabled: v(d.path) === true, triggered: v(d.trig) === true };
    case 'smoke': {
      const level = v(d.path);
      return { level: typeof level === 'number' ? level : null, detected: v(d.detected) === true, buzzerMuted: v(d.mute) === true, threshold: d.threshold };
    }
    case 'sensor': return { temperature: v(d.path) ?? null, humidity: v(d.humidity) ?? null };
    default: return {};
  }
}

/** Danh sách mọi thiết bị kèm trạng thái, để trả về từ GET /api/devices. */
export function listDevices(home) {
  return Object.entries(DEVICES).map(([id, d]) => ({
    id, floor: d.floor, kind: d.kind, label: d.label,
    actions: d.readOnly ? [] : (ACTIONS[d.kind] || []),
    readOnly: !!d.readOnly, state: deviceState(home, id),
  }));
}

/** Kiểm tra id và lệnh trước khi đọc Firebase. Ném DeviceError nếu không hợp lệ. */
export function validateAction(id, action) {
  const d = DEVICES[id];
  if (!d) throw new DeviceError(404, `Không có thiết bị "${id}".`);
  if (d.readOnly) throw new DeviceError(403, d.readOnly);
  const allowed = ACTIONS[d.kind] || [];
  if (!allowed.includes(action)) throw new DeviceError(400, `Lệnh "${action}" không hợp lệ cho ${d.label}. Dùng: ${allowed.join(', ')}.`);
}

/**
 * Đổi một lệnh (vd. "toggle") thành các giá trị cần ghi lên Firebase.
 * Trả về object { "<đường dẫn trong /home>": giá trị } để ghi bằng một lần update().
 */
export function resolveAction(home, id, action) {
  validateAction(id, action);
  const d = DEVICES[id];
  const st = deviceState(home, id);
  switch (d.kind) {
    case 'light': {
      const on = action === 'toggle' ? !(st.on === true) : action === 'on';
      return { [d.path]: d.invert ? !on : on };
    }
    case 'alarm': {
      const en = action === 'toggle' ? !st.enabled : action === 'on';
      return { [d.path]: en };
    }
    case 'smoke': {
      const mute = action === 'toggle' ? !st.buzzerMuted : action === 'mute';
      return { [d.mute]: mute };
    }
    case 'garage': {
      const cmd = action === 'toggle' ? (st.status === 'open' ? 'close' : 'open') : action;
      return { [d.cmd]: cmd };
    }
  }
  throw new DeviceError(400, 'Lệnh không hợp lệ.');
}
