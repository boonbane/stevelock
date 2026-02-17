#!/usr/bin/env bun
import fs from "fs";
import path from "path";
import { createRequire } from "module";
import { $ } from "bun";
import yargs from "yargs";
import { hideBin } from "yargs/helpers";

import { Platform } from "../src/platform";
import { CMake } from "./cmake";

const root = path.join(import.meta.dir, "..");
const cmake = () => CMake.cmake();
const require = createRequire(import.meta.url);
const cmakeJs = require.resolve("cmake-js/bin/cmake-js");

const paths = {
  root,
  cache: {
    root: path.join(root, ".cache"),
    npm: path.join(root, ".cache", "npm"),
  },
  build: {
    root: path.join(root, ".cache", "build"),
    addon: (target: Platform.Target) => path.join(root, ".cache", "build", "native", Platform.triple(target)),
    native: path.join(root, ".cache", "build", "native-static"),
    test: path.join(root, ".cache", "build", "test"),
    testAsan: path.join(root, ".cache", "build", "test-asan"),
  },
  store: {
    root: path.join(root, ".cache", "store"),
    native: {
      root: path.join(root, ".cache", "store", "native"),
      bin: path.join(root, ".cache", "store", "native", "bin"),
      lib: path.join(root, ".cache", "store", "native", "lib"),
      include: path.join(root, ".cache", "store", "native", "include"),
    },
    release: {
      dir: path.join(root, ".cache", "store", "release"),
      native: (target: Platform.Target) =>
        path.join(root, ".cache", "store", "release", `stevelock-${Platform.triple(target)}.tar.gz`),
      header: path.join(root, ".cache", "store", "release", "stevelock.h"),
    },
    tar: {
      dir: path.join(root, ".cache", "store", "stevelock"),
      js: path.join(root, ".cache", "store", "stevelock", "stevelock.tar.gz"),
      addon: (target: Platform.Target) => {
        const name = Platform.packageName(target).split("/")[1];
        return path.join(root, ".cache", "store", "@stevelock", name, `${name}.tar.gz`);
      },
    },
  },
  examples: {
    cache: {
      local: path.join(root, "example", "local", ".cache"),
      remote: path.join(root, "example", "remote", ".cache"),
    },
    nodeModules: {
      root: path.join(root, "example", "node_modules"),
      local: path.join(root, "example", "local", "node_modules"),
      remote: path.join(root, "example", "remote", "node_modules"),
    },
    lock: {
      root: path.join(root, "example", "bun.lock"),
      local: path.join(root, "example", "local", "bun.lock"),
      remote: path.join(root, "example", "remote", "bun.lock"),
    },
  },
  manifest: path.join(root, "package.json"),
  addon: {
    dir: (target: Platform.Target) => path.join(root, Platform.packageDir(target)),
    out: (target: Platform.Target) => path.join(root, Platform.packageDir(target), "stevelock.node"),
    built: (target: Platform.Target) => path.join(paths.build.addon(target), "Release", "stevelock.node"),
    manifest: (target: Platform.Target) => path.join(root, Platform.packageDir(target), "package.json"),
  },
};

const rm = (value: string) => {
  if (fs.existsSync(value)) {
    fs.rmSync(value, { recursive: true, force: true });
  }
};

const packTarball = async (cwd: string): Promise<string> => {
  fs.mkdirSync(paths.cache.npm, { recursive: true });
  const before = new Set(fs.readdirSync(paths.cache.npm).filter((entry) => entry.endsWith(".tgz")));
  await $`bun pm pack --destination ${paths.cache.npm}`.cwd(cwd);
  const after = fs.readdirSync(paths.cache.npm).filter((entry) => entry.endsWith(".tgz"));
  const made = after.filter((entry) => !before.has(entry));
  if (made.length > 0) {
    return path.join(paths.cache.npm, made[0]);
  }

  const sorted = after
    .map((entry) => ({
      entry,
      time: fs.statSync(path.join(paths.cache.npm, entry)).mtimeMs,
    }))
    .sort((a, b) => b.time - a.time);
  if (sorted.length === 0) {
    throw new Error(`failed to pack tarball in ${paths.cache.npm}`);
  }
  return path.join(paths.cache.npm, sorted[0].entry);
};

