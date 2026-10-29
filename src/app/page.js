import { redirect } from "next/navigation";

// Trang chủ chuyển thẳng tới bảng điều khiển + mô hình 3D
export default function Home() {
  redirect("/nha-iot.html");
}
