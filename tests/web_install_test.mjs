// SPDX-License-Identifier: GPL-3.0-only
// The install page (web/install.html) without a browser or an FM-1: fm1ota.js against a simulated FM-1 (the fake
// device is SLOOP's, web/test_web.mjs, Leo Kuroshita) with ND-07's identities, and the page's inlined script parses.
//   node tests/web_install_test.mjs
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Updater, pack7, unpack7, OUR_LOADER } from "../web/fm1ota.js";

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
class FakeFM1 {
  constructor(image, { unplugAfter = Infinity, finalIdentity = "FM-1_900", identity = "FM-1_015", name = "FM-1",
                      loader = "ota-FM-1_900", outName = (n) => n, readAll = false, jump = false } = {}) {
    this.jump = jump;   // the running firmware restarts into its loader on UPGRADE, without step 1
    this.image = image; this.unplugAfter = unplugAfter; this.served = 0; this.bad = 0;
    this.readAll = readAll; this.written = new Uint8Array(image.length); this.wrote = 0;   // readAll: the loader reads it all
    this.finalIdentity = finalIdentity; this.loader = loader; this.outName = outName;
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(identity, name);
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name: this.outName(name), state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      const body = [0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)];
      this.tx([0xF0, ...pack7(body), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      if (this.jump && !this.identity.startsWith("ota-")) { this.input.state = this.output.state = "disconnected"; setTimeout(() => this.boot(this.loader, "Felucca Update"), 300); return; }
      const n = this.readAll ? Math.ceil(this.image.length / 512) : 6;
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: n }, (_, k) => [k * 512, Math.min(512, this.image.length - k * 512)]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      if (addr < 0xE0000000 && this.identity.startsWith("ota-")) { this.written.set(got, addr); this.wrote += got.length; }
      this.waiting = null;
      this.served++;
      if (this.served >= this.unplugAfter) { this.input.state = this.output.state = "disconnected"; return; }
      if (addr === 0xE0000000) setTimeout(() => this.boot(this.loader, "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.finalIdentity, "Felucca"), 300);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

const image = Uint8Array.from({ length: 0x3000 }, (_, i) => (i * 13) & 0xFF);
{
  const dev = new FakeFM1(image, { identity: "FM-1_47070", loader: "ota-FM-1_47070", finalIdentity: "FM-1_47070", name: "Felucca" });
  const steps = [];
  const got = await new Updater(dev.access).install(image, "FM-1_47070", (k) => steps.push(k));
  ok(got === "FM-1_47070" && dev.bad === 0 && steps.at(-1) === "done", "ND-07 over ND-07: running -> loader -> new build");
}
{
  const dev = new FakeFM1(image, { identity: "FM-1_015", loader: "ota-FM-1_015", finalIdentity: "FM-1_47071" });
  const got = await new Updater(dev.access).install(image, "FM-1_47071");
  ok(got === "FM-1_47071" && dev.bad === 0, "ND-07 over the official firmware");
}
{
  const dev = new FakeFM1(image, { unplugAfter: 4, readAll: true });
  dev.boot("ota-FM-1_47070", "Felucca Update");
  const done = await new Updater(dev.access).resume(image, null, { package: "FM-1_47070" });
  ok(done === false, "update mode, unplugged again: resume stops, no error");
  const dev2 = new FakeFM1(image, { readAll: true });
  dev2.boot("ota-FM-1_47070", "Felucca Update");
  ok(await new Updater(dev2.access).resume(image, null, { package: "FM-1_47070" }) === true && dev2.bad === 0, "update mode: resume finishes the write");
}
ok(OUR_LOADER({ text: "ota-FM-1_47070" }) && OUR_LOADER({ text: "ota-FM-1_47073" }) && !OUR_LOADER({ text: "ota-FM-1_4707" }), "ND-07's loader is resumed");
{   /* the page's script, inlined: it parses as a module */
  const html = readFileSync(new URL("../web/install.html", import.meta.url), "utf8");
  const js = html.slice(html.indexOf('<script type="module">') + 22, html.lastIndexOf("</script>"));
  const f = join(mkdtempSync(join(tmpdir(), "nd07-")), "page.mjs");
  writeFileSync(f, js);
  let parses = true;
  try { execFileSync(process.execPath, ["--check", f]); } catch (_) { parses = false; }
  ok(parses && js.includes("class Updater") && js.includes("function logicalImage"), "install.html: the inlined script parses");
}
console.log(failed ? `FAIL: ${failed}` : "ok: all");
process.exit(failed ? 1 : 0);
