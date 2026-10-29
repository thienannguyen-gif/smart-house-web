import admin from "firebase-admin";

// Khởi tạo Firebase Admin bằng khóa service account trong biến FIREBASE_SERVICE_ACCOUNT (.env).
// Khởi tạo lúc gọi lần đầu, để "npm run build" không cần khóa.
let database = null;

function init() {
  if (database) return database;
  let sa;
  try {
    sa = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT || "{}");
  } catch {
    throw new Error("FIREBASE_SERVICE_ACCOUNT trong .env không phải JSON hợp lệ (phải nằm trên một dòng).");
  }
  const databaseURL = sa.databaseURL || process.env.FIREBASE_DATABASE_URL;
  if (!sa.project_id || !databaseURL) {
    throw new Error("Thiếu FIREBASE_SERVICE_ACCOUNT hoặc FIREBASE_DATABASE_URL trong .env.");
  }
  if (!admin.apps.length) {
    admin.initializeApp({ credential: admin.credential.cert(sa), databaseURL });
  }
  database = admin.database();
  return database;
}

// Dùng như firebase-admin: db.ref("home").once("value")
export const db = { ref: (path) => init().ref(path) };

/** Chạy một lệnh Firebase, quá 10 giây thì báo lỗi thay vì treo. */
export function withTimeout(promise, ms = 10000) {
  let t;
  const timer = new Promise((_, rej) => { t = setTimeout(() => rej(Object.assign(new Error("Firebase không phản hồi sau " + ms / 1000 + " giây. Kiểm tra Internet và FIREBASE_SERVICE_ACCOUNT trong .env."), { status: 504 })), ms); });
  return Promise.race([promise, timer]).finally(() => clearTimeout(t));
}
export default admin;
