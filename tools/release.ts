#!/usr/bin/env bun
import fs from "fs";
import path from "path";
import os from "os";
import yargs from "yargs";
import * as yhelpers from "yargs/helpers";

const main = async () => {
  const root = path.join(import.meta.dir, "..");
  const targets = {
    "darwin-arm64-apple": {
      pkg: "@stevelock/stevelock-darwin-arm64-apple",
      dir: "packages/stevelock-darwin-arm64-apple",
    },
    "darwin-x64-apple": {
      pkg: "@stevelock/stevelock-darwin-x64-apple",
      dir: "packages/stevelock-darwin-x64-apple",
    },
    "linux-arm64-gnu": {
      pkg: "@stevelock/stevelock-linux-arm64-gnu",
      dir: "packages/stevelock-linux-arm64-gnu",
    },
    "linux-x64-gnu": {
      pkg: "@stevelock/stevelock-linux-x64-gnu",
      dir: "packages/stevelock-linux-x64-gnu",
    },
  } as const;

  type Key = keyof typeof targets;

  const plat = os.platform();
  const arch = os.arch();
  const host =
    plat === "darwin" && arch === "arm64"
      ? "darwin-arm64-apple"
      : plat === "darwin" && arch === "x64"
        ? "darwin-x64-apple"
        : plat === "linux" && arch === "arm64"
          ? "linux-arm64-gnu"
          : plat === "linux" && arch === "x64"
            ? "linux-x64-gnu"
            : "";

  const argv = await yargs(yhelpers.hideBin(process.argv))
    .scriptName("release")
    .version(false)
    .command("clean", "remove build/store artifacts and local installs")
    .command("build", "build host addon into packages/<triple>", {
      target: { type: "string", default: "auto" },
    })
    .command("package", "set versions and generate local store tarballs", {
      version: { type: "string" },
    })
    .command("publish", "publish tarballs from .cache/store", {
      tag: { type: "string", default: "latest" },
      dryRun: { type: "boolean", default: false },
    })
    .command("smoke", "build, package current version, and run local example")
    .demandCommand(1)
    .strict()
    .parse();

  const cmd = String(argv._[0] ?? "");

  if (cmd === "clean") {
    const drop = [
      path.join(root, ".cache", "build"),
      path.join(root, ".cache", "store"),
      path.join(root, "example", "local", ".cache"),
      path.join(root, "example", "remote", ".cache"),
      path.join(root, "example", "node_modules"),
      path.join(root, "example", "bun.lock"),
      path.join(root, "example", "local", "node_modules"),
      path.join(root, "example", "local", "bun.lock"),
      path.join(root, "example", "remote", "node_modules"),
      path.join(root, "example", "remote", "bun.lock"),
    ];
    for (const p of drop) {
      if (fs.existsSync(p)) {
        fs.rmSync(p, { recursive: true, force: true });
      }
    }

    for (const name of fs.readdirSync(root)) {
      if (name.endsWith(".tgz")) {
        fs.rmSync(path.join(root, name), { force: true });
      }
    }

    const keys = Object.keys(targets) as Key[];
    for (const k of keys) {
      const dir = path.join(root, targets[k].dir);
      for (const name of fs.readdirSync(dir)) {
        const full = path.join(dir, name);
        if (name.endsWith(".tgz")) {
          fs.rmSync(full, { force: true });
        }
        if (name === "stevelock.node") {
          fs.rmSync(full, { force: true });
        }
      }
    }

    console.log("cleaned artifacts");
    return;
  }

  if (cmd === "build") {
    const raw = String((argv as Record<string, unknown>).target ?? "auto");
    const key = (raw === "auto" ? host : raw) as Key | "";
    if (!key || !(key in targets)) {
      console.error(`unsupported target: ${raw}`);
      process.exitCode = 1;
      return;
    }

    const t = targets[key];
    const out = path.join(root, ".cache", "build", "native", key);
    const dst = path.join(root, t.dir, "stevelock.node");
    const src = path.join(out, "Release", "stevelock.node");

    const built = Bun.spawnSync(["cmake-js", "rebuild", "--out", out], {
      cwd: root,
      stdout: "inherit",
      stderr: "inherit",
    });
    if (built.exitCode !== 0) {
      process.exitCode = built.exitCode;
      return;
    }

    if (!fs.existsSync(src)) {
      console.error(`missing built addon: ${src}`);
      process.exitCode = 1;
      return;
    }

    fs.mkdirSync(path.dirname(dst), { recursive: true });
    fs.copyFileSync(src, dst);
    console.log(`built ${key} -> ${dst}`);

    return;
  }

  if (cmd === "package") {
    const rootPkgPath = path.join(root, "package.json");
    const rootPkg = (await Bun.file(rootPkgPath).json()) as Record<string, unknown>;
    const version = String((argv as Record<string, unknown>).version ?? rootPkg.version ?? "");
    if (!version) {
      console.error("missing package version");
      process.exitCode = 1;
      return;
    }
    const optional = Object.keys(targets)
      .map((k) => targets[k as Key].pkg)
      .sort()
      .map((name) => [name, version]);
    const nextRoot = {
      ...rootPkg,
      version,
      optionalDependencies: Object.fromEntries(optional),
    };
    await Bun.write(rootPkgPath, `${JSON.stringify(nextRoot, null, 2)}\n`);

    const keys = Object.keys(targets) as Key[];
    for (const k of keys) {
      const pkgPath = path.join(root, targets[k].dir, "package.json");
      const pkg = (await Bun.file(pkgPath).json()) as Record<string, unknown>;
      const next = { ...pkg, version };
      await Bun.write(pkgPath, `${JSON.stringify(next, null, 2)}\n`);
    }

    const tar = (cwd: string) => {
      const before = new Set(fs.readdirSync(cwd).filter((name) => name.endsWith(".tgz")));
      const packed = Bun.spawnSync(["bun", "pm", "pack"], {
        cwd,
        stdout: "inherit",
        stderr: "inherit",
      });
      if (packed.exitCode !== 0) {
        return "";
      }
      const after = fs.readdirSync(cwd).filter((name) => name.endsWith(".tgz"));
      const made = after.filter((name) => !before.has(name));
      if (made.length > 0) {
        return path.join(cwd, made[0]);
      }
      const all = after
        .map((name) => ({ name, mtime: fs.statSync(path.join(cwd, name)).mtimeMs }))
        .sort((a, b) => b.mtime - a.mtime);
      if (all.length === 0) {
        return "";
      }
      return path.join(cwd, all[0].name);
    };

    const storeRoot = path.join(root, ".cache", "store");
    const rootTgz = tar(root);
    if (!rootTgz) {
      console.error("failed to pack wrapper tarball");
      process.exitCode = 1;
      return;
    }

    const rootStore = path.join(storeRoot, "stevelock");
    fs.mkdirSync(rootStore, { recursive: true });
    const rootTar = path.join(rootStore, "stevelock.tar.gz");
    fs.copyFileSync(rootTgz, rootTar);
    console.log(`stored wrapper tarball -> ${rootTar}`);

    for (const k of keys) {
      const pkgDir = path.join(root, targets[k].dir);
      const addon = path.join(pkgDir, "stevelock.node");
      if (!fs.existsSync(addon)) {
        console.log(`skipped ${k}, no stevelock.node`);
        continue;
      }
      const tgz = tar(pkgDir);
      if (!tgz) {
        console.error(`failed to pack native tarball: ${k}`);
        process.exitCode = 1;
        return;
      }

      const parts = targets[k].pkg.split("/");
      const scope = parts[0];
      const name = parts[1];
      const dstDir = path.join(storeRoot, scope, name);
      fs.mkdirSync(dstDir, { recursive: true });
      const dstTar = path.join(dstDir, `${name}.tar.gz`);
      fs.copyFileSync(tgz, dstTar);
      console.log(`stored native tarball -> ${dstTar}`);
    }

    return;
  }

  if (cmd === "publish") {
    const tag = String((argv as Record<string, unknown>).tag ?? "latest");
    const dryRun = Boolean((argv as Record<string, unknown>).dryRun);
    const storeRoot = path.join(root, ".cache", "store");
    const native = (Object.keys(targets) as Key[])
      .map((k) => {
        const parts = targets[k].pkg.split("/");
        const scope = parts[0];
        const name = parts[1];
        return path.join(storeRoot, scope, name, `${name}.tar.gz`);
      })
      .filter((p) => fs.existsSync(p));
    const files = [path.join(storeRoot, "stevelock", "stevelock.tar.gz"), ...native].filter((p) => fs.existsSync(p));

    if (files.length === 0) {
      console.error("no tarballs in .cache/store");
      process.exitCode = 1;
      return;
    }

    for (const file of files) {
      const args = ["publish", file, "--tag", tag];
      const full = dryRun ? [...args, "--dry-run"] : args;
      const run = Bun.spawnSync(["npm", ...full], { cwd: root, stdout: "inherit", stderr: "inherit" });
      if (run.exitCode !== 0) {
        process.exitCode = run.exitCode;
        return;
      }
    }

    return;
  }

  if (cmd === "smoke") {
    const pack = Bun.spawnSync(["bun", "run", "package"], {
      cwd: root,
      stdout: "inherit",
      stderr: "inherit",
    });
    if (pack.exitCode !== 0) {
      process.exitCode = pack.exitCode;
      return;
    }

    const local = Bun.spawnSync(["bun", "run", "example:local"], {
      cwd: root,
      stdout: "inherit",
      stderr: "inherit",
    });
    process.exitCode = local.exitCode;
    return;
  }

  console.error(`unsupported command: ${cmd}`);
  process.exitCode = 1;
};

await main();
