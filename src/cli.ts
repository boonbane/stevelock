#!/usr/bin/env bun
import yargs from "yargs";
import { hideBin } from "yargs/helpers";
import { create } from "./index.ts";
import { resolve } from "path";
import { which } from "bun";

const argv = await yargs(hideBin(process.argv))
  .usage("$0 --dirs <path...> [--net] -- <command...>")
  .option("dirs", {
    type: "string",
    array: true,
    demandOption: true,
    describe: "Writable directories",
  })
  .option("net", {
    type: "boolean",
    default: false,
    describe: "Allow network access",
  })
  .parse();

const dirs = (argv.dirs as string[]).map((d) => resolve(d));
const args = (argv["--"] ?? argv._) as string[];

if (args.length === 0) {
  console.error("error: no command specified (put command after --)");
  process.exit(1);
}

const cmd = args[0].toString();
const cmdPath = which(cmd) ?? cmd;
const cmdArgs = args.slice(1).map(String);

const lock = create({ write: dirs, network: argv.net as boolean });
lock.spawn(cmdPath, cmdArgs);

// stream stdout/stderr to parent
const stdout = Bun.file(lock.stdoutFd()).stream();
const stderr = Bun.file(lock.stderrFd()).stream();

async function pipe(stream: ReadableStream<Uint8Array>, out: typeof Bun.stdout) {
  const reader = stream.getReader();
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      out.write(value);
    }
  } catch {
    /* fd closed */
  }
}

const p1 = pipe(stdout, Bun.stdout);
const p2 = pipe(stderr, Bun.stderr);

const code = lock.wait();
await Promise.allSettled([p1, p2]);
lock.destroy();
process.exit(code);
