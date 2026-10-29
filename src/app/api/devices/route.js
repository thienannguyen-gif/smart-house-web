import { db, withTimeout } from "@/lib/firebaseAdmin";
import { listDevices } from "@/lib/devices";
import { CORS, checkKey, json } from "@/lib/apiAuth";

export const dynamic = "force-dynamic";

export function OPTIONS() {
  return new Response(null, { status: 204, headers: CORS });
}

// GET /api/devices → toàn bộ nhánh /home và trạng thái từng thiết bị
export async function GET(req) {
  const denied = checkKey(req);
  if (denied) return denied;
  try {
    const snap = await withTimeout(db.ref("home").once("value"));
    const home = snap.val() || {};
    return json({ ok: true, time: Date.now(), home, devices: listDevices(home) });
  } catch (e) {
    return json({ ok: false, error: "Không đọc được Firebase: " + e.message }, e.status || 502);
  }
}
