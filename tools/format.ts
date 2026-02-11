#!/usr/bin/env bun
import path from "path";

const main = async () => {
  const root = path.join(import.meta.dir, "..");
  const glob = new Bun.Glob("src/native/**/*.{c,h}");
  const files = [] as string[];

  for await (const file of glob.scan({ cwd: root, onlyFiles: true })) {
    files.push(path.join(root, file));
  }

  if (files.length === 0) {
    return;
  }

  const run = Bun.spawnSync(["clang-format", "-i", "--style=file", ...files], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });

  process.exitCode = run.exitCode;
};

await main();