const packArchive = (cwd: string, out: string, entries: string[]) => {
  fs.mkdirSync(path.dirname(out), { recursive: true });
  rm(out);

  const result = Bun.spawnSync(["tar", "-czf", out, ...entries], {
    cwd,
    stdio: ["inherit", "inherit", "inherit"],
  });
  if (!result.success) {
    throw new Error(`tar exited with code ${result.exitCode}`);
  }
};

const copyFile = (src: string, out: string) => {
  fs.mkdirSync(path.dirname(out), { recursive: true });
  rm(out);
  fs.copyFileSync(src, out);
};

const clean = () => {
  rm(paths.build.root);
  rm(paths.store.root);
  rm(paths.cache.npm);
  rm(paths.examples.cache.local);
  rm(paths.examples.cache.remote);
  rm(paths.examples.nodeModules.root);
  rm(paths.examples.nodeModules.local);
  rm(paths.examples.nodeModules.remote);
  rm(paths.examples.lock.root);
  rm(paths.examples.lock.local);
  rm(paths.examples.lock.remote);

  for (const name of fs.readdirSync(paths.root)) {
    if (name.endsWith(".tgz")) {
      rm(path.join(paths.root, name));
    }
  }

  for (const target of Platform.supported) {
    for (const name of fs.readdirSync(paths.addon.dir(target))) {
      if (name.endsWith(".tgz") || name === "stevelock.node") {
        rm(path.join(paths.addon.dir(target), name));
      }
    }
  }
};

const buildAddon = (target: Platform.Target) => {
  const result = Bun.spawnSync(["node", cmakeJs, "compile", "--out", paths.build.addon(target)], {
    cwd: root,
    stdio: ["inherit", "inherit", "inherit"],
  });
  if (!result.success) {
    throw new Error(`cmake-js exited with code ${result.exitCode}`);
  }

  fs.mkdirSync(paths.addon.dir(target), { recursive: true });
  fs.copyFileSync(paths.addon.built(target), paths.addon.out(target));
  console.log(`built ${Platform.triple(target)} -> ${paths.addon.out(target)}`);
};

const native = {
  build: () => {
    rm(paths.store.native.root);
    fs.mkdirSync(paths.store.native.root, { recursive: true });

    cmake()
      .source(root)
      .buildDir(paths.build.native)
      .configure()
      .build("stevelock_native")
      .install(paths.store.native.root);

    console.log(`installed native artifacts -> ${paths.store.native.root}`);
  },
  test: (asan: boolean) => {
    const buildDir = asan ? paths.build.testAsan : paths.build.test;

    cmake()
      .source(root)
      .buildDir(buildDir)
      .define("STEVELOCK_TEST_ASAN", asan ? "ON" : "OFF")
      .configure()
      .build("stevelock_net_probe")
      .build("stevelock_testbox")
      .build("stevelock_process_test")
      .build("stevelock_lifecycle_test")
      .build("stevelock_sandbox_test")
      .run("stevelock_process_test")
      .run("stevelock_lifecycle_test")
      .run("stevelock_sandbox_test");
  }
}

const testAddon = (target: Platform.Target) => {
  buildAddon(target);
};

const packageAddon = async (target: Platform.Target, value?: string) => {
  const rootPkg = (await Bun.file(paths.manifest).json()) as Record<string, unknown>;
  const version = String(value ?? rootPkg.version ?? "");
  if (!version) {
    throw new Error("missing package version");
  }

  const pkg = (await Bun.file(paths.addon.manifest(target)).json()) as Record<string, unknown>;
  const next = { ...pkg, version };
  await Bun.write(paths.addon.manifest(target), `${JSON.stringify(next, null, 2)}\n`);

  if (!fs.existsSync(paths.addon.out(target))) {
    throw new Error(`missing addon binary: ${paths.addon.out(target)}`);
  }

  fs.mkdirSync(path.dirname(paths.store.tar.addon(target)), { recursive: true });
  fs.copyFileSync(await packTarball(paths.addon.dir(target)), paths.store.tar.addon(target));
  console.log(`stored addon tarball -> ${paths.store.tar.addon(target)}`);
};

