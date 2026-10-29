import { db, withTimeout } from "@/lib/firebaseAdmin";
import { DEVICES, DeviceError, deviceState, resolveAction, validateAction } from "@/lib/devices";
import { CORS, checkKey, json } from "@/lib/apiAuth";

export const dynamic = "force-dynamic";

export function OPTIONS() {
  return new Response(null, { status: 204, headers: CORS });
}

const readHome = async () => (await withTimeout(db.ref("home").once("value"))).val() || {};

function fail(e) {
  if (e instanceof DeviceError) return json({ ok: false, error: e.message }, e.status);
  return json({ ok: false, error: (e.status === 504 ? "" : "Lỗi máy chủ: ") + e.message }, e.status || 500);
}

// GET /api/devices/:id → trạng thái một thiết bị
export async function GET(req, { params }) {
  const denied = checkKey(req);
  if (denied) return denied;
  try {
    const { id } = await params;
    if (!DEVICES[id]) throw new DeviceError(404, `Không có thiết bị "${id}".`);
    const home = await readHome();
    const d = DEVICES[id];
    return json({ ok: true, id, label: d?.label, state: deviceState(home, id) });
  } catch (e) {
    return fail(e);
  }
}

// POST /api/devices/:id   body: { "action": "on" | "off" | "toggle" | "open" | "close" | "mute" | "unmute" }
export async function POST(req, { params }) {
  const denied = checkKey(req);
  if (denied) return denied;
  try {
    const { id } = await params;
    let body = {};
    try { body = await req.json(); } catch { /* body rỗng */ }
    const action = String(body.action || "").trim();
    validateAction(id, action);
    const home = await readHome();
    const writes = resolveAction(home, id, action);
    const update = Object.fromEntries(Object.entries(writes).map(([p, v]) => ["home/" + p, v]));
    await withTimeout(db.ref().update(update));
    const after = await readHome();
    return json({ ok: true, id, action, wrote: writes, state: deviceState(after, id), home: after });
  } catch (e) {
    return fail(e);
  }
}
