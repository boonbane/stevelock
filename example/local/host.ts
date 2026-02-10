import * as stevelock from "stevelock";
import fs from "fs";
import path from "path";

const main = async () => {
  const jail = path.join(import.meta.dir, ".cache", "scratch", "jail");
  const out = path.join(jail, "output.txt");
  const escape = path.join(import.meta.dir, ".cache", "scratch", "escape.txt");
  const script = path.join(import.meta.dir, "jail.ts");
  const bun = Bun.which("bun");

  if (!bun) {
    console.error("bun not found");
    return 1;
  }

  fs.mkdirSync(jail, { recursive: true });
  if (fs.existsSync(escape)) {
    fs.rmSync(escape);
  }

  const steve = stevelock.create({ writable: [jail] });
  steve.spawn(bun, ["run", script, jail, escape]);

  const code = steve.wait();
  const outText = (await Bun.file(steve.stdoutFd()).text()).trim();
  const errText = (await Bun.file(steve.stderrFd()).text()).trim();
  const wrote = fs.existsSync(out);
  const escaped = fs.existsSync(escape);
  const blocked = !escaped;
  const errLine = errText
    ? errText.split("\n").find((line) => line.includes("EACCES") || line.includes("permission denied")) ?? "permission denied (expected)"
    : "";

  if (wrote) {
    fs.rmSync(out);
  }
  if (escaped) {
    fs.rmSync(escape);
  }

  steve.destroy();
  console.log("child exit", code);
  console.log(blocked ? "result PASS: escape write blocked" : "result FAIL: escape write allowed");

  if (outText) {
    console.log("stdout", outText);
  }

  if (blocked && errLine) {
    console.log("expected error", errLine);
  }

  if (!blocked && errText) {
    console.log("stderr", errText);
  }

  if (blocked) {
    return 0;
  }

  return 2;
};

process.exit(await main());
