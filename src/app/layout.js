export const metadata = {
  title: "Smart House",
  description: "Nhà thông minh IoT 3 tầng: ESP8266 + Firebase",
};

export default function RootLayout({ children }) {
  return (
    <html lang="vi">
      <body style={{ margin: 0 }}>{children}</body>
    </html>
  );
}
