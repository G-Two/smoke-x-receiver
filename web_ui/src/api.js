// Minimal fetch wrapper replacing axios. Paths are relative to the page — the
// device serves the API and UI from the same origin — matching the previous
// axios usage. Throws on network error or non-2xx status so callers' catch
// blocks fire the same way axios rejections did.
async function request(path, options) {
  const res = await fetch(path, options)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res
}

export async function getJSON(path) {
  const res = await request(path)
  return res.json()
}

export async function postJSON(path, body) {
  return request(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  })
}
