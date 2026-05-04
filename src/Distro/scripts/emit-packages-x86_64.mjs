/**
 * gnu_pkg_lists.py icindeki cift tirnak stringlerini okur;
 * icinde boslukla ayrilmis birden fazla paket varsa hepsini ayirir.
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const py = path.join(__dirname, "../../Dotfile/gnu_pkg_lists.py");
const raw = fs.readFileSync(py, "utf8");
const lines = raw.split(/\r?\n/);

const pkgs = new Set();
const pkgRe = /^[a-z0-9@][a-z0-9@.+-]*$/;

const skipLine = (ln) => {
  const t = ln.trimStart();
  return t.startsWith("#") || t.startsWith('"""') || t.startsWith("'''");
};

for (const ln of lines) {
  if (skipLine(ln)) continue;
  const re = /"([^"]+)"/g;
  let m;
  while ((m = re.exec(ln))) {
    const inner = m[1].trim();
    if (!inner || inner.length > 200) continue;
    if (/[A-Z\u0080-\uFFFF]/.test(inner)) continue;
    if (inner.includes(":") && !inner.includes("-")) continue;
    for (const tok of inner.split(/\s+/)) {
      if (pkgRe.test(tok)) pkgs.add(tok);
    }
  }
}

const sorted = [...pkgs].sort();
const outPath = path.join(__dirname, "../profile/packages.x86_64");
const header = `# Synced from ../Dotfile/gnu_pkg_lists.py
# Guncelle: node scripts/emit-packages-x86_64.mjs
# multilib: pacman.conf icinde acik olmali (steam / lib32).

`;
fs.writeFileSync(outPath, header + sorted.join("\n") + "\n", "utf8");
console.error(`Wrote ${sorted.length} packages -> ${outPath}`);
