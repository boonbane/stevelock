#!/usr/bin/env bun
import fs from "fs";
import path from "path";
import yargs from "yargs";
import { hideBin } from "yargs/helpers";

import { Platform } from "../src/platform";

const root = path.join(import.meta.dir, "..");

const paths = {
  root,
  store: {
    root: path.join(root, ".cache", "store"),
    wrapper: path.join(root, ".cache", "store", "stevelock", "stevelock.tar.gz"),
    native: (target: Platform.Target) => {
      const name = Platform.packageName(target).split("/")[1];
      return path.join(root, ".cache", "store", "@stevelock", name, `${name}.tar.gz`);
    },
  },
};

const publish = (file: string, tag: string, dryRun: boolean) => {
  const args = ["publish", file, "--tag", tag, ...(dryRun ? ["--dry-run"] : [])];
  const result = Bun.spawnSync(["npm", ...args], {
    cwd: paths.root,
    stdio: ["inherit", "inherit", "inherit"],
  });
  if (!result.success) {
    throw new Error(`npm publish failed for ${file} (${result.exitCode})`);
  }
};

const main = async () => {
  const argv = await yargs(hideBin(process.argv))
    .scriptName("publish")
    .version(false)
    .option("tag", { type: "string", default: "latest" })
    .option("dryRun", { type: "boolean", default: false })
    .strict()
    .parse();

  const tag = String((argv as Record<string, unknown>).tag ?? "latest");
  const dryRun = Boolean((argv as Record<string, unknown>).dryRun);

  const files = [
    paths.store.wrapper,
    ...Platform.supported.map((target) => paths.store.native(target)),
  ].filter((file) => fs.existsSync(file));

  if (files.length === 0) {
    throw new Error(`no tarballs in ${paths.store.root}`);
  }

  for (const file of files) {
    console.log(`publishing ${file}`);
    publish(file, tag, dryRun);
  }
};

await main();
