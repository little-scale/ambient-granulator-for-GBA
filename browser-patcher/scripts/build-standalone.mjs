import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const source = await readFile(join(root, "index.html"), "utf8");
const style = await readFile(join(root, "style.css"), "utf8");
const bank = await readFile(join(root, "bank.js"), "utf8");
const app = await readFile(join(root, "app.js"), "utf8");
const inlineScript = value => value.replaceAll("</script", "<\\/script");

let output = source.replace(
  /\s*<link rel="stylesheet" href="style\.css">/,
  `\n  <style>\n${style}\n  </style>`,
);
output = output.replace(
  /\s*<script src="bank\.js"><\/script>\s*<script src="app\.js"><\/script>/,
  `\n  <script>\n${inlineScript(bank)}\n${inlineScript(app)}\n  </script>`,
);
if (/href="style\.css"|src="(?:bank|app)\.js"/.test(output))
  throw new Error("Standalone patcher still contains local runtime dependencies.");

const destination = join(root, "dist", "ambient-granulator-gba-patcher.html");
await mkdir(dirname(destination), { recursive: true });
await writeFile(destination, output);
console.log(`Built ${destination}`);
