import { NextResponse } from "next/server";

// Cho phép trang điều khiển mở từ máy (file://) hoặc từ domain khác gọi API.
export const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, x-api-key",
  "Cache-Control": "no-store",
};

export const json = (body, status = 200) => NextResponse.json(body, { status, headers: CORS });

/**
 * Kiểm tra khóa API trong header "x-api-key".
 * Khóa đặt bằng biến môi trường SMART_HOME_API_KEY. Không đặt thì API mở cho mọi người.
 * Trả về một response lỗi nếu sai khóa, hoặc null nếu hợp lệ.
 */
export function checkKey(req) {
  const expected = process.env.SMART_HOME_API_KEY;
  if (!expected) return null;
  if (req.headers.get("x-api-key") === expected) return null;
  return json({ ok: false, error: "Sai hoặc thiếu khóa API (header x-api-key)." }, 401);
}