const packageJs = async (value?: string) => {
  const rootPkg = (await Bun.file(paths.manifest).json()) as Record<string, unknown>;
  const version = String(value ?? rootPkg.version ?? "");
  if (!version) {
    throw new Error("missing package version");
  }

  const optional = Platform.supported
    .map((target) => Platform.packageName(target))
    .sort()
    .map((name) => [name, version]);

  const nextRoot = {
    ...rootPkg,
    version,
    optionalDependencies: Object.fromEntries(optional),
  };
  await Bun.write(paths.manifest, `${JSON.stringify(nextRoot, null, 2)}\n`);

  const rootTgz = await packTarball(paths.root);
  fs.mkdirSync(paths.store.tar.dir, { recursive: true });
  fs.copyFileSync(rootTgz, paths.store.tar.js);
  console.log(`stored wrapper tarball -> ${paths.store.tar.js}`);
};

const packageNative = () => {
  const target = Platform.detect();
  const triple = Platform.triple(target);
  const lib = path.join(paths.store.native.lib, "libstevelock.a");
  const header = path.join(paths.store.native.include, "stevelock.h");
  if (!fs.existsSync(lib)) {
    throw new Error(`missing native library: ${lib}. run \`bun run build:native\` first`);
  }
  if (!fs.existsSync(header)) {
    throw new Error(`missing header: ${header}. run \`bun run build:native\` first`);
  }

  packArchive(paths.store.native.root, paths.store.release.native(target), ["lib/libstevelock.a", "include/stevelock.h"]);
  console.log(`stored native release tarball (${triple}) -> ${paths.store.release.native(target)}`);
};

const packageHeader = () => {
  const header = path.join(paths.store.native.include, "stevelock.h");
  if (!fs.existsSync(header)) {
    throw new Error(`missing header: ${header}. run \`bun run build:native\` first`);
  }

  copyFile(header, paths.store.release.header);
  console.log(`stored header release file -> ${paths.store.release.header}`);
};

const smoke = async () => {
  const target = Platform.detect();
  buildAddon(target);
  await packageAddon(target);
  await packageJs();
  await $`bun run --cwd ./example/local local`.cwd(root);
};

const main = async () => {
  const argv = await yargs(hideBin(process.argv))
    .scriptName("release")
    .version(false)
    .command("clean", "remove build/store artifacts and local installs")
    .command("build:addon", "build host addon")
    .command("build:native", "build and install static native library")
    .command("test:native", "configure, build, and run native tests", {
      asan: { type: "boolean", default: false },
    })
    .command("test:addon", "build host addon")
    .command("package:addon", "package host addon tarball", {
      version: { type: "string" },
    })
    .command("package:js", "package js wrapper tarball", {
      version: { type: "string" },
    })
    .command("package:native", "package host native static library for GitHub release")
    .command("package:header", "package stevelock.h for GitHub release")
    .command("smoke", "build, package current version, and run local example")
    .demandCommand(1)
    .strict()
    .parse();

  const command = String(argv._[0] ?? "");
  if (command === "clean") {
    clean();
    console.log("cleaned artifacts");
    return;
  }

  if (command === "build:addon") {
    const target = Platform.detect();
    buildAddon(target);
    return;
  }

  if (command === "build:native") {
    native.build();
    return;
  }

  if (command === "test:native") {
    native.test(Boolean((argv as Record<string, unknown>).asan));
    return;
  }

  if (command === "test:addon") {
    const target = Platform.detect();
    testAddon(target);
    return;
  }

  if (command === "package:addon") {
    const target = Platform.detect();
    await packageAddon(target, (argv as Record<string, unknown>).version as string | undefined);
    return;
  }

  if (command === "package:js") {
    await packageJs((argv as Record<string, unknown>).version as string | undefined);
    return;
  }

  if (command === "package:native") {
    packageNative();
    return;
  }

  if (command === "package:header") {
    packageHeader();
    return;
  }

  if (command === "smoke") {
    await smoke();
    return;
  }

  throw new Error(`unsupported command: ${command}`);
};

await main();
